/*
  rafzrf.c — RAFAELIA ZIPRAF/BIT TOOL (real)
  ------------------------------------------
  - HEX dump + BIT dump (segbit)
  - Scanner de assinaturas ZIP (Local/CD/EOCD/Zip64)
  - Parser de Local File Headers (com heurística p/ "stack" e truncamento)
  - CRC32 real (valida STORE; para DEFLATE valida apenas header vs descritor quando possível)
  - Menu interativo quando não há comando
  - CLI quando há comando
  - Benchmarks: CRC throughput + scan throughput

  Build:
    cc -O3 -std=c11 -Wall -Wextra -pedantic rafzrf.c -o rafzrf

  Use:
    ./rafzrf                  (menu)
    ./rafzrf info   <file>
    ./rafzrf scan   <file>
    ./rafzrf zipls  <file>
    ./rafzrf hex    <file> [off] [len]
    ./rafzrf bits   <file> [off] [len]
    ./rafzrf crc    <file>           (verifica CRC p/ entries STORE quando possível)
    ./rafzrf bench  <file> [iters]
*/

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>

/* -------------------- tempo -------------------- */
static double now_sec(void) {
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
    }
#endif
    /* fallback tosco: clock() */
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

/* ------------------ leitura -------------------- */
typedef struct {
    uint8_t *data;
    size_t   size;
} Blob;

static void die(const char *msg) {
    fprintf(stderr, "❌ %s\n", msg);
    exit(1);
}

static Blob read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "❌ Não abriu '%s': %s\n", path, strerror(errno));
        exit(1);
    }
    if (fseek(f, 0, SEEK_END) != 0) die("fseek end falhou");
    long sz = ftell(f);
    if (sz < 0) die("ftell falhou");
    if (fseek(f, 0, SEEK_SET) != 0) die("fseek set falhou");

    Blob b = {0};
    b.size = (size_t)sz;
    b.data = (uint8_t*)malloc(b.size ? b.size : 1);
    if (!b.data) die("malloc falhou");

    size_t rd = fread(b.data, 1, b.size, f);
    fclose(f);
    if (rd != b.size) die("fread incompleto");
    return b;
}

/* ------------------ endian --------------------- */
static uint16_t rd16le(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32le(const uint8_t *p) { return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24)); }
static uint64_t rd64le(const uint8_t *p) {
    uint64_t lo = rd32le(p);
    uint64_t hi = rd32le(p+4);
    return lo | (hi << 32);
}

/* ------------------ CRC32 ---------------------- */
static uint32_t crc_table[256];
static bool crc_init_done = false;

static void crc32_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) {
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        crc_table[i] = c;
    }
    crc_init_done = true;
}

