/*
 * raf_dev_omni_perf.c
 * RAFAELIA • Omni-Architecture Mode • Universal Polyglot C
 *
 * Compliance: ISO/IEC 9899:2011 (C11) strict.
 * Standards: IEEE 754 (Float), ISO 25010 (Portability), NIST 800-53 (SA-8).
 * Targets: x64, ARM64, RISC-V, MIPS, PowerPC, SPARC, WASM, AVR.
 */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #include <stdalign.h> /* C11: alignas, alignof */
    #include <stdatomic.h>
#else
    /* Fallback para C99/ANSI */
    #define alignas(x)
    #define _Atomic
#endif

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>  /* Standard IO fallback */
#include <time.h>

/* ------------------------------- */
/* 1. Camada de Abstração de Hardware (HAL) */
/* ------------------------------- */

/* Detecção de Endianness (Crucial para RISC vs CISC) */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define RAF_BIG_ENDIAN 1
#else
    #define RAF_LITTLE_ENDIAN 1
#endif

/* Otimização de Branch (Compilador Agnostic) */
#if defined(__GNUC__) || defined(__clang__)
    #define LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define RAF_RESTRICT __restrict__
    #define RAF_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
    #define RAF_RESTRICT __restrict
    #define RAF_INLINE __forceinline
#else
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
    #define RAF_RESTRICT
    #define RAF_INLINE inline
#endif

/* Abstração de I/O de Baixo Nível */
#if defined(_WIN32)
    #include <io.h>
    #define raf_read  _read
    #define raf_write _write
    #define RAF_FD_IN  0
    #define RAF_FD_OUT 1
#elif defined(__unix__) || defined(__APPLE__) || defined(__linux__)
    #include <unistd.h>
    #define raf_read  read
    #define raf_write write
    #define RAF_FD_IN  STDIN_FILENO
    #define RAF_FD_OUT STDOUT_FILENO
#else
    /* Bare Metal / WASM / Unknown - Fallback para stdio */
    #define RAF_USE_STDIO_FALLBACK 1
#endif

/* ------------------------------- */
/* 2. Definições de Memória */
/* ------------------------------- */
#define RAF_ROWS 64
#define RAF_COLS 8
#define RAF_BUF_SIZE 16384 /* 16KB universal page fit */

/* Alinhamento Adaptativo:
   AVX-512 (x64) precisa de 64.
   NEON (ARM) prefere 16 ou 64.
   RISC-V Vector prefere alinhamento à largura do vetor (VLEN).
   Cache lines geralmente são 64 bytes.
*/
#define RAF_ALIGNMENT 64

typedef struct {
    alignas(RAF_ALIGNMENT) float    matrix[RAF_ROWS * RAF_COLS];
    alignas(8)             uint64_t epoch;
    uint32_t rows_count;
    uint32_t severity;
    /* Scales for optimization */
    float ranges_lo[RAF_COLS];
    float ranges_scale[RAF_COLS];
} RafCore;

/* Buffers na BSS (Zero Overhead Startup) */
alignas(RAF_ALIGNMENT) static uint8_t g_in_buf[RAF_BUF_SIZE];
alignas(RAF_ALIGNMENT) static uint8_t g_out_buf[RAF_BUF_SIZE];
static size_t g_out_pos = 0;

/* ------------------------------- */
/* 3. Lógica Atemporal (Algoritmos) */
/* ------------------------------- */

/* Parser de Float (mínimo, rápido): [-]ddd[.ddd]
   - Robustez: garante avanço do ponteiro em lixo para não travar.
*/
static RAF_INLINE const char* fast_parse_float(const char* RAF_RESTRICT p, float* RAF_RESTRICT out) {
    const char* start = p;

    float res  = 0.0f;
    float fact = 1.0f;
    bool sign  = false;

    /* Skip whitespace */
    while (*p <= ' ' && *p != 0) p++;
    if (*p == 0) { *out = 0.0f; return p; }

    /* Optional '+' or '-' */
    if (*p == '-') { sign = true; p++; }
    else if (*p == '+') { p++; }

    /* Integer part */
    bool any = false;
    while (*p >= '0' && *p <= '9') {
        any = true;
        res = res * 10.0f + (float)(*p++ - '0');
    }

    /* Fractional part */
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') {
            any = true;
            fact *= 0.1f;
            res += (float)(*p++ - '0') * fact;
        }
    }

    /* Se não consumiu nada numérico, avança 1 byte (anti-loop em lixo) */
    if (!any) {
        *out = 0.0f;
        if (*p != 0) return p + 1;
        return start; /* fim */
    }

    *out = sign ? -res : res;
    return p;
}

