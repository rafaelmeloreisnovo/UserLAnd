#define _POSIX_C_SOURCE 199309L

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * RAF BITRAF STORE (vector -> vector -> verb)
 * -------------------------------------------
 * Pure C, no external dependencies.
 *
 * - Ingests a raw byte stream (e.g. grayscale pixels) and normalizes it into a
 *   1024-slot base-20 vector (10x10x10 core plus padding) to keep the state
 *   space bounded.
 * - Applies a verb-driven vector->vector transform to keep the pipeline
 *   programmable without higher-level runtimes.
 * - Computes four fractal fingerprints and two parity rails to make it easy to
 *   audit the vector on-device.
 * - Persists the result in a tiny ZipRafStore record so a databaseraf can be
 *   populated offline without extra tooling.
 *
 * The CLI accepts an optional path to a raw file (default: synthetic gradient):
 *   ./raf_bitraf_store input.raw --verb shift
 */

#define BITRAF_STATES 1024
#define BITRAF_BASE 20
#define BITRAF_FRACTALS 4
#define BITRAF_DIM 10
#define BITRAF_VOLUME (BITRAF_DIM * BITRAF_DIM * BITRAF_DIM)
#define ZIPRAF_MAGIC 0x5A525354u /* 'ZRST' -> ZipRafSTore */

typedef struct {
    uint16_t state[BITRAF_STATES];
} bitraf_vector;

typedef struct {
    uint16_t cell[BITRAF_DIM][BITRAF_DIM][BITRAF_DIM];
} bitraf_matrix;

typedef enum {
    VERB_PASS = 0,
    VERB_SHIFT,
    VERB_FRACTALIZE,
    VERB_BITWISE
} bitraf_verb;

typedef struct {
    uint32_t magic;
    uint32_t record_id;
    uint32_t packed_len;
    uint32_t parity_even;
    uint32_t parity_odd;
    uint32_t fractal[BITRAF_FRACTALS];
} zipraf_header;

static uint32_t now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return 0;
    }
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

static void load_source(const char *path, uint8_t *buf, size_t buf_len, size_t *written) {
    *written = 0;
    if (path == NULL) {
        for (size_t i = 0; i < buf_len; ++i) {
            buf[i] = (uint8_t)(i % 256); /* synthetic gradient */
        }
        *written = buf_len;
        return;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Could not open %s, using synthetic data.\n", path);
        for (size_t i = 0; i < buf_len; ++i) {
            buf[i] = (uint8_t)((i * 3) % 256);
        }
        *written = buf_len;
        return;
    }

    *written = fread(buf, 1, buf_len, f);
    fclose(f);
    if (*written == 0) {
        fprintf(stderr, "No bytes read from %s, keeping vector zeroed.\n", path);
    }
}

static void normalize_to_bitraf(const uint8_t *src, size_t src_len, bitraf_vector *out) {
    for (size_t i = 0; i < BITRAF_STATES; ++i) {
        uint8_t sample = (i < src_len) ? src[i] : (uint8_t)(i & 0xFF);
        out->state[i] = (uint16_t)(sample % BITRAF_BASE);
    }
}

static bitraf_verb parse_verb(const char *verb) {
    if (verb == NULL) {
        return VERB_PASS;
    }
    if (strcmp(verb, "shift") == 0) {
        return VERB_SHIFT;
    }
    if (strcmp(verb, "fractalize") == 0) {
        return VERB_FRACTALIZE;
    }
    if (strcmp(verb, "bitwise") == 0) {
        return VERB_BITWISE;
    }
    return VERB_PASS;
}

