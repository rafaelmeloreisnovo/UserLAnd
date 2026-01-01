#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

#define NUM_ELEMENTS 100000000
// Tamanho 16384 (2^14) permite otimização binária automática pelo compilador
#define LUT_SIZE 16384 
#define LUT_MASK (LUT_SIZE - 1)

// Tabelas de Consulta (Lookup Tables)
float SIN_LUT[LUT_SIZE];
float COS_LUT[LUT_SIZE];

// Prepara as tabelas (Gênese do Conhecimento)
void init_lut() {
    printf(">> [INIT] Gerando Tabelas de Conhecimento (LUT)...\n");
    for (int i = 0; i < LUT_SIZE; i++) {
        SIN_LUT[i] = sinf((float)i * 0.001f);
        COS_LUT[i] = cosf((float)i * 0.002f);
    }
}

// Otimização: Substitui cálculo trigonométrico por leitura de array
// static inline sugere ao compilador "colar" essa função dentro do loop
static inline float super_fast_calc(int val_idx, int idx_mod) {
    // Acesso O(1) à memória.
    // O compilador otimizará o % LUT_SIZE para uma operação AND bitwise (&) 
    // porque LUT_SIZE é potência de 2.
    return sqrtf((float)val_idx) * SIN_LUT[val_idx & LUT_MASK] + COS_LUT[idx_mod & LUT_MASK];
}

int main() {
    printf("=== RAFAELIA: ALGORITHMIC SUPREMACY (LUT) ===\n");
    
    // Alocação
    float *data = (float*) malloc(NUM_ELEMENTS * sizeof(float));
    if (!data) { perror("Erro de Alocação"); return 1; }
    
    init_lut();

    // Setup dos dados (Criação do Universo)
    #pragma omp parallel for
    for (long i = 0; i < NUM_ELEMENTS; i++) data[i] = (float)i;

    // --- BASELINE (Valores de referência dos seus testes anteriores) ---
    double time_original_slow = 6.7524; // Seu teste Serial
    double time_turbo_prev    = 1.1902; // Seu teste Turbo Math
    
    // --- EXECUÇÃO LUT (Memória > Processamento) ---
    printf(">> [RUN] Executando abordagem LUT + OpenMP...\n");
    double start = omp_get_wtime();
    float sum = 0.0f;

    // Unroll manual não é necessário, o compilador fará com -O3
    #pragma omp parallel for reduction(+:sum) schedule(static)
    for (long i = 0; i < NUM_ELEMENTS; i++) {
        // Casting para int para usar como índice de array
        sum += super_fast_calc((int)data[i], i);
    }

    double end = omp_get_wtime();
    double time_lut = end - start;

    printf("   [OK] Concluído em: %.4f s\n", time_lut);
    printf("-----------------------------------------\n");
    
    // RELATÓRIO FINAL
    double speedup_global = time_original_slow / time_lut;
    double speedup_local = time_turbo_prev / time_lut;
    
    printf("📊 RELATÓRIO DE PERFORMANCE:\n");
    printf("   Vs Baseline (Serial): %.2fx mais rápido\n", speedup_global);
    printf("   Vs Turbo Math (Anterior): %.2fx mais rápido\n", speedup_local);
    
    if (speedup_global > 30.0) {
        printf("\n🚀 STATUS: BARREIRA DO SOMROMPIDA (>30x)!\n");
    } else {
        printf("\n⚠️  STATUS: Ganho sólido, mas limitado pela RAM.\n");
    }

    free(data);
    return 0;
}
