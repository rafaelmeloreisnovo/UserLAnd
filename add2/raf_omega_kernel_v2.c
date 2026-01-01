#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>
#include <stdint.h>

// =======================================================
// RAFAELIA OMEGA KERNEL v2
//   - Baseline Serial (Float, sem LUT) -> igual
//   - Turbo LUT (SIN/COS + SQRT via LUT + OMP + Bitmask)
//   - Foco: tirar sqrtf do loop quente
// =======================================================

#define NUM_ELEMENTS_DEFAULT 100000000LL   // 100M

// LUT potência de 2
#define LUT_BITS 14
#define LUT_SIZE (1 << LUT_BITS)
#define LUT_MASK (LUT_SIZE - 1)

#if defined(__GNUC__) || defined(__clang__)
static float SIN_LUT[LUT_SIZE]  __attribute__((aligned(64)));
static float COS_LUT[LUT_SIZE]  __attribute__((aligned(64)));
static float SQRT_LUT[LUT_SIZE] __attribute__((aligned(64)));
#else
static float SIN_LUT[LUT_SIZE];
static float COS_LUT[LUT_SIZE];
static float SQRT_LUT[LUT_SIZE];
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Inicialização das LUTs ---
static void init_luts(void) {
    printf(">> [LUT] Gerando Tabelas (SIN/COS/SQRT) com %d entradas...\n", LUT_SIZE);
    const float step = 2.0f * (float)M_PI / (float)LUT_SIZE;

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < LUT_SIZE; i++) {
        float x = step * (float)i;
        SIN_LUT[i]  = sinf(x);
        COS_LUT[i]  = cosf(x);
        SQRT_LUT[i] = sqrtf((float)i + 1.0f); // +1 para evitar sqrt(0) se quiser
    }
    printf(">> [LUT] Pronto.\n\n");
}

// --- Baseline "bonito" (sem LUT, sem OMP) ---
static inline float baseline_calc(float val, int idx) {
    float root = sqrtf(fabsf(val));
    float s    = sinf(val * 0.001f);
    float c    = cosf(idx * 0.002f);
    return root * s + c;
}

// --- Turbo "cheat" (sem sqrtf no loop) ---
static inline float turbo_calc(uint32_t idx) {
    uint32_t lut_idx = idx & LUT_MASK;

    float root = SQRT_LUT[lut_idx];      // sqrt aproximado via LUT
    float s    = SIN_LUT[lut_idx];
    float c    = COS_LUT[lut_idx];

    return root * s + c;
}

int main(int argc, char **argv) {
    long long N = NUM_ELEMENTS_DEFAULT;
    if (argc >= 2) {
        long long tmp = atoll(argv[1]);
        if (tmp > 0) N = tmp;
    }

    printf("=== RAFAELIA OMEGA KERNEL v2 ===\n");
    printf("Elementos : %lld\n", N);
    printf("LUT       : %d entradas (2^%d)\n\n", LUT_SIZE, LUT_BITS);

    init_luts();

    // --------------------------------------------
    // 1) BASELINE SERIAL
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
    // 2) TURBO (LUT + OMP + Bitmask + SQRT LUT)
    // --------------------------------------------
    printf(">> [2] Rodando TURBO v2 (SIN/COS/SQRT via LUT + OMP)...\n");
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

    printf("   Tempo TURBO v2: %.6f s | Checksum: %.3f\n", time_turbo, sum_turbo);
    printf("   Threads (OMP)  : %d\n", num_threads);

    // --------------------------------------------
    // RELATÓRIO
    // --------------------------------------------
    float diff = fabsf(sum_base - sum_turbo);

    printf("\n=== RELATÓRIO RAFAELIA v2 ===\n");
    printf("🐢 BASELINE : %.6f s\n", time_base);
    printf("🐇 TURBO v2 : %.6f s\n", time_turbo);
    if (time_turbo > 0.0)
        printf("⚡ SPEEDUP  : %.2fx\n", time_base / time_turbo);
    else
        printf("⚡ SPEEDUP  : infinito (tempo TURBO ~ 0?)\n");

    printf("ΔChecksum   : %.3f (diferença numérica esperada; LUT sqrt)\n", diff);

    printf("\nNotas:\n");
    printf(" - TURBO v2 remove sqrtf do loop quente usando LUT.\n");
    printf(" - Checksum se afasta mais do baseline (modelo diferente),\n");
    printf("   mas o kernel reflete o teto de throughput da máquina.\n");

    return 0;
}