static uint32_t crc32_calc(const uint8_t *buf, size_t len) {
    if (!crc_init_done) crc32_init();
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        c = crc_table[(c ^ buf[i]) & 0xFFu] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

/* ---------------- HEX / BITS ------------------- */
static void hexdump(const uint8_t *p, size_t size, size_t off, size_t len) {
    if (off > size) { printf("⚠️ offset fora do arquivo.\n"); return; }
    if (off + len > size) len = size - off;

    const size_t bytes_per_line = 16;
    for (size_t i = 0; i < len; i += bytes_per_line) {
        size_t line = (len - i > bytes_per_line) ? bytes_per_line : (len - i);
        printf("%08" PRIx64 "  ", (uint64_t)(off + i));

        for (size_t j = 0; j < bytes_per_line; j++) {
            if (j < line) printf("%02x ", p[off + i + j]);
            else printf("   ");
            if (j == 7) printf(" ");
        }

        printf(" |");
        for (size_t j = 0; j < line; j++) {
            uint8_t c = p[off + i + j];
            putchar((c >= 32 && c <= 126) ? (char)c : '.');
        }
        printf("|\n");
    }
}

static void bitdump(const uint8_t *p, size_t size, size_t off, size_t len) {
    if (off > size) { printf("⚠️ offset fora do arquivo.\n"); return; }
    if (off + len > size) len = size - off;

    for (size_t i = 0; i < len; i++) {
        uint8_t b = p[off + i];
        printf("%08" PRIx64 "  %02x  ", (uint64_t)(off + i), b);
        for (int k = 7; k >= 0; k--) {
            putchar((b & (1u << k)) ? '1' : '0');
            if (k == 4) putchar(' ');
        }
        putchar('\n');
    }
}

/* ---------------- ZIP signatures --------------- */
#define SIG_LFH   0x04034b50u /* local file header */
#define SIG_CDH   0x02014b50u /* central dir header */
#define SIG_EOCD  0x06054b50u /* end of central dir */
#define SIG_DD    0x08074b50u /* data descriptor (optional signature) */
#define SIG_Z64E  0x06064b50u /* zip64 eocd record */
#define SIG_Z64L  0x07064b50u /* zip64 eocd locator */

typedef struct {
    size_t off;
    uint32_t sig;
} SigHit;

static const char* sig_name(uint32_t s) {
    switch (s) {
        case SIG_LFH:  return "LFH (Local File Header)";
        case SIG_CDH:  return "CDH (Central Dir Header)";
        case SIG_EOCD: return "EOCD (End of Central Dir)";
        case SIG_DD:   return "DD (Data Descriptor)";
        case SIG_Z64E: return "Z64-EOCD (Zip64 Record)";
        case SIG_Z64L: return "Z64-LOC (Zip64 Locator)";
        default: return "UNKNOWN";
    }
}

static bool is_zip_sig(uint32_t s) {
    return s==SIG_LFH || s==SIG_CDH || s==SIG_EOCD || s==SIG_DD || s==SIG_Z64E || s==SIG_Z64L;
}

static SigHit* scan_sigs(const uint8_t *p, size_t size, size_t *out_n) {
    size_t cap = 128, n = 0;
    SigHit *hits = (SigHit*)malloc(cap * sizeof(SigHit));
    if (!hits) die("malloc hits falhou");

    for (size_t i = 0; i + 4 <= size; i++) {
        uint32_t s = rd32le(p + i);
        if (is_zip_sig(s)) {
            if (n == cap) {
                cap *= 2;
                SigHit *nh = (SigHit*)realloc(hits, cap * sizeof(SigHit));
                if (!nh) die("realloc hits falhou");
                hits = nh;
            }
            hits[n++] = (SigHit){ .off = i, .sig = s };
        }
    }
    *out_n = n;
    return hits;
}

/* --------- EOCD busca do fim (robusta) --------- */
static bool find_eocd(const uint8_t *p, size_t size, size_t *eocd_off) {
    /* EOCD fica no final, com comentário <= 65535. Procuramos nos últimos ~66k + 22. */
    size_t min_tail = 22;
    size_t max_back = 22 + 65535;
    if (size < min_tail) return false;

    size_t start = (size > max_back) ? (size - max_back) : 0;
    for (size_t i = size - min_tail; i + 4 > start; i--) {
        if (rd32le(p + i) == SIG_EOCD) {
            *eocd_off = i;
            return true;
        }
        if (i == 0) break;
    }
    return false;
}

/* ---------------- parse LFH -------------------- */
typedef struct {
    bool ok;
    size_t off;
    uint16_t ver_needed;
    uint16_t flags;
    uint16_t method;
    uint32_t crc32;
    uint32_t csize;
    uint32_t usize;
    const uint8_t *fname;
    uint16_t fname_len;
    const uint8_t *extra;
    uint16_t extra_len;
    size_t data_off;
    size_t data_len_guess;
    bool has_dd_flag;
} LFH;

static bool bounds(size_t off, size_t need, size_t size) {
    return (off <= size) && (need <= size - off);
}

static LFH parse_lfh(const uint8_t *p, size_t size, size_t off) {
    LFH h; memset(&h, 0, sizeof(h));
    h.off = off;

    if (!bounds(off, 30, size)) return h;
    if (rd32le(p + off) != SIG_LFH) return h;

    h.ver_needed = rd16le(p + off + 4);
    h.flags      = rd16le(p + off + 6);
    h.method     = rd16le(p + off + 8);
    h.crc32      = rd32le(p + off + 14);
    h.csize      = rd32le(p + off + 18);
    h.usize      = rd32le(p + off + 22);
    h.fname_len  = rd16le(p + off + 26);
    h.extra_len  = rd16le(p + off + 28);
    h.has_dd_flag = (h.flags & 0x0008u) != 0;

    size_t name_off = off + 30;
    size_t extra_off = name_off + h.fname_len;
    size_t data_off = extra_off + h.extra_len;

    if (!bounds(name_off, h.fname_len, size)) return h;
    if (!bounds(extra_off, h.extra_len, size)) return h;
    if (data_off > size) return h;

    h.fname = p + name_off;
    h.extra = p + extra_off;
    h.data_off = data_off;

    /* data_len_guess:
       - sem DD: csize é confiável.
       - com DD: csize pode ser 0 ou lixo; fazemos heurística: até próximo LFH/CDH/EOCD.
    */
    if (!h.has_dd_flag) {
        h.data_len_guess = (size_t)h.csize;
        if (!bounds(h.data_off, h.data_len_guess, size)) {
            /* truncado */
            h.data_len_guess = (h.data_off < size) ? (size - h.data_off) : 0;
        }
    } else {
        /* heurística: varre por próxima assinatura conhecida */
        size_t i = h.data_off;
        size_t best = size; /* fim */
        for (; i + 4 <= size; i++) {
            uint32_t s = rd32le(p + i);
            if (s == SIG_LFH || s == SIG_CDH || s == SIG_EOCD) {
                best = i;
                break;
            }
        }
        h.data_len_guess = (best > h.data_off) ? (best - h.data_off) : 0;
    }

    h.ok = true;
    return h;
}

/* -------- tentar ler Data Descriptor ----------- */
typedef struct {
    bool ok;
    size_t off;
    bool had_sig;
    uint32_t crc32;
    uint32_t csize;
    uint32_t usize;
    bool zip64_sizes;
    uint64_t csize64;
    uint64_t usize64;
} DD;

static DD try_parse_dd(const uint8_t *p, size_t size, size_t off_after_data) {
    DD d; memset(&d, 0, sizeof(d));
    d.off = off_after_data;
    if (off_after_data >= size) return d;

    /* Pode vir com ou sem assinatura */
    if (bounds(off_after_data, 16, size) && rd32le(p + off_after_data) == SIG_DD) {
        d.had_sig = true;
        d.crc32 = rd32le(p + off_after_data + 4);
        d.csize = rd32le(p + off_after_data + 8);
        d.usize = rd32le(p + off_after_data + 12);
        d.ok = true;
        return d;
    }

    /* Sem assinatura: (crc32, csize, usize) */
    if (bounds(off_after_data, 12, size)) {
        d.had_sig = false;
        d.crc32 = rd32le(p + off_after_data + 0);
        d.csize = rd32le(p + off_after_data + 4);
        d.usize = rd32le(p + off_after_data + 8);
        d.ok = true;
        return d;
    }

    return d;
}

/* -------------- impressão segura --------------- */
static void print_fname(const uint8_t *s, uint16_t n) {
    putchar('"');
    for (uint16_t i = 0; i < n; i++) {
        uint8_t c = s[i];
        if (c >= 32 && c <= 126) putchar((char)c);
        else putchar('.');
    }
    putchar('"');
}

/* ----------------- comandos ZIP ---------------- */
static void cmd_info(const char *path) {
    Blob b = read_file(path);
    printf("📦 Arquivo: %s\n", path);
    printf("📏 Tamanho: %zu bytes (%.3f MiB)\n", b.size, (double)b.size / (1024.0*1024.0));

    size_t eocd_off = 0;
    if (find_eocd(b.data, b.size, &eocd_off)) {
        printf("✅ EOCD encontrado em 0x%zx\n", eocd_off);
        if (bounds(eocd_off, 22, b.size)) {
            uint16_t disk = rd16le(b.data + eocd_off + 4);
            uint16_t cd_disk = rd16le(b.data + eocd_off + 6);
            uint16_t n_this = rd16le(b.data + eocd_off + 8);
            uint16_t n_total = rd16le(b.data + eocd_off + 10);
            uint32_t cd_size = rd32le(b.data + eocd_off + 12);
            uint32_t cd_off  = rd32le(b.data + eocd_off + 16);
            uint16_t cmt_len = rd16le(b.data + eocd_off + 20);
            printf("   disks: this=%u cd_disk=%u | entries: this=%u total=%u\n",
                   disk, cd_disk, n_this, n_total);
            printf("   central_dir: off=0x%08x size=%u | comment_len=%u\n",
                   cd_off, cd_size, cmt_len);
        }
    } else {
        printf("⚠️ EOCD não encontrado (pode ser truncado / SFX / ZIP empilhado / não-ZIP).\n");
    }

    free(b.data);
}

static void cmd_scan(const char *path) {
    Blob b = read_file(path);
    size_t n = 0;
    SigHit *hits = scan_sigs(b.data, b.size, &n);

    printf("🔎 Scan ZIP signatures: %zu hits\n", n);
    for (size_t i = 0; i < n; i++) {
        printf("  [%4zu] off=0x%08zx  sig=0x%08x  %s\n",
               i, hits[i].off, hits[i].sig, sig_name(hits[i].sig));
    }

    /* sinal de “stack”: muitos LFH em sequência */
    size_t lfh = 0;
    for (size_t i = 0; i < n; i++) if (hits[i].sig == SIG_LFH) lfh++;
    if (lfh >= 2) printf("🧬 Indício de empilhamento: %zu Local Headers no mesmo blob.\n", lfh);

    free(hits);
    free(b.data);
}

static void cmd_zipls(const char *path) {
    Blob b = read_file(path);
    size_t n = 0;
    SigHit *hits = scan_sigs(b.data, b.size, &n);

    printf("🗂️ ZIPLS (Local Headers):\n");
    size_t count = 0;
    for (size_t i = 0; i < n; i++) {
        if (hits[i].sig != SIG_LFH) continue;
        LFH h = parse_lfh(b.data, b.size, hits[i].off);
        if (!h.ok) continue;

        printf("\n  #%zu  off=0x%08zx\n", count, h.off);
        printf("     ver_needed=%u flags=0x%04x method=%u %s\n",
               h.ver_needed, h.flags, h.method, h.has_dd_flag ? "(DD=1)" : "(DD=0)");
        printf("     crc32=0x%08x csize=%u usize=%u\n", h.crc32, h.csize, h.usize);
        printf("     name_len=%u extra_len=%u data_off=0x%zx data_len_guess=%zu\n",
               h.fname_len, h.extra_len, h.data_off, h.data_len_guess);
        printf("     name="); print_fname(h.fname, h.fname_len); printf("\n");

        /* tenta Data Descriptor quando DD=1 (heurística: logo após data_guess) */
        if (h.has_dd_flag) {
            size_t dd_off = h.data_off + h.data_len_guess;
            DD d = try_parse_dd(b.data, b.size, dd_off);
            if (d.ok) {
                printf("     DD @0x%zx %s  crc=0x%08x csize=%u usize=%u\n",
                       d.off, d.had_sig ? "(sig=0x08074b50)" : "(no-sig)",
                       d.crc32, d.csize, d.usize);
            } else {
                printf("     DD: não parseável em 0x%zx (arquivo pode estar truncado/segmentado).\n", dd_off);
            }
        }

        count++;
    }
    if (count == 0) printf("  (nenhum LFH parseável)\n");

    free(hits);
    free(b.data);
}

static void cmd_hex(const char *path, size_t off, size_t len) {
    Blob b = read_file(path);
    printf("🧾 HEXDUMP off=0x%zx len=%zu\n", off, len);
    hexdump(b.data, b.size, off, len);
    free(b.data);
}

static void cmd_bits(const char *path, size_t off, size_t len) {
    Blob b = read_file(path);
    printf("🧷 BITDUMP off=0x%zx len=%zu\n", off, len);
    bitdump(b.data, b.size, off, len);
    free(b.data);
}

static void cmd_crc(const char *path) {
    Blob b = read_file(path);
    size_t n = 0;
    SigHit *hits = scan_sigs(b.data, b.size, &n);

    printf("🧪 CRC verify (real) — foco: STORE (method=0)\n");
    size_t ok=0, bad=0, skip=0;

    for (size_t i = 0; i < n; i++) {
        if (hits[i].sig != SIG_LFH) continue;
        LFH h = parse_lfh(b.data, b.size, hits[i].off);
        if (!h.ok) continue;

        printf("\n  off=0x%08zx  ", h.off);
        print_fname(h.fname, h.fname_len);

        if (h.method != 0) {
            printf("  method=%u -> (pula: não-STORE; sem inflate aqui)\n", h.method);
            skip++;
            continue;
        }

        /* Para STORE, data é “como está” (sem compressão) */
        size_t dlen = h.data_len_guess;

        /* se DD=0, e csize disponível, preferimos csize */
        if (!h.has_dd_flag && bounds(h.data_off, (size_t)h.csize, b.size)) dlen = (size_t)h.csize;

        if (!bounds(h.data_off, dlen, b.size)) {
            printf("  ⚠️ truncado: data fora do range.\n");
            skip++;
            continue;
        }

        uint32_t got = crc32_calc(b.data + h.data_off, dlen);
        uint32_t exp = h.crc32;

        /* se tiver DD, tenta pegar crc do DD como “verdade” */
        if (h.has_dd_flag) {
            size_t dd_off = h.data_off + dlen;
            DD d = try_parse_dd(b.data, b.size, dd_off);
            if (d.ok) exp = d.crc32;
        }

        if (got == exp) {
            printf("  ✅ CRC OK  (0x%08x)\n", got);
            ok++;
        } else {
            printf("  ❌ CRC FAIL got=0x%08x exp=0x%08x (len=%zu)\n", got, exp, dlen);
            bad++;
        }
    }

    printf("\n📊 Resultado: ok=%zu bad=%zu skip=%zu\n", ok, bad, skip);

    free(hits);
    free(b.data);
}

static void cmd_bench(const char *path, int iters) {
    if (iters <= 0) iters = 50;
    Blob b = read_file(path);

    printf("⏱️ BENCH: file=%s size=%zu bytes iters=%d\n", path, b.size, iters);

    /* 1) CRC throughput */
    double t0 = now_sec();
    uint32_t acc = 0;
    for (int i = 0; i < iters; i++) acc ^= crc32_calc(b.data, b.size);
    double t1 = now_sec();
    double dt = (t1 - t0);
    double mb = (double)b.size * (double)iters / (1024.0*1024.0);
    printf("  CRC32: %.3f MiB em %.6f s => %.2f MiB/s  (acc=0x%08x)\n",
           mb, dt, (dt>0? mb/dt : 0.0), acc);

    /* 2) Scan throughput */
    t0 = now_sec();
    size_t total_hits = 0;
    for (int i = 0; i < iters; i++) {
        size_t n=0;
        SigHit *h = scan_sigs(b.data, b.size, &n);
        total_hits += n;
        free(h);
    }
    t1 = now_sec();
    dt = (t1 - t0);
    mb = (double)b.size * (double)iters / (1024.0*1024.0);
    printf("  SCAN:  %.3f MiB em %.6f s => %.2f MiB/s  (total_hits=%zu)\n",
           mb, dt, (dt>0? mb/dt : 0.0), total_hits);

    free(b.data);
}

/* -------------------- menu --------------------- */
static void menu(void) {
    char path[1024];
    printf("🧭 RAFZRF — menu\n");
    printf("Arquivo alvo: ");
    if (!fgets(path, sizeof(path), stdin)) return;
    path[strcspn(path, "\r\n")] = 0;
    if (!path[0]) return;

    while (1) {
        printf("\n");
        printf("1) info      2) scan      3) zipls\n");
        printf("4) hexdump   5) bitdump   6) crc-verify\n");
        printf("7) bench     0) sair\n");
        printf("Escolha: ");

        char line[64];
        if (!fgets(line, sizeof(line), stdin)) break;
        int op = atoi(line);

        if (op == 0) break;
        if (op == 1) cmd_info(path);
        else if (op == 2) cmd_scan(path);
        else if (op == 3) cmd_zipls(path);
        else if (op == 4) {
            printf("offset (hex ou dec): ");
            if (!fgets(line, sizeof(line), stdin)) continue;
            size_t off = (size_t)strtoull(line, NULL, 0);
            printf("len: ");
            if (!fgets(line, sizeof(line), stdin)) continue;
            size_t len = (size_t)strtoull(line, NULL, 0);
            cmd_hex(path, off, len);
        } else if (op == 5) {
            printf("offset (hex ou dec): ");
            if (!fgets(line, sizeof(line), stdin)) continue;
            size_t off = (size_t)strtoull(line, NULL, 0);
            printf("len: ");
            if (!fgets(line, sizeof(line), stdin)) continue;
            size_t len = (size_t)strtoull(line, NULL, 0);
            cmd_bits(path, off, len);
        } else if (op == 6) cmd_crc(path);
        else if (op == 7) {
            printf("iters (default 50): ");
            if (!fgets(line, sizeof(line), stdin)) continue;
            int it = atoi(line);
            cmd_bench(path, it ? it : 50);
        } else {
            printf("⚠️ opção inválida.\n");
        }
    }
}

/* ------------------- main ---------------------- */
static void usage(const char *argv0) {
    printf("Uso:\n");
    printf("  %s                 (menu)\n", argv0);
    printf("  %s info   <file>\n", argv0);
    printf("  %s scan   <file>\n", argv0);
    printf("  %s zipls  <file>\n", argv0);
    printf("  %s hex    <file> [off] [len]\n", argv0);
    printf("  %s bits   <file> [off] [len]\n", argv0);
    printf("  %s crc    <file>\n", argv0);
    printf("  %s bench  <file> [iters]\n", argv0);
}

int main(int argc, char **argv) {
    if (argc == 1) { menu(); return 0; }
    if (argc < 3) { usage(argv[0]); return 2; }

    const char *cmd  = argv[1];
    const char *path = argv[2];

    if (strcmp(cmd, "info") == 0) cmd_info(path);
    else if (strcmp(cmd, "scan") == 0) cmd_scan(path);
    else if (strcmp(cmd, "zipls") == 0) cmd_zipls(path);
    else if (strcmp(cmd, "hex") == 0) {
        size_t off = (argc >= 4) ? (size_t)strtoull(argv[3], NULL, 0) : 0;
        size_t len = (argc >= 5) ? (size_t)strtoull(argv[4], NULL, 0) : 256;
        cmd_hex(path, off, len);
    } else if (strcmp(cmd, "bits") == 0) {
        size_t off = (argc >= 4) ? (size_t)strtoull(argv[3], NULL, 0) : 0;
        size_t len = (argc >= 5) ? (size_t)strtoull(argv[4], NULL, 0) : 64;
        cmd_bits(path, off, len);
    } else if (strcmp(cmd, "crc") == 0) cmd_crc(path);
    else if (strcmp(cmd, "bench") == 0) {
        int iters = (argc >= 4) ? atoi(argv[3]) : 50;
        cmd_bench(path, iters);
    } else {
        usage(argv[0]);
        return 2;
    }

    return 0;
}
