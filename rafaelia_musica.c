/*
 * RAFAELIA MÚSICA SDK (C11, low-level, sem dependências externas)
 * ---------------------------------------------------------------
 * Pequeno motor determinístico ψχρΔΣΩ com excitação interna para evitar
 * o colapso no atrator trivial. Usa somente matrizes/arrays estáticos,
 * CRC32 in-line e mapeamento toroidal para observability.
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define VEC_DIM 6
#define PHI 1.6180339887498948482
#define RAFA_MAGIC 0x52414641u /* "RAFA" */
#define CRC32_POLY 0xEDB88320u  /* IEEE 802.3 (reversed) */
/* Laplaciano menos dissipativo para manter o motor “vivo” */
static const double LAPLACE_ALPHA = 0.28;
static const double TORUS_R_MAJOR = 2.0;
static const double TORUS_R_MINOR = 0.65;
static const double MICRO_PSI_GAIN = 0.0020;
static const double MICRO_DELTA_GAIN = 0.0012;
static const double MICRO_PHASE = 0.37;
static const double MEM_U_GAIN = 0.20;
static const double MEM_W_GAIN = 0.17;
static const double ANGLE_BASE_STEP = 0.015;
static const double ANGLE_MOD_STEP = 0.002;
static const double INIT_MODULUS = 1000.0;
static const double INIT_DIVISOR = 5000.0;

typedef struct {
    double   v[VEC_DIM]; /* ψ χ ρ Δ Σ Ω */
    double   mem_u;
    double   mem_w;
    double   angle;
    uint64_t step;
    uint32_t crc;
} RafaeliaState;

/* CRC32 table-less (polynomial 0xEDB88320) */
static uint32_t crc32_update(uint32_t prev, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *) data;
    uint32_t c = ~prev;
    while (len--) {
        c ^= *p++;
        /* Reflected bitwise CRC32 (IEEE 802.3, reversed polynomial 0xEDB88320) */
        for (int k = 0; k < 8; k++) {
            c = (c >> 1) ^ ((c & 1u) ? CRC32_POLY : 0u);
        }
    }
    return ~c;
}

static uint32_t crc_state(const RafaeliaState *R) {
    uint32_t c = R->crc;
    c = crc32_update(c, R->v, sizeof(R->v));
    c = crc32_update(c, &R->mem_u, sizeof(R->mem_u));
    c = crc32_update(c, &R->mem_w, sizeof(R->mem_w));
    c = crc32_update(c, &R->angle, sizeof(R->angle));
    c = crc32_update(c, &R->step, sizeof(R->step));
    return c;
}

static int parse_pos_int(const char *s, int fallback) {
    if (!s) return fallback;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    return (end == s || v <= 0 || v > (long)INT_MAX) ? fallback : (int)v;
}

static uint32_t parse_seed(const char *s, uint32_t fallback) {
    if (!s) return fallback;
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 10);
    return (end == s || v > (unsigned long)UINT32_MAX) ? fallback : (uint32_t)v;
}

static void apply_verbo(RafaeliaState *R, const double verbo[VEC_DIM]) {
    for (int i = 0; i < VEC_DIM; i++) {
        R->v[i] += verbo[i];
    }
}

/* Difusão discreta (Laplaciano Wx - D*x) em anel 6x6 */
static void laplacian_step(RafaeliaState *R) {
    static const double W[VEC_DIM][VEC_DIM] = {
        {0, 0.5, 0, 0, 0, 0.5},
        {0.5, 0, 0.5, 0, 0, 0},
        {0, 0.5, 0, 0.5, 0, 0},
        {0, 0, 0.5, 0, 0.5, 0},
        {0, 0, 0, 0.5, 0, 0.5},
        {0.5, 0, 0, 0, 0.5, 0},
    };
    static const double D[VEC_DIM] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    double next[VEC_DIM];

    for (int i = 0; i < VEC_DIM; i++) {
        double acc = 0.0;
        for (int j = 0; j < VEC_DIM; j++) {
            double w = W[i][j];
            if (w != 0.0) {
                acc += w * R->v[j];
            }
        }
        next[i] = R->v[i] + LAPLACE_ALPHA * (acc - D[i] * R->v[i]);
    }
    memcpy(R->v, next, sizeof(next));
}

