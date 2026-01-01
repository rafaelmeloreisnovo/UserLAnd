#include <stdint.h>
#include <stddef.h>
#include <unistd.h>   // write(), read(), close()
#include <fcntl.h>    // open()
#include <sys/stat.h> // mode_t

#define MAGIC_RAF   0x52414641u
#define DIM_OCT     8
#define DEPTH       10000
#define FLAG_TAG14  (1u << 4)
#define UNIVERSE_FILE "universe.zipraf"

// --------- I/O LOW LEVEL ---------
static void print_str(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    if (len > 0) {
        (void) write(1, s, len);
    }
}

static void print_u64(uint64_t v) {
    char buf[32];
    int i = 0;

    if (v == 0) {
        (void) write(1, "0", 1);
        return;
    }

    while (v > 0 && i < (int)sizeof(buf)) {
        uint64_t digit = v % 10u;
        buf[i++] = (char)('0' + (char)digit);
        v /= 10u;
    }
    while (i > 0) {
        i--;
        (void) write(1, &buf[i], 1);
    }
}

static void print_int(int v) {
    if (v < 0) {
        (void) write(1, "-", 1);
        v = -v;
    }
    print_u64((uint64_t)v);
}

// --------- ESTRUTURAS ZIPRAF ---------
typedef struct {
    uint32_t magic;
    uint32_t crc_payload;
    uint32_t crc_header;
    uint64_t timestamp;
    uint32_t flags;
} ZipHeader;

typedef struct {
    ZipHeader header;
    float     state[DIM_OCT];
    int32_t   hook_next;
    int32_t   ref_sheet;
    int32_t   hint_path;
    uint64_t  solution_cache;
} HyperNode;

static HyperNode universe[DEPTH];

// --------- CRC32 IMPLEMENTADO ---------
static uint32_t crc32_raw(const void *data, uint32_t len) {
    const uint8_t *p = (const uint8_t *) data;
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t i, k;

    for (i = 0; i < len; i++) {
        crc ^= (uint32_t)p[i];
        for (k = 0; k < 8; k++) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

// --------- CARREGAR UNIVERSO DO DISCO ---------
static int load_universe(void) {
    int fd = open(UNIVERSE_FILE, O_RDONLY);
    if (fd < 0) {
        print_str("[ERRO] Não foi possível abrir arquivo ");
        print_str(UNIVERSE_FILE);
        print_str("\n");
        return 0;
    }

    uint8_t *p = (uint8_t *)universe;
    uint32_t total = (uint32_t)sizeof(universe);
    uint32_t read_bytes = 0;

    while (read_bytes < total) {
        ssize_t r = read(fd, p + read_bytes, (size_t)(total - read_bytes));
        if (r < 0) {
            print_str("[ERRO] Falha ao ler universo do disco.\n");
            (void) close(fd);
            return 0;
        }
        if (r == 0) {
            break;
        }
        read_bytes += (uint32_t)r;
    }

    (void) close(fd);

    if (read_bytes != total) {
        print_str("[ERRO] Tamanho lido != tamanho esperado.\n");
        print_str("Lido: ");
        print_u64((uint64_t)read_bytes);
        print_str(", Esperado: ");
        print_u64((uint64_t)total);
        print_str("\n");
        return 0;
    }

    print_str("[OK] Universo carregado de ");
    print_str(UNIVERSE_FILE);
    print_str(" (bytes: ");
    print_u64((uint64_t)read_bytes);
    print_str(")\n");
    return 1;
}

// --------- NAVEGAÇÃO LINEAR COM HINT ---------
static void solve_linear(void) {
    int32_t idx = 0;
    uint64_t total_resolution = 0;
    int steps = 0;

    print_str("\n[PROCESS] Iniciando Resolução Linear (FROM ZIPRAF)...\n");

    while (idx >= 0 && idx < DEPTH) {
        HyperNode *n = &universe[idx];

        // validação do selo
        ZipHeader tmp = n->header;
        tmp.crc_header = 0;
        {
            uint32_t check = crc32_raw((const void *)&tmp,
                                       (uint32_t)sizeof(ZipHeader));
            if (check != n->header.crc_header) {
                print_str("[ALERTA] Selo quebrado em idx=");
                print_int(idx);
                print_str(". Encerrando navegação.\n");
                break;
            }
        }

        total_resolution += n->solution_cache;

        if (n->hint_path >= 0 && n->hint_path < DEPTH) {
            idx = n->hint_path;
        } else {
            idx = n->hook_next;
        }

        steps++;
    }

    print_str("\n⚡ RESULTADO FINAL (FROM ZIPRAF)\n");
    print_str("   Passos Lineares : ");
    print_int(steps);
    print_str("\n   Solução (Soma)  : ");
    print_u64(total_resolution);
    print_str("\n");
}

int main(void) {
    print_str("=== RAFAELIA RUN (ZIPRAF READER) ===\n");
    print_str("Carregando universo pré-resolvido...\n");
    if (!load_universe()) {
        print_str("Falha ao carregar universo.\n");
        return 1;
    }
    solve_linear();
    return 0;
}
