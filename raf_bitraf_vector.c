#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * BITRAF/ZIPRAF VETORIZADOR
 * -------------------------
 * Módulo C puro, sem dependências externas, pensado para converter vetores de
 * entrada em vetores normalizados de 1024 estados (BITRAF_STATES) e armazená-
 * los em um buffer de 20 camadas (ZIPRAF_BASE) com redundância de paridade
 * dupla. A geometria "10 x 10 x 10" serve como grade conceitual para mapear
 * coordenadas fractais (4 níveis) em um índice compacto.
 *
 * A proposta aqui é vector -> vector -> verbo: absorver sinais (como pixels),
 * vetorizar, aplicar um verbo (mixagem fractal/paridade) e persistir em um
 * armazenamento ZiprafStire.
 */

#define BITRAF_STATES 1024
#define ZIPRAF_BASE   20
#define FRACTAL_LVLS  4
#define PARITY_PLANES 2
#define CELL_VOLUME   1000 /* 10x10x10 */

/* Vetor normalizado com 1024 estados. */
typedef struct {
    uint16_t lane[BITRAF_STATES];
} BitrafVector;

/* Armazenamento ZiprafStire: 20 slots com duas paridades. */
typedef struct {
    BitrafVector slot[ZIPRAF_BASE];
    uint16_t parity[PARITY_PLANES];
    size_t frames_ingested;
} ZiprafStore;

static uint16_t fold_parity(const uint16_t *src, size_t len, uint16_t seed) {
    uint16_t p = seed;
    for (size_t i = 0; i < len; ++i) {
        p ^= src[i];
        p = (uint16_t)((p << 1) | (p >> 15));
    }
    return p;
}

static void bitraf_clear(BitrafVector *dst) {
    memset(dst->lane, 0, sizeof(dst->lane));
}

/*
 * Expande um vetor arbitrário (por exemplo, histograma) para 1024 estados.
 * Usa uma mistura de projeções fractais e máscara de paridade simples.
 */
static void bitraf_project(const uint16_t *input, size_t len, BitrafVector *out) {
    bitraf_clear(out);

    for (size_t i = 0; i < len; ++i) {
        uint32_t seed = (uint32_t)(i * 0x45d9f3b) ^ (uint32_t)input[i];
        for (int lvl = 0; lvl < FRACTAL_LVLS; ++lvl) {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            uint32_t idx = seed & (BITRAF_STATES - 1); // 1024-1 mask
            out->lane[idx] = (uint16_t)((out->lane[idx] + input[i]) % CELL_VOLUME);
        }
    }

    /* Injeta paridade leve espalhada no último quarto do vetor. */
    uint16_t parity_seed = fold_parity(input, len, 0xA5A5);
    for (size_t j = BITRAF_STATES * 3 / 4; j < BITRAF_STATES; ++j) {
        out->lane[j] ^= parity_seed;
        parity_seed = (uint16_t)((parity_seed << 1) | (parity_seed >> 15));
    }
}

/*
 * Converte um raster de pixels (0-255) em um histograma de 20 buckets e projeta
 * para o espaço BITRAF. Isso oferece um caminho simples para "absorver" imagens
 * sem qualquer dependência externa.
 */
static void zipraf_absorb_image(ZiprafStore *store, const uint8_t *pixels, size_t count) {
    uint16_t histogram[ZIPRAF_BASE] = {0};
    for (size_t i = 0; i < count; ++i) {
        size_t bucket = (pixels[i] * ZIPRAF_BASE) / 256;
        if (bucket >= ZIPRAF_BASE) bucket = ZIPRAF_BASE - 1;
        ++histogram[bucket];
    }

    BitrafVector projected;
    bitraf_project(histogram, ZIPRAF_BASE, &projected);

    size_t slot = store->frames_ingested % ZIPRAF_BASE;
    store->slot[slot] = projected;
    store->parity[0] = fold_parity(projected.lane, BITRAF_STATES / 2, store->parity[0]);
    store->parity[1] = fold_parity(projected.lane + BITRAF_STATES / 2, BITRAF_STATES / 2, store->parity[1]);
    store->frames_ingested++;
}

static void dump_slot(const ZiprafStore *store, size_t idx) {
    printf("[ZIPRAF] slot %zu → parity0=0x%04X parity1=0x%04X\n",
           idx, store->parity[0], store->parity[1]);
    printf("        head: %u %u %u %u ...\n",
           store->slot[idx].lane[0],
           store->slot[idx].lane[1],
           store->slot[idx].lane[2],
           store->slot[idx].lane[3]);
}

int main(void) {
    /* Exemplo: vetorizar uma imagem sintética 10x10 (100 pixels). */
    ZiprafStore store = {0};
    uint8_t demo_pixels[CELL_VOLUME / 10];

    /* Gera gradiente simples para demonstrar a compressão em 20 buckets. */
    for (size_t i = 0; i < sizeof(demo_pixels); ++i) {
        demo_pixels[i] = (uint8_t)((i * 255) / sizeof(demo_pixels));
    }

    zipraf_absorb_image(&store, demo_pixels, sizeof(demo_pixels));
    dump_slot(&store, 0);

    /* Reaproveita o mesmo buffer com ruído leve para ilustrar estado cumulativo. */
    for (size_t i = 0; i < sizeof(demo_pixels); ++i) {
        demo_pixels[i] ^= (uint8_t)(i * 17); // ruído determinístico
    }

    zipraf_absorb_image(&store, demo_pixels, sizeof(demo_pixels));
    dump_slot(&store, 1);

    return 0;
}
