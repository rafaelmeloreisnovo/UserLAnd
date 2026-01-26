#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <limits.h>

#define MAX_WIDTH_BITS 512
#define MAX_LANES (MAX_WIDTH_BITS/64)
#define CHUNK_SIZE 32768
#define MASK64 UINT64_C(0xFFFFFFFFFFFFFFFF)
#define OFFSET_BASIS UINT64_C(0xCBF29CE484222325)
#define PRIME UINT64_C(0x00000100000001B3)

/* deterministic left rotate without relying on compiler builtins */
static uint64_t rotl64(uint64_t v, unsigned r) {
    r &= 63;
    return (v << r) | (v >> (64 - r));
}

/* guarded read to avoid partial chunks on slow storage */
static size_t safe_read(FILE *fp, unsigned char *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        size_t got = fread(buf + total, 1, len - total, fp);
        total += got;
        if (got == 0) {
            if (feof(fp)) {
                break;
            }
            if (ferror(fp)) {
                clearerr(fp);
                break;
            }
        }
    }
    return total;
}

/* low-level FNV+rotate lane update */
static uint64_t lane_update(uint64_t state, const unsigned char *data, size_t len) {
    size_t full = len / 8;
    size_t tail = len - (full * 8);
    size_t i;
    for (i = 0; i < full; ++i) {
        uint64_t word =
            ((uint64_t)data[i * 8 + 0]) |
            ((uint64_t)data[i * 8 + 1] << 8) |
            ((uint64_t)data[i * 8 + 2] << 16) |
            ((uint64_t)data[i * 8 + 3] << 24) |
            ((uint64_t)data[i * 8 + 4] << 32) |
            ((uint64_t)data[i * 8 + 5] << 40) |
            ((uint64_t)data[i * 8 + 6] << 48) |
            ((uint64_t)data[i * 8 + 7] << 56);
        state ^= word;
        state = (state * PRIME) & MASK64;
        state = rotl64(state, 31);
        state ^= (state >> 33);
    }
    for (i = 0; i < tail; ++i) {
        state ^= data[full * 8 + i];
        state = (state * PRIME) & MASK64;
    }
    return state;
}

/* emit hex without sprintf to avoid locale surprises */
static void hex64(uint64_t v, char *out) {
    static const char *hex = "0123456789abcdef";
    int i;
    for (i = 0; i < 16; ++i) {
        unsigned shift = (15 - i) * 4;
        out[i] = hex[(v >> shift) & 0xF];
    }
}

/* simple timer using clock_gettime for monotonic measurement */
static double now_seconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1e9);
}

static int parse_uint_arg(const char *value, unsigned *out) {
    if (value == NULL || out == NULL) {
        return 0;
    }
    errno = 0;
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0') {
        return 0;
    }
    if (parsed > UINT_MAX) {
        return 0;
    }
    *out = (unsigned)parsed;
    return 1;
}

int main(int argc, char **argv) {
    const char *path = NULL;
    unsigned width_bits = 256;
    unsigned lanes;
    unsigned i;
    FILE *fp;
    unsigned char buf[CHUNK_SIZE];
    uint64_t states[MAX_LANES];
    double start, end;
    unsigned long long total_bytes = 0ULL;

    /* minimal argument parsing to stay POSIX-compatible */
    for (i = 1; i < (unsigned)argc; ++i) {
        if (strcmp(argv[i], "-w") == 0 && (i + 1) < (unsigned)argc) {
            if (!parse_uint_arg(argv[i + 1], &width_bits)) {
                fprintf(stderr, "error: invalid width '%s'\n", argv[i + 1]);
                return 1;
            }
            ++i;
        } else {
            path = argv[i];
        }
    }

    if (path == NULL) {
        fprintf(stderr, "usage: %s [-w WIDTH_BITS] <file>\n", argv[0]);
        return 1;
    }

    if (width_bits < 64) {
        width_bits = 64;
    }
    if (width_bits > MAX_WIDTH_BITS) {
        width_bits = MAX_WIDTH_BITS;
    }
    lanes = width_bits / 64;
    for (i = 0; i < lanes; ++i) {
        states[i] = (OFFSET_BASIS ^ (UINT64_C(0x9E3779B97F4A7C15) * (uint64_t)i)) & MASK64;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "error: unable to open %s: %s\n", path, strerror(errno));
        return 1;
    }

    start = now_seconds();
    for (;;) {
        size_t got = safe_read(fp, buf, sizeof(buf));
        if (got == 0) {
            break;
        }
        total_bytes += (unsigned long long)got;
        for (i = 0; i < lanes; ++i) {
            states[i] = lane_update(states[i], buf, got);
        }
        if (got < sizeof(buf)) {
            break;
        }
    }
    end = now_seconds();
    fclose(fp);

    /* structured JSON-like output for downstream parsers */
    printf("{\n");
    printf("  \"engine\": \"AETHER_C_FNV\",\n");
    printf("  \"width_bits\": %u,\n", width_bits);
    printf("  \"bytes\": %llu,\n", (unsigned long long)total_bytes);
    printf("  \"seconds\": %.9f,\n", (end >= start) ? (end - start) : 0.0);
    printf("  \"hex\": \"");
    for (i = 0; i < lanes; ++i) {
        char tmp[16];
        hex64(states[i], tmp);
        fwrite(tmp, 1, 16, stdout);
    }
    printf("\"\n}");
    printf("\n");

    return 0;
}