static void vector_to_matrix(const bitraf_vector *src, bitraf_matrix *dst, uint16_t *padding_fold) {
    memset(dst->cell, 0, sizeof(dst->cell));
    uint16_t fold = 0;

    for (size_t i = 0; i < BITRAF_STATES; ++i) {
        uint16_t value = (uint16_t)(src->state[i] % BITRAF_BASE);
        if (i < BITRAF_VOLUME) {
            size_t xy = i / BITRAF_DIM;
            size_t z = i % BITRAF_DIM;
            size_t x = xy / BITRAF_DIM;
            size_t y = xy % BITRAF_DIM;
            dst->cell[x][y][z] = value;
        } else {
            /* Fold the padding tail into a deterministic seed without UB shifts. */
            uint16_t offset = (uint16_t)((i - BITRAF_VOLUME) % 15u);
            uint16_t rotated = (uint16_t)((value << offset) | (value >> (15u - offset)));
            fold ^= rotated;
            fold = (uint16_t)((fold << 1) | (fold >> 15));
        }
    }

    if (padding_fold) {
        *padding_fold = fold;
    }
}

static void bitwise_kernel(const bitraf_vector *src, bitraf_vector *dst) {
    bitraf_matrix m;
    uint16_t padding_seed = 0;
    vector_to_matrix(src, &m, &padding_seed);

    size_t idx = 0;
    uint16_t parity_even = 0, parity_odd = 0;
    for (size_t x = 0; x < BITRAF_DIM; ++x) {
        for (size_t y = 0; y < BITRAF_DIM; ++y) {
            for (size_t z = 0; z < BITRAF_DIM; ++z) {
                uint16_t base = m.cell[x][y][z];
                uint16_t nx = m.cell[(x + 1) % BITRAF_DIM][(y + 1) % BITRAF_DIM][z];
                uint16_t ny = m.cell[x][(y + 2) % BITRAF_DIM][(z + 3) % BITRAF_DIM];
                uint16_t parity_mask = (uint16_t)(((x << 3) ^ (y << 1) ^ z) & 0x1Fu);
                uint16_t mixed = (uint16_t)((base ^ nx ^ ny ^ padding_seed ^ parity_mask) % BITRAF_BASE);
                mixed = (uint16_t)((mixed + (uint16_t)((x + y + z) % BITRAF_BASE)) % BITRAF_BASE);

                dst->state[idx] = mixed;
                if ((idx & 1u) == 0) {
                    parity_even ^= mixed;
                } else {
                    parity_odd ^= mixed;
                }
                ++idx;
            }
        }
    }

    /* Recycle parity lanes for the remaining 24 states. */
    while (idx < BITRAF_STATES) {
        uint16_t spill = (uint16_t)((padding_seed ^ parity_even ^ parity_odd ^ idx) % BITRAF_BASE);
        dst->state[idx++] = spill;
    }
}

static void apply_verb(bitraf_vector *dst, const bitraf_vector *src, bitraf_verb verb) {
    switch (verb) {
    case VERB_SHIFT:
        for (size_t i = 0; i < BITRAF_STATES; ++i) {
            uint16_t a = src->state[i];
            uint16_t b = src->state[(i + 1) % BITRAF_STATES];
            dst->state[i] = (uint16_t)((a + b + 1u) % BITRAF_BASE);
        }
        break;
    case VERB_FRACTALIZE:
        for (size_t i = 0; i < BITRAF_STATES; ++i) {
            uint16_t acc = src->state[i];
            acc = (uint16_t)((acc + src->state[(i * 3) % BITRAF_STATES]) % BITRAF_BASE);
            acc = (uint16_t)((acc + src->state[(i * 7 + 13) % BITRAF_STATES]) % BITRAF_BASE);
            acc = (uint16_t)((acc + src->state[(i * 11 + 31) % BITRAF_STATES]) % BITRAF_BASE);
            dst->state[i] = acc;
        }
        break;
    case VERB_BITWISE:
        bitwise_kernel(src, dst);
        break;
    case VERB_PASS:
    default:
        memcpy(dst->state, src->state, sizeof(dst->state));
        break;
    }
}