/* Emissor de Float (Universal) - Evita sprintf
   - Sem UB por INT_MIN (usa uint64 para parte inteira)
   - 6 casas com arredondamento simples e clamp
*/
static RAF_INLINE void fast_emit_float(float v) {
    if (g_out_pos + 32 > RAF_BUF_SIZE) return;

    /* NaN -> 0 */
    if (v != v) v = 0.0f;

    uint8_t* b = &g_out_buf[g_out_pos];
    int n = 0;

    /* sinal */
    if (v < 0.0f) { b[n++] = '-'; v = -v; }

    /* parte inteira em 64-bit (evita UB) */
    uint64_t ip = (uint64_t)v;

    if (ip == 0) {
        b[n++] = '0';
    } else {
        char buf[24];
        int k = 0;
        while (ip > 0) {
            buf[k++] = (char)('0' + (char)(ip % 10));
            ip /= 10;
        }
        while (k > 0) b[n++] = (uint8_t)buf[--k];
    }

    b[n++] = '.';

    /* fração */
    float frac = v - (float)((uint64_t)v);
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 0.9999999f) frac = 0.9999999f;

    uint32_t fv = (uint32_t)(frac * 1000000.0f + 0.5f);

    uint32_t div = 100000;
    for (int i = 0; i < 6; ++i) {
        b[n++] = (uint8_t)('0' + (char)((fv / div) % 10));
        div /= 10;
    }

    g_out_pos += (size_t)n;
}

/* Core Processing: Otimizado para Pipeline Superescalar (CISC & RISC) */
static void raf_process_batch(RafCore* core) {
    const uint32_t total = core->rows_count * (uint32_t)RAF_COLS;
    float* RAF_RESTRICT data       = core->matrix;
    const float* RAF_RESTRICT lo   = core->ranges_lo;
    const float* RAF_RESTRICT sc   = core->ranges_scale;

    for (uint32_t i = 0; i < total; i += (uint32_t)RAF_COLS) {
        for (int c = 0; c < RAF_COLS; ++c) {
            float val  = data[i + (uint32_t)c];
            float norm = (val - lo[c]) * sc[c];

            /* clamp 0..1 */
            if (norm < 0.0f) norm = 0.0f;
            if (norm > 1.0f) norm = 1.0f;

            data[i + (uint32_t)c] = norm;
        }
    }
}

/* ------------------------------- */
/* 4. I/O System (Poly-Interface) */
/* ------------------------------- */

static void raf_io_flush(void) {
    if (g_out_pos == 0) return;

#if defined(RAF_USE_STDIO_FALLBACK)
    (void)fwrite(g_out_buf, 1, g_out_pos, stdout);
#else
    /* write completo (evita perda em writes parciais) */
    size_t off = 0;
    while (off < g_out_pos) {
        /* raf_write é int em POSIX/Windows; evita size_t direto */
        int w = (int)raf_write(RAF_FD_OUT, (const void*)(g_out_buf + off),
                               (unsigned)(g_out_pos - off));
        if (w <= 0) break;
        off += (size_t)w;
    }
#endif

    g_out_pos = 0;
}

static RAF_INLINE void raf_io_put(char c) {
    if (UNLIKELY(g_out_pos >= RAF_BUF_SIZE)) raf_io_flush();
    g_out_buf[g_out_pos++] = (uint8_t)c;
}

static void raf_io_puts(const char* s) {
    while (*s) raf_io_put(*s++);
}

static void raf_io_put_u32(uint32_t v) {
    char tmp[16];
    int k = 0;
    if (v == 0) tmp[k++] = '0';
    else {
        while (v) { tmp[k++] = (char)('0' + (char)(v % 10)); v /= 10; }
    }
    while (k) raf_io_put(tmp[--k]);
}

static void raf_io_put_u64(uint64_t v) {
    char tmp[32];
    int k = 0;
    if (v == 0) tmp[k++] = '0';
    else {
        while (v) { tmp[k++] = (char)('0' + (char)(v % 10)); v /= 10; }
    }
    while (k) raf_io_put(tmp[--k]);
}

