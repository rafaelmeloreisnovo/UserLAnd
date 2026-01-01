#include <stdint.h>
#include <stddef.h>
#include <unistd.h>   // write()
#include <fcntl.h>    // open()
#include <sys/stat.h> // mode_t

#define MAGIC_RAF   0x52414641u   // "RAFA"
#define DIM_OCT     8
#define DEPTH       10000
#define FLAG_TAG14  (1u << 4)
#define PHI         1.61803398f
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

// --------- GÊNESE DO UNIVERSO ---------
static void genesis_web(void) {
    int i, d;

    for (i = 0; i < DEPTH; i++) {
        HyperNode *n = &universe[i];

        n->header.magic = MAGIC_RAF;
        n->header.flags = FLAG_TAG14;
        n->header.timestamp = (uint64_t)i; // tempo simbólico

        for (d = 0; d < DIM_OCT; d++) {
            n->state[d] = (float)(i * d) * PHI;
        }

        n->solution_cache = (uint64_t)((uint64_t)i * 963u * 42u);

        if (i < (DEPTH - 1)) {
            n->hook_next = i + 1;
            n->ref_sheet = i + 1;
        } else {
            n->hook_next = -1;
            n->ref_sheet = -1;
        }

        n->hint_path = -1;

        n->header.crc_payload = crc32_raw((const void *)n->state,
                                          (uint32_t)sizeof(n->state));

        ZipHeader tmp = n->header;
        tmp.crc_header = 0;
        n->header.crc_header = crc32_raw((const void *)&tmp,
                                         (uint32_t)sizeof(ZipHeader));
    }

    // Um HINT simbólico: nó 50 salta para 144
    if (DEPTH > 144) {
        universe[50].hint_path = 144;
    }
}

// --------- SALVAR EM DISCO ---------
static void save_universe(void) {
    int fd = open(UNIVERSE_FILE, O_WRONLY | O_CREAT | O_TRUNC, (mode_t)0644);
    if (fd < 0) {
        print_str("[ERRO] Não foi possível criar arquivo ");
        print_str(UNIVERSE_FILE);
        print_str("\n");
        return;
    }

    const uint8_t *p = (const uint8_t *)universe;
    uint32_t total = (uint32_t)sizeof(universe);
    uint32_t written = 0;

    while (written < total) {
        ssize_t w = write(fd, p + written, (size_t)(total - written));
        if (w <= 0) {
            print_str("[ERRO] Falha ao escrever universo em disco.\n");
            break;
        }
        written += (uint32_t)w;
    }

    (void) close(fd);

    print_str("[OK] Universo salvo em ");
    print_str(UNIVERSE_FILE);
    print_str(" (bytes: ");
    print_u64((uint64_t)written);
    print_str(")\n");
}

int main(void) {
    print_str("=== RAFAELIA BUILD (ZIPRAF GÊNESE) ===\n");
    print_str("Gerando universo em memória...\n");
    genesis_web();
    print_str("Salvando universo em arquivo binário...\n");
    save_universe();
    print_str("Concluído.\n");
    return 0;
}