/* Shannon entropy in 6 dimensions (uses |v| as probability mass) */
static double entropy6(const double v[VEC_DIM]) {
    double total = 0.0;
    for (int i = 0; i < VEC_DIM; i++) total += fabs(v[i]);
    if (total < 1e-12) return 0.0;
    double h = 0.0;
    for (int i = 0; i < VEC_DIM; i++) {
        double p = fabs(v[i]) / total;
        if (p > 0.0) h -= p * log(p);
    }
    return h;
}

static double coherence6(const double v[VEC_DIM]) {
    const double h = entropy6(v);
    const double h_max = log((double)VEC_DIM);
    return (h_max > 0.0) ? (1.0 - h / h_max) : 0.0;
}

static void torus_embed(const RafaeliaState *R, double *x, double *y, double *z) {
    double cu = cos(R->mem_u), su = sin(R->mem_u);
    double cw = cos(R->mem_w), sw = sin(R->mem_w);
    *x = (TORUS_R_MAJOR + TORUS_R_MINOR * cw) * cu;
    *y = (TORUS_R_MAJOR + TORUS_R_MINOR * cw) * su;
    *z = TORUS_R_MINOR * sw;
}

static void step_cycle(RafaeliaState *R, const double verbo[VEC_DIM], bool has_verbo) {
    if (has_verbo && verbo) apply_verbo(R, verbo);

    /* Internal micro-verbo that keeps the oscillator energized */
    R->v[0] += MICRO_PSI_GAIN * sin(R->angle + MICRO_PHASE);
    R->v[3] += MICRO_DELTA_GAIN * cos(R->angle * PHI);

    laplacian_step(R);

    /* Toroidal memories prevent the system from freezing */
    double s1 = R->v[0] + R->v[1] + R->v[2];
    double s2 = R->v[3] + R->v[4] + R->v[5];
    R->mem_u += MEM_U_GAIN * tanh(s1);
    R->mem_w += MEM_W_GAIN * tanh(s2);

    /* Final saturation applied only to noise/transduction channels */
    R->v[2] = tanh(R->v[2]);  /* ρ */
    R->v[3] = tanh(R->v[3]);  /* Δ */

    R->angle += ANGLE_BASE_STEP + ANGLE_MOD_STEP * sin(R->mem_u + R->mem_w);
    R->crc = crc_state(R);
    R->step++;
}

int main(int argc, char **argv) {
    int steps = (argc > 1) ? parse_pos_int(argv[1], 120) : 120;
    uint32_t seed = (argc > 2) ? parse_seed(argv[2], 4242u) : 4242u;

    RafaeliaState R = {0};
    uint32_t mixer = seed;
    for (int i = 0; i < VEC_DIM; i++) {
        uint32_t input = seed ^ (uint32_t)i;
        mixer = crc32_update(mixer, &input, sizeof(input));
        R.v[i] = (double)(mixer % (uint32_t)INIT_MODULUS) / INIT_DIVISOR;
    }
    R.angle = MICRO_PHASE;
    R.crc = RAFA_MAGIC; /* "RAFA" */

    double verbo[VEC_DIM] = {0}; /* Optional stdin injection; zeroed for demo */
    const bool has_verbo = false;

    printf("=== RAFAELIA MUSICA (SDK low-level) ===\n");
    printf("Passos: %d | Semente: %u\n\n", steps, seed);

    for (int t = 0; t < steps; t++) {
        step_cycle(&R, verbo, has_verbo);

        double X, Y, Z;
        torus_embed(&R, &X, &Y, &Z);
        double H = entropy6(R.v);
        double C = coherence6(R.v);

        printf("t=%03d psi/chi/rho/delta/sigma/omega=[%.5f %.5f %.5f %.5f %.5f %.5f] ", t,
               R.v[0], R.v[1], R.v[2], R.v[3], R.v[4], R.v[5]);
        printf("Toro=(%.3f, %.3f, %.3f) H=%.4f C=%.4f CRC=%08X\n",
               X, Y, Z, H, C, R.crc);
    }

    return 0;
}
