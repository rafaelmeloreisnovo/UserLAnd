#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

/* * NORMA IEEE 1012: Verificação e Validação.
 * OTIMIZAÇÃO: ARM64 NEON SIMD (implícito via compilador -O3).
 */

#define THRESHOLD 2 // Limite de diferença para "trecho parecido"

typedef struct {
    const char *data;
    size_t size;
    const char *pattern;
    size_t pattern_len;
} SearchTask;

// Função de similaridade (Fator de Paridade √S aplicado a texto)
void search_similar(const char *map, size_t file_size, const char *pattern) {
    size_t p_len = strlen(pattern);
    
    // Slicer Ω: Varredura por janela deslizante
    for (size_t i = 0; i <= file_size - p_len; i++) {
        int diff = 0;
        for (size_t j = 0; j < p_len; j++) {
            if (map[i + j] != pattern[j]) {
                diff++;
            }
            if (diff > THRESHOLD) break; 
        }

        if (diff <= THRESHOLD) {
            printf("[MATCH FOUND] Offset: %zu | Trecho: %.*s\n", i, (int)p_len, &map[i]);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s <arquivo> \"padrão\"\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    const char *pattern = argv[2];

    // ISO 27001: Garantia de disponibilidade e integridade
    int fd = open(filename, O_RDONLY);
    if (fd == -1) { perror("Erro ao abrir"); return 1; }

    struct stat st;
    fstat(fd, &st);
    size_t size = st.st_size;

    // MMAP: Técnica essencial para 800MB em ARM64 (não sobrecarrega RAM)
    char *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) { perror("Erro no mmap"); close(fd); return 1; }

    printf("--- RAFAELIA Ω SLICER ATIVO ---\n");
    printf("Analisando %zu bytes em busca de: %s\n", size, pattern);

    search_similar(map, size, pattern);

    // Limpeza (Vazio -> Novo Vazio)
    munmap(map, size);
    close(fd);
    return 0;
}
