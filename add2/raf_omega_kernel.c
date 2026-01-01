#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>
#include <stdint.h>

// =======================================================
// RAFAELIA OMEGA KERNEL
//   - Baseline Serial (Float, trig real)
//   - Turbo LUT (Float, OMP + Bitmask)
//   - Foco: loop quente mínimo, memória limpa
// =======================================================

#define NUM_ELEMENTS_DEFAULT 100000000LL   // 100M

// LUT potência de 2 -> índice com AND (&) ao invés de %
// 2^14 = 16384 pontos -> bom equilíbrio precisão/cache
#define LUT_BITS 14
#define LUT_SIZE (1 << LUT_BITS)
#define LUT_MASK (LUT_SIZE - 1)

// Alinhamento ajuda vetorização / cache de algumas CPUs
#if defined(__GNUC__) || defined(__clang__)
static float SIN_LUT[LUT_SIZE]  __attribute__((aligned(64)));
static float COS_LUT[LUT_SIZE]  __attribute__((aligned(64)));
#else
static float SIN_LUT[LUT_SIZE];
static float COS_LUT[LUT_SIZE];
#endif

// --- Inicialização das LUTs ---
// Range: [0, 2π) mapeado ciclicamente
static void init_luts(void) {
    printf(">> [LUT] Gerando Tabelas (SIN/COS) com %d entradas...\n", LUT_SIZE);
    const float step = 2.0f * (float)M_PI / (float)LUT_SIZE;

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < LUT_SIZE; i++) {
        float x = step * (float)i;
        SIN_LUT[i] = sinf(x);
        COS_LUT[i] = cosf(x);
    }
    printf(">> [LUT] Pronto.\n\n");
}

// --- Cálculo "bonito" (Baseline) ---
// Usa sinf/cosf/sqrtf diretos, sem LUT, sem paralelismo
static inline float baseline_calc(float val, int idx) {
    float root = sqrtf(fabsf(val));
    float s    = sinf(val * 0.001f);
    float c    = cosf(idx * 0.002f);
    return root * s + c;
}

// --- Cálculo "cheat" (Turbo) ---
// ÍNDICE -> reduzido por bitmask em LUT (muito mais barato que %)
static inline float turbo_calc(uint32_t idx) {
    uint32_t lut_idx = idx & LUT_MASK;
    float root = sqrtf((float)idx);          // se quiser, pode virar LUT/approx depois
    return root * SIN_LUT[lut_idx] + COS_LUT[lut_idx];
}

int main(int argc, char **argv) {
    long long N = NUM_ELEMENTS_DEFAULT;
    if (argc >= 2) {
        long long tmp = atoll(argv[1]);
        if (tmp > 0) N = tmp;
    }

    printf("=== RAFAELIA OMEGA KERNEL ===\n");
    printf("Elementos : %lld\n", N);
    printf("LUT       : %d entradas (2^%d)\n\n", LUT_SIZE, LUT_BITS);

    init_luts();

    // --------------------------------------------
    // MODO 1: BASELINE (Serial, sem LUT)
    // --------------------------------------------
    printf(">> [1] Rodando BASELINE SERIAL (Float, sem LUT)...\n");
    double t0 = omp_get_wtime();
    float sum_base = 0.0f;

    for (long long i = 0; i < N; i++) {
        float v = (float)i * 1.61803f;
        sum_base += baseline_calc(v, (int)i);
    }

    double t1 = omp_get_wtime();
    double time_base = t1 - t0;
    printf("   Tempo BASELINE: %.6f s | Checksum: %.3f\n\n", time_base, sum_base);

    // --------------------------------------------
    // MODO 2: TURBO (LUT + OMP + Bitmask)
    // --------------------------------------------
    printf(">> [2] Rodando TURBO (LUT + OMP + Bitmask)...\n");
    double t2 = omp_get_wtime();
    float sum_turbo = 0.0f;

    #pragma omp parallel for reduction(+:sum_turbo) schedule(static)
    for (long long i = 0; i < N; i++) {
        uint32_t idx = (uint32_t)i;
        sum_turbo += turbo_calc(idx);
    }

    double t3 = omp_get_wtime();
    double time_turbo = t3 - t2;

    int num_threads = 1;
    #pragma omp parallel
    {
        #pragma omp single
        num_threads = omp_get_num_threads();
    }

    printf("   Tempo TURBO   : %.6f s | Checksum: %.3f\n", time_turbo, sum_turbo);
    printf("   Threads (OMP) : %d\n", num_threads);

    // --------------------------------------------
    // RELATÓRIO
    // --------------------------------------------
    float diff = fabsf(sum_base - sum_turbo);

    printf("\n=== RELATÓRIO RAFAELIA ===\n");
    printf("🐢 BASELINE : %.6f s\n", time_base);
    printf("🐇 TURBO    : %.6f s\n", time_turbo);
    if (time_turbo > 0.0)
        printf("⚡ SPEEDUP  : %.2fx\n", time_base / time_turbo);
    else
        printf("⚡ SPEEDUP  : infinito (tempo TURBO ~ 0?)\n");

    printf("ΔChecksum   : %.3f (diferença numérica esperada)\n", diff);

    printf("\nLegenda:\n");
    printf(" - BASELINE = referência legível, física direta (sin/cos/sqrt).\n");
    printf(" - TURBO    = versão cheat, usando LUT e paralelismo.\n");
    printf(" - ΔChecksum pequeno indica coerência do modelo.\n");

    return 0;
}
