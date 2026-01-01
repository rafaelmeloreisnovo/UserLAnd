#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

// 100 Milhões de Pontos
#define NUM_ELEMENTS 100000000 
#define SEED_VAL 1.61803f

// --- TRUQUE ANTI-OTIMIZAÇÃO (Para o compilador não deletar o loop) ---
void black_hole(void* p) {
    asm volatile("" : : "g"(p) : "memory");
}

// --- MODO LENTO (Simula código padrão/legado) ---
// Sem vetorização, Double precision, Serial
__attribute__((optimize("no-tree-vectorize")))
double heavy_slow(double val, int idx) {
    // Trigometria pesada em Double
    return sqrt(fabs(val)) * sin(val * 0.001) + cos(idx * 0.002);
}

// --- MODO TURBO (Metal) ---
// Float precision, Fast Math intrínseco
float heavy_fast(float val, int idx) {
    // Versões 'f' (sinf, sqrtf) são muito mais rápidas na FPU
    return sqrtf(fabsf(val)) * sinf(val * 0.001f) + cosf(idx * 0.002f);
}

int main() {
    printf("=== RAFAELIA: QUANTUM LEAP (50X ATTEMPT) ===\n");
    printf("Alocando memória...\n");

    // Alinhamento de memória para facilitar vetorização (64 bytes)
    float *data_f;
    double *data_d;
    posix_memalign((void**)&data_f, 64, NUM_ELEMENTS * sizeof(float));
    posix_memalign((void**)&data_d, 64, NUM_ELEMENTS * sizeof(double));

    // Inicialização Paralela
    #pragma omp parallel for
    for (long i = 0; i < NUM_ELEMENTS; i++) {
        data_f[i] = (float)i * SEED_VAL;
        data_d[i] = (double)i * SEED_VAL;
    }

    // ---------------------------------------------------------
    // TESTE 1: A Realidade Comum (Single Thread, Double, Scalar)
    // ---------------------------------------------------------
    printf("\n>> [1] Executando MODO LENTO (Baseline)...\n");
    double start_slow = omp_get_wtime();
    double sum_slow = 0.0;
    
    // Forçamos execução serial
    for (long i = 0; i < NUM_ELEMENTS; i++) {
        sum_slow += heavy_slow(data_d[i], i);
    }
    black_hole(&sum_slow); // Impede deleção
    
    double time_slow = omp_get_wtime() - start_slow;
    printf("   Tempo Lento: %.4f s\n", time_slow);


    // ---------------------------------------------------------
    // TESTE 2: A Velocidade da Luz (Multi Thread, Float, Vector)
    // ---------------------------------------------------------
    printf("\n>> [2] Executando MODO TURBO (Otimizado)...\n");
    double start_fast = omp_get_wtime();
    float sum_fast = 0.0f;

    // OMP com Schedule Guided (Melhor balanceamento de carga)
    // Simd: Força uso de instruções NEON/AVX
    #pragma omp parallel for reduction(+:sum_fast) schedule(guided)
    for (long i = 0; i < NUM_ELEMENTS; i++) {
        sum_fast += heavy_fast(data_f[i], i);
    }
    black_hole(&sum_fast); // Impede deleção

    double time_fast = omp_get_wtime() - start_fast;
    printf("   Tempo Turbo: %.4f s\n", time_fast);

    // ---------------------------------------------------------
    // RESULTADOS
    // ---------------------------------------------------------
    double speedup = time_slow / time_fast;
    printf("\n=== PLACAR FINAL ===\n");
    printf("🐢 Baseline : %.4f s\n", time_slow);
    printf("🐇 Otimizado: %.4f s\n", time_fast);
    printf("⚡ SPEEDUP  : %.2fx\n", speedup);
    
    if (speedup > 40.0) printf("🏆 CONQUISTA DESBLOQUEADA: VELOCIDADE DE SUPERCOMPUTADOR!\n");
    else printf("🔧 Nota: Para mais velocidade, seria necessário GPU (CUDA/OpenCL).\n");

    free(data_f);
    free(data_d);
    return 0;
}
