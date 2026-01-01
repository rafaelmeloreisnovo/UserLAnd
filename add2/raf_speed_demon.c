#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

// MUDANÇA 1: Usar float (32-bit) em vez de double (64-bit)
#define REAL float
#define NUM_ELEMENTS 100000000 
#define SEED_VAL 1.61803f

// MUDANÇA 2: Funções com sufixo 'f' (sinf, sqrtf, cosf)
// Isso evita a conversão implicita para double que mata a performance
REAL heavy_calc(REAL val, int idx) {
    return sqrtf(fabsf(val)) * sinf(val * 0.001f) + cosf(idx * 0.002f);
}

int main() {
    printf("=== RAFAELIA: SPEED DEMON (FLOAT32) ===\n");
    
    REAL *data = (REAL*) malloc(NUM_ELEMENTS * sizeof(REAL));
    if (!data) return 1;

    // Inicialização
    #pragma omp parallel for
    for (long i = 0; i < NUM_ELEMENTS; i++) {
        data[i] = (REAL)i * SEED_VAL;
    }

    // --- BASELINE (SERIAL) ---
    double start_s = omp_get_wtime();
    REAL sum_serial = 0.0f;
    for (long i = 0; i < NUM_ELEMENTS; i++) {
        sum_serial += heavy_calc(data[i], i);
    }
    double time_serial = omp_get_wtime() - start_s;
    printf("[SERIAL 32-bit] Tempo: %.4f s\n", time_serial);

    // --- TURBO (PARALLEL) ---
    double start_p = omp_get_wtime();
    REAL sum_parallel = 0.0f;

    // SIMD agressivo
    #pragma omp parallel for reduction(+:sum_parallel) schedule(static)
    for (long i = 0; i < NUM_ELEMENTS; i++) {
        sum_parallel += heavy_calc(data[i], i);
    }
    double time_parallel = omp_get_wtime() - start_p;

    printf("[TURBO  32-bit] Tempo: %.4f s\n", time_parallel);
    printf("-----------------------------------------\n");
    printf("⚡ SPEEDUP FINAL: %.2fx (vs Serial Float)\n", time_serial / time_parallel);
    
    // Comparar Maçãs com Laranjas (Ganho sobre o código anterior em Double)
    // O valor 6.57s veio do seu log anterior
    double old_baseline = 6.5728; 
    printf("🚀 GANHO GLOBAL:  %.2fx (vs Double Original)\n", old_baseline / time_parallel);

    free(data);
    return 0;
}