static void compute_parities(const bitraf_vector *v, uint32_t *even, uint32_t *odd) {
    uint32_t p_even = 0, p_odd = 0;
    for (size_t i = 0; i < BITRAF_STATES; ++i) {
        if ((i & 1u) == 0) {
            p_even ^= v->state[i];
        } else {
            p_odd ^= v->state[i];
        }
    }
    *even = p_even;
    *odd = p_odd;
}

static void compute_fractals(const bitraf_vector *v, uint32_t out[BITRAF_FRACTALS]) {
    const size_t seeds[BITRAF_FRACTALS] = {1, 3, 9, 27};
    for (size_t f = 0; f < BITRAF_FRACTALS; ++f) {
        uint32_t acc = 0;
        size_t stride = seeds[f];
        for (size_t i = 0; i < BITRAF_STATES; i += stride) {
            acc = (acc * 1315423911u) ^ (uint32_t)v->state[i];
        }
        out[f] = acc;
    }
}

static size_t pack_base20(const bitraf_vector *v, uint8_t *out, size_t out_cap) {
    size_t out_pos = 0;
    uint8_t pending = 0;
    int pending_bits = 0;

    for (size_t i = 0; i < BITRAF_STATES; ++i) {
        uint8_t value = (uint8_t)(v->state[i] % BITRAF_BASE);
        pending |= (uint8_t)(value << pending_bits);
        pending_bits += 5; /* base-20 fits in 5 bits */

        while (pending_bits >= 8) {
            if (out_pos >= out_cap) {
                return out_pos;
            }
            out[out_pos++] = (uint8_t)(pending & 0xFFu);
            pending >>= 8;
            pending_bits -= 8;
        }
    }

    if (pending_bits > 0 && out_pos < out_cap) {
        out[out_pos++] = pending;
    }

    return out_pos;
}

static void append_zipraf_record(const char *path, const bitraf_vector *v, uint32_t record_id) {
    uint32_t parity_even = 0, parity_odd = 0;
    uint32_t fractals[BITRAF_FRACTALS];
    compute_parities(v, &parity_even, &parity_odd);
    compute_fractals(v, fractals);

    uint8_t packed[BITRAF_STATES];
    size_t packed_len = pack_base20(v, packed, sizeof(packed));

    zipraf_header header;
    header.magic = ZIPRAF_MAGIC;
    header.record_id = record_id;
    header.packed_len = (uint32_t)packed_len;
    header.parity_even = parity_even;
    header.parity_odd = parity_odd;
    for (size_t i = 0; i < BITRAF_FRACTALS; ++i) {
        header.fractal[i] = fractals[i];
    }

    FILE *f = fopen(path, "ab");
    if (!f) {
        fprintf(stderr, "Could not open %s for append.\n", path);
        return;
    }

    fwrite(&header, sizeof(header), 1, f);
    fwrite(packed, 1, packed_len, f);
    fclose(f);

    printf("[ZipRafStore] Record %u appended to %s (len=%zu, parities=%u/%u)\n",
           record_id, path, packed_len, parity_even, parity_odd);
    printf("  Fractals: %08X %08X %08X %08X\n",
           fractals[0], fractals[1], fractals[2], fractals[3]);
}

int main(int argc, char **argv) {
    const char *source_path = NULL;
    const char *verb_arg = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--verb") == 0 && i + 1 < argc) {
            verb_arg = argv[++i];
        } else {
            source_path = argv[i];
        }
    }

    uint8_t raw[BITRAF_STATES];
    size_t raw_len = 0;
    load_source(source_path, raw, sizeof(raw), &raw_len);

    bitraf_vector base = {0};
    normalize_to_bitraf(raw, raw_len, &base);

    bitraf_vector transformed = {0};
    bitraf_verb verb = parse_verb(verb_arg);
    apply_verb(&transformed, &base, verb);

    const char *verb_name = verb_arg ? verb_arg : "pass";
    printf("[BITRAF] verb=%s states=%d base=%d\n", verb_name, BITRAF_STATES, BITRAF_BASE);

    append_zipraf_record("databaseraf.bin", &transformed, now_ms());
    return 0;
}
