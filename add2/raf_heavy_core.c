#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>
#include <time.h>

// === DEFINIÇÃO DA CARGA (A "Necessidade") ===
// Aumentamos para 100 Milhões de elementos (aprox 762 MB)
#define NUM_ELEMENTS 100000000 
#define SEED_VAL 1.6180339887 // Golden Ratio

// Função Pesada: Simula cálculo de entropia não-linear
// Força a ALU a trabalhar com raiz quadrada e trigonometria
double heavy_calc(double val, int idx) {
    return sqrt(fabs(val)) * sin(val * 0.001) + cos(idx * 0.002);
}

int main() {
    printf("=== RAFAELIA: HEAVY CORE SIMULATION ===\n");
    printf("Alocando Universo [%d elementos]...\n", NUM_ELEMENTS);
    
    // Alocação na Heap (Stack não aguentaria)
    double *data = (double*) malloc(NUM_ELEMENTS * sizeof(double));
    if (!data) { perror("Falha na Alocação de Memória"); return 1; }

    // Gênese (Inicialização Paralela para velocidade)
    #pragma omp parallel for
    for (long i = 0; i < NUM_ELEMENTS; i++) {
        data[i] = (double)i * SEED_VAL;
    }
    printf("[OK] Universo Carregado na RAM.\n\n");

    // --- TESTE 1: SERIAL (SINGLE CORE) ---
    printf(">> Iniciando Processamento SERIAL (Base Line)...\n");
    double start_s = omp_get_wtime();
    double sum_serial = 0.0;
    
    for (long i = 0; i < NUM_ELEMENTS; i++) {
        sum_serial += heavy_calc(data[i], i);
    }
    
    double end_s = omp_get_wtime();
    double time_serial = end_s - start_s;
    printf("   [SERIAL] Tempo: %.4f s | Checksum: %.2f\n\n", time_serial, sum_serial);

    // --- TESTE 2: PARALELO (MULTI-CORE + VECTORIZATION) ---
    printf(">> Iniciando Processamento OTIMIZADO (OpenMP + AVX)...\n");
    double start_p = omp_get_wtime();
    double sum_parallel = 0.0;

    // OMP Parallel: Cria threads
    // Reduction: Soma segura sem race condition
    // Schedule Static: Divisão igualitária de carga (bom para arrays densos)
    #pragma omp parallel for reduction(+:sum_parallel) schedule(static)
    for (long i = 0; i < NUM_ELEMENTS; i++) {
        sum_parallel += heavy_calc(data[i], i);
    }

    double end_p = omp_get_wtime();
    double time_parallel = end_p - start_p;
    
    // Validação
    double diff = fabs(sum_serial - sum_parallel);
    
    printf("   [TURBO ] Tempo: %.4f s | Checksum: %.2f\n", time_parallel, sum_parallel);
    printf("   -----------------------------------------\n");
    if (diff < 0.001) printf("   [VALIDACAO] SUCESSO. Resultados identicos.\n");
    else              printf("   [VALIDACAO] ERRO. Divergencia numerica!\n");

    // RESULTADOS FINAIS
    printf("\n=== RELATÓRIO DE EFICIÊNCIA ===\n");
    printf("⚡ ACELERAÇÃO (Speedup): %.2fx mais rápido\n", time_serial / time_parallel);
    printf("   Processados: %.0f milhoes de ops complexas/seg\n", (NUM_ELEMENTS/time_parallel)/1000000.0);

    free(data);
    return 0;
}
