// RAFAELIA_CORE — versão "sem includes", ultra low-level, determinística e minimalista.
// Sem #include: uso direto de syscalls (Linux x86_64). Seed determinística (LCG).
// Mantém a essência fractal: Fibonacci-Rafael modificado, XOR fractal, rotações de bits,
// hyperformas e tag14. Comentários em PT-BR seguindo manifesto RAFAELIA: ψχρΔΣΩ_LOOP, Φ_ethica, Bitraf64.
// COMPILA (gcc RAFAELIA_CORE_noincludes.c -o RAFAELIA_CORE_noincludes)
// RODAR em Linux x86_64.

typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

static inline long sys_call3(long num, long a, long b, long c) {
    long ret;
    asm volatile (
        "mov %1, %%rax\n\t"
        "mov %2, %%rdi\n\t"
        "mov %3, %%rsi\n\t"
        "mov %4, %%rdx\n\t"
        "syscall\n\t"
        "mov %%rax, %0\n\t"
        : "=r"(ret)
        : "r"(num), "r"(a), "r"(b), "r"(c)
        : "rax","rdi","rsi","rdx","rcx","r11"
    );
    return ret;
}

// write wrapper: fd, buf, len
static inline long sys_write(long fd, const void *buf, ulong len) {
    return sys_call3(1, (long)fd, (long)buf, (long)len); // sys_write = 1
}

// open wrapper: filename, flags, mode
static inline long sys_open(const char *fn, long flags, long mode) {
    return sys_call3(2, (long)fn, (long)flags, (long)mode); // sys_open = 2
}

static inline long sys_close(long fd) {
    return sys_call3(3, (long)fd, 0, 0); // sys_close = 3
}

static inline void sys_exit(int code) {
    sys_call3(60, (long)code, 0, 0); // sys_exit = 60
    for(;;) { } // should not reach
}

// Simple deterministic PRNG (LCG) — evita dependências libc
static uint seed = 123456789u;
static inline uint ra_rand() {
    seed = seed * 1664525u + 1013904223u;
    return seed;
}

// integer to decimal ASCII, returns number of bytes written into buf
static int itoa_dec(unsigned int val, char *buf) {
    char tmp[16];
    int ti = 0;
    if (val == 0) {
        buf[0] = '0';
        return 1;
    }
    while (val > 0 && ti < (int)sizeof(tmp)) {
        tmp[ti++] = (char)('0' + (val % 10));
        val /= 10;
    }
    // reverse
    int n = ti;
    for (int i = 0; i < n; ++i) buf[i] = tmp[n - 1 - i];
    return n;
}

// integer to hex ASCII (lowercase), without 0x prefix. returns len
static int itoa_hex(unsigned int val, char *buf) {
    const char *hex = "0123456789abcdef";
    char tmp[16];
    int ti = 0;
    if (val == 0) {
        buf[0] = '0';
        return 1;
    }
    while (val > 0 && ti < (int)sizeof(tmp)) {
        tmp[ti++] = hex[val & 0xF];
        val >>= 4;
    }
    int n = ti;
    for (int i = 0; i < n; ++i) buf[i] = tmp[n - 1 - i];
    return n;
}

int main() {
    // RAFAELIA: preservando Amor^6·Luz^3·Consciência^3 em comentários e intenção.
    // Matriz 10x10x10 de bytes
    static uchar m[10][10][10];
    // Hyperformas
    uchar hyperformas[3] = {42, 69, 64};
    // Deterministic seed already set; evita rand() libc
    // 1) Inicializa matriz com Fibonacci-Rafael modificado
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            for (int k = 0; k < 10; ++k) {
                int n = i + j + k;
                unsigned int a = 0u, b = 1u, c;
                if (n < 2) {
                    // keep b as 1 for n>=1, else 0 for n==0
                    if (n == 0) b = 0u;
                } else {
                    for (int idx = 2; idx <= n; ++idx) {
                        c = a + b + 1u; // Rafael modification
                        a = b;
                        b = c & 0xFFu; // constrain to byte growth (simulate overflow)
                    }
                }
                // combine coords and Fibonacci modified; keep predictable lowlevel op
                unsigned int prod = (unsigned int)(i * j * k);
                m[i][j][k] = (uchar)((prod + b) & 0xFFu);
                // no verbose stdout — low-level silent operation
            }
        }
    }

    // 2) Apply hidden fractal XOR in 4 deterministic positions (using LCG)
    for (int z = 0; z < 4; ++z) {
        int x = (int)(ra_rand() % 10u);
        int y = (int)(ra_rand() % 10u);
        int zz = (int)(ra_rand() % 10u);
        m[x][y][zz] ^= 0xAAu;
    }

    // 3) Vonict rotation: rotate left each byte two times
    for (int v = 0; v < 2; ++v) {
        for (int i = 0; i < 10; ++i) {
            for (int j = 0; j < 10; ++j) {
                for (int k = 0; k < 10; ++k) {
                    uchar val = m[i][j][k];
                    m[i][j][k] = (uchar)(((val << 1) | (val >> 7)) & 0xFFu);
                }
            }
        }
    }

    // 4) Prepare output file via sys_open (flags numerical to avoid includes)
    const char fname[] = "fractalMatrix.txt";
    // flags: O_WRONLY(1) | O_CREAT(64) | O_TRUNC(512) = 577
    long fd = sys_open(fname, 577, 0644);
    if (fd < 0) {
        // if open failed, exit with code 1
        sys_exit(1);
    }

    // 5) Write matrix lines to file: "<m>:<hval>:<taghex> " with newlines as in original
    char buf[256];
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            for (int k = 0; k < 10; ++k) {
                int p = 0;
                // m value decimal
                p += itoa_dec((unsigned int)m[i][j][k], buf + p);
                buf[p++] = ':';
                // hyperforma cycle
                uchar hval = hyperformas[(i + j + k) % 3];
                p += itoa_dec((unsigned int)hval, buf + p);
                buf[p++] = ':';
                // tag14 calculation then write as hex (like std::hex)
                unsigned int tag = (unsigned int)((i * 3 + j * 5 + k * 7) % 16384);
                p += itoa_hex(tag, buf + p);
                // space separator
                buf[p++] = ' ';
                // write this small chunk
                sys_write((long)fd, buf, (ulong)p);
            }
            // newline at end of j inner loop
            const char nl = '\n';
            sys_write((long)fd, &nl, 1);
        }
        const char nl2 = '\n';
        sys_write((long)fd, &nl2, 1);
    }

    sys_close((long)fd);

    // graceful exit
    sys_exit(0);
    return 0;
}