/* ------------------------------- */
/* Main Entry Point */
/* ------------------------------- */
int main(void) {
    /* garante alinhamento do core na stack (C11) */
    alignas(RAF_ALIGNMENT) RafCore core;

    memset(&core, 0, sizeof(core));
    core.epoch = (uint64_t)time(NULL);

    /* Setup Ranges */
    const float hi_defs[RAF_COLS] = {100.f, 8192.f, 2000.f, 1000.f, 100.f, 110.f, 120.f, 10.f};
    for (int i = 0; i < RAF_COLS; ++i) {
        core.ranges_lo[i] = 0.0f;
        core.ranges_scale[i] = (hi_defs[i] > 1e-5f) ? (1.0f / hi_defs[i]) : 0.0f;
    }

    /* Leitura (Ingestion) */
    size_t bytes_read = 0;
#if defined(RAF_USE_STDIO_FALLBACK)
    bytes_read = fread(g_in_buf, 1, RAF_BUF_SIZE, stdin);
#else
    {
        int r = (int)raf_read(RAF_FD_IN, g_in_buf, (unsigned)RAF_BUF_SIZE);
        bytes_read = (r > 0) ? (size_t)r : 0;
    }
#endif

    if (bytes_read > 0) {
        const char* p   = (const char*)g_in_buf;
        const char* end = p + bytes_read;

        while (p < end && core.rows_count < (uint32_t)RAF_ROWS) {
            /* Skip comments / empty lines */
            if (*p == '#' || *p == '\n' || *p == '\r') {
                while (p < end && *p != '\n') p++;
                if (p < end) p++;
                continue;
            }

            float* r = &core.matrix[core.rows_count * (uint32_t)RAF_COLS];

            /* Unroll manual de 8 colunas */
            p = fast_parse_float(p, &r[0]);
            p = fast_parse_float(p, &r[1]);
            p = fast_parse_float(p, &r[2]);
            p = fast_parse_float(p, &r[3]);
            p = fast_parse_float(p, &r[4]);
            p = fast_parse_float(p, &r[5]);
            p = fast_parse_float(p, &r[6]);
            p = fast_parse_float(p, &r[7]);

            core.rows_count++;

            /* Next line */
            while (p < end && *p != '\n') p++;
            if (p < end) p++;
        }
    }

    /* Processamento */
    raf_process_batch(&core);

    /* Energia e Severidade (Lógica Heurística) */
    float energy = 0.0f;
    uint32_t n = core.rows_count * (uint32_t)RAF_COLS;
    for (uint32_t i = 0; i < n; ++i) energy += core.matrix[i] * core.matrix[i];

    if (energy > 384.0f) core.severity = 2;
    else if (energy > 96.0f) core.severity = 1;
    else core.severity = 0;

    /* Saída JSON (Serialização Manual Zero-Copy) */
    raf_io_puts("{\"tag\":\"raf_omni\",\"arch\":");

#if defined(__x86_64__) || defined(_M_X64)
    raf_io_puts("\"x64\"");
#elif defined(__aarch64__) || defined(_M_ARM64)
    raf_io_puts("\"arm64\"");
#elif defined(__riscv)
    raf_io_puts("\"riscv\"");
#elif defined(__wasm__)
    raf_io_puts("\"wasm\"");
#else
    raf_io_puts("\"generic\"");
#endif

    raf_io_puts(",\"epoch\":");
    raf_io_put_u64(core.epoch);

    raf_io_puts(",\"rows\":");
    raf_io_put_u32(core.rows_count);

    raf_io_puts(",\"severity\":");
    raf_io_put_u32(core.severity);

    raf_io_puts(",\"energy\":");
    fast_emit_float(energy);

    raf_io_puts(",\"data\":[");
    for (uint32_t r = 0; r < core.rows_count; ++r) {
        if (r > 0) raf_io_put(',');
        raf_io_put('[');

        float* row = &core.matrix[r * (uint32_t)RAF_COLS];
        for (int c = 0; c < RAF_COLS; ++c) {
            if (c > 0) raf_io_put(',');
            fast_emit_float(row[c]);
        }

        raf_io_put(']');
    }
    raf_io_puts("]}\n");

    raf_io_flush();
    return 0;
}
