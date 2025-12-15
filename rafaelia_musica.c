/*
 * RAFAELIA MÚSICA SDK (C11, low-level, sem dependências externas)
 * ---------------------------------------------------------------
 * Pequeno motor determinístico ψχρΔΣΩ com excitação interna para evitar
 * o colapso no atrator trivial. Usa somente matrizes/arrays estáticos,
 * CRC32 in-line e mapeamento toroidal para observabilidade.
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VEC_DIM 6
#define PHI 1.6180339887498948482
#define RAFA_MAGIC 0x52414641u
/* Laplaciano menos dissipativo para manter o motor “vivo” */
static const double LAPLACE_ALPHA = 0.28;

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
        /* Reflected bitwise CRC32 using polynomial 0xEDB88320 */
        for (int k = 0; k < 8; k++) {
            c = (c >> 1) ^ (0xEDB88320u & (-(int32_t)(c & 1u)));
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
    double next[VEC_DIM];

    for (int i = 0; i < VEC_DIM; i++) {
        double acc = 0.0;
        double diag = 0.0;
        for (int j = 0; j < VEC_DIM; j++) {
            double w = W[i][j];
            if (w != 0.0) {
                acc += w * R->v[j];
                diag += w;
            }
        }
        next[i] = R->v[i] + LAPLACE_ALPHA * (acc - diag * R->v[i]);
    }
    memcpy(R->v, next, sizeof(next));
}

/* Entropia Shannon em 6 dimensões (usa |v| como probabilidade) */
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
    const double Rmaj = 2.0;
    const double rmin = 0.65;
    double cu = cos(R->mem_u), su = sin(R->mem_u);
    double cw = cos(R->mem_w), sw = sin(R->mem_w);
    *x = (Rmaj + rmin * cw) * cu;
    *y = (Rmaj + rmin * cw) * su;
    *z = rmin * sw;
}

static void step_cycle(RafaeliaState *R, const double verbo[VEC_DIM], int got) {
    if (got && verbo) apply_verbo(R, verbo);

    /* micro-verbo interno (mantém música viva) */
    R->v[0] += 0.0020 * sin(R->angle + 0.37);
    R->v[3] += 0.0012 * cos(R->angle * PHI);

    laplacian_step(R);

    /* Memórias toroidais (evitam congelar) */
    double s1 = R->v[0] + R->v[1] + R->v[2];
    double s2 = R->v[3] + R->v[4] + R->v[5];
    R->mem_u += 0.20 * tanh(s1);
    R->mem_w += 0.17 * tanh(s2);

    /* 5) saturação final (apenas nos canais de ruído/transdução) */
    R->v[2] = tanh(R->v[2]);  /* ρ */
    R->v[3] = tanh(R->v[3]);  /* Δ */

    R->angle += 0.015 + 0.002 * sin(R->mem_u + R->mem_w);
    R->crc = crc_state(R);
    R->step++;
}

int main(int argc, char **argv) {
    int steps = (argc > 1) ? atoi(argv[1]) : 120;
    uint32_t seed = (argc > 2) ? (uint32_t)atoi(argv[2]) : 4242u;

    RafaeliaState R = {0};
    for (int i = 0; i < VEC_DIM; i++) {
        uint32_t tmp = seed;
        seed = crc32_update(seed, &tmp, sizeof(tmp));
        R.v[i] = (double)(seed % 1000) / 5000.0;
    }
    R.angle = 0.37;
    R.crc = RAFA_MAGIC; /* "RAFA" */

    double verbo[VEC_DIM] = {0}; /* stdin opcional; fica zerado para demo */

    printf("=== RAFAELIA MÚSICA (SDK low-level) ===\n");
    printf("Passos: %d | Semente: %u\n\n", steps, seed);

    for (int t = 0; t < steps; t++) {
        step_cycle(&R, verbo, 0);

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
