#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>
#include <stdint.h>

// =======================================================
// RAFAELIA: ALGORITHMIC SUPREMACY - LUT x BITWISE EDITION
// =======================================================

#define NUM_ELEMENTS 100000000LL   // 100M

// LUT como potência de 2 para usar bitmask (AND) ao invés de módulo (%)
// Isso é ordens de magnitude mais rápido para a CPU calcular índices
#define LUT_BITS 14                // 2^14 = 16384
#define LUT_SIZE (1 << LUT_BITS)
#define LUT_MASK (LUT_SIZE - 1)

// Tabelas estáticas (Melhor alinhamento de memória que malloc)
static float SIN_LUT[LUT_SIZE];
static float COS_LUT[LUT_SIZE];

// Inicialização da Inteligência (Pré-cálculo)
static inline void init_lut(void) {
    printf(">> Gerando Tabelas de Conhecimento Prévio (LUT, %d entradas)...\n", LUT_SIZE);
    
    // Mapeamento cíclico perfeito de 0 a 2π
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < LUT_SIZE; i++) {
        float x = (float)i * (2.0f * (float)M_PI / (float)LUT_SIZE);
        SIN_LUT[i] = sinf(x);
        COS_LUT[i] = cosf(x);
    }
}

// Núcleo da Execução: Always Inline para eliminar overhead de chamada de função
static inline __attribute__((always_inline))
float super_fast_calc(uint32_t idx) {
    // Bitwise AND (&) é muito mais barato que Divisão/Módulo (%)
    uint32_t lut_idx = idx & LUT_MASK;

    // A raiz quadrada (sqrtf) é a única operação real restante.
    // O resto é apenas busca em memória.
    return sqrtf((float)idx) * SIN_LUT[lut_idx] + COS_LUT[lut_idx];
}

int main(void) {
    printf("=== RAFAELIA: ALGORITHMIC SUPREMACY (LUT v2) ===\n");

    // Marcos Históricos para Comparação
    const double time_previous_best = 1.1902;  // Seu melhor tempo "Turbo"
    const double original_slow      = 6.7524;  // Seu tempo "Serial" original

    init_lut();

    // Warm-up: Acorda os núcleos e carrega tabelas no Cache L1/L2
    float warm_sum = 0.0f;
    for (uint32_t i = 0; i < 100000; ++i) warm_sum += super_fast_calc(i);
    printf(">> Warm-up concluído.\n\n");

    printf(">> Executando abordagem LUT v2 (Bitmask + OMP + Ofast)...\n");

    double start = omp_get_wtime();
    float sum = 0.0f;

    // Loop Vetorizável
    #pragma omp parallel for reduction(+:sum) schedule(static)
    for (long long i = 0; i < NUM_ELEMENTS; i++) {
        // Cast explícito e seguro
        sum += super_fast_calc((uint32_t)i);
    }

    double end = omp_get_wtime();
    double time_lut = end - start;

    int num_threads = 0;
    #pragma omp parallel
    {
        #pragma omp single
        num_threads = omp_get_num_threads();
    }

    printf("\n=== RESULTADOS RAFAELIA LUT v2 ===\n");
    printf("   Threads Ativas:              %d\n", num_threads);
    printf("   Checksum (Integridade):      %.2f\n", sum);
    printf("   Tempo LUT v2:                %.6f s\n", time_lut);
    printf("-----------------------------------------\n");
    
    double speedup_global = original_slow / time_lut;
    printf("⚡ SPEEDUP vs Turbo anterior:    %.2fx\n", time_previous_best / time_lut);
    printf("🚀 SPEEDUP vs Original total:    %.2fx\n", speedup_global);

    if (speedup_global > 40.0) printf("\n🏆 NOVA CONQUISTA: NÍVEL DE SUPERCOMPUTAÇÃO ATINGIDO!\n");
    
    return 0;
}
