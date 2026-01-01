/*
 * fec/src/rs_gf256.c
 *
 * Compact Reed-Solomon GF(256) encode/recover implementation.
 * Functional and suitable for embedding; not highly optimized.
 */

#include "fec/include/libziprafa_fec.h"
#include <stdlib.h>
#include <string.h>

static uint8_t gf_exp[512];
static uint8_t gf_log[256];
static int gf_inited = 0;
static void gf_init(void) {
    if (gf_inited) return;
    uint8_t x = 1;
    for (int i=0;i<255;i++) { gf_exp[i] = x; gf_log[x]=i; x <<= 1; if (x & 0x100) x ^= 0x11d; }
    for (int i=255;i<512;i++) gf_exp[i] = gf_exp[i-255];
    gf_inited = 1;
}
static inline uint8_t gf_mul(uint8_t a, uint8_t b) {
    if (!a || !b) return 0;
    return gf_exp[gf_log[a] + gf_log[b]];
}
static inline uint8_t gf_div(uint8_t a, uint8_t b) {
    if (!b) return 0;
    if (!a) return 0;
    int v = gf_log[a] - gf_log[b];
    if (v < 0) v += 255;
    return gf_exp[v];
}

static uint8_t *build_vandermonde(int k, int m) {
    gf_init();
    uint8_t *mat = malloc((size_t)k * (size_t)m);
    for (int i=0;i<m;i++) {
        uint8_t alpha = (uint8_t)(i+1);
        for (int j=0;j<k;j++) {
            if (j==0) mat[i*k + j] = 1;
            else mat[i*k + j] = gf_mul(mat[i*k + j - 1], alpha);
        }
    }
    return mat;
}

int zfec_encode(int k, int m, size_t shard_size, const u8 **data_shards, u8 **parity_shards) {
    if (!data_shards || !parity_shards) return -1;
    gf_init();
    uint8_t *mat = build_vandermonde(k,m);
    for (int i=0;i<m;i++) {
        memset(parity_shards[i], 0, shard_size);
        for (int j=0;j<k;j++) {
            uint8_t coef = mat[i*k + j];
            if (coef == 1) {
                for (size_t t=0;t<shard_size;t++) parity_shards[i][t] ^= data_shards[j][t];
            } else if (coef != 0) {
                for (size_t t=0;t<shard_size;t++) parity_shards[i][t] ^= gf_mul(coef, data_shards[j][t]);
            }
        }
    }
    free(mat);
    return 0;
}

/* Solve linear system A * X = B over GF(256) (in-place) */
static int gf_solve(uint8_t *A, int n, uint8_t *B, int rowsize) {
    gf_init();
    for (int i=0;i<n;i++) {
        int piv = -1;
        for (int r=i;r<n;r++) if (A[r*n + i] != 0) { piv = r; break; }
        if (piv < 0) return -1;
        if (piv != i) {
            for (int c=i;c<n;c++){ uint8_t tmp = A[i*n + c]; A[i*n + c] = A[piv*n + c]; A[piv*n + c] = tmp; }
            for (int c=0;c<rowsize;c++){ uint8_t tmp = B[i*rowsize + c]; B[i*rowsize + c] = B[piv*rowsize + c]; B[piv*rowsize + c] = tmp; }
        }
        uint8_t inv = gf_div(1, A[i*n + i]);
        for (int c=i;c<n;c++) A[i*n + c] = gf_mul(A[i*n + c], inv);
        for (int c=0;c<rowsize;c++) B[i*rowsize + c] = gf_mul(B[i*rowsize + c], inv);
        for (int r=0;r<n;r++) {
            if (r==i) continue;
            uint8_t f = A[r*n + i];
            if (f==0) continue;
            for (int c=i;c<n;c++) A[r*n + c] ^= gf_mul(f, A[i*n + c]);
            for (int c=0;c<rowsize;c++) B[r*rowsize + c] ^= gf_mul(f, B[i*rowsize + c]);
        }
    }
    return 0;
}

int zfec_recover(u8 **shards, int k, int m, size_t shard_size, int *present) {
    int total = k + m;
    int missing = 0;
    for (int i=0;i<total;i++) if (!present[i]) missing++;
    if (missing == 0) return 0;
    if (missing > m) return -1;
    int *rows = malloc(sizeof(int) * k);
    int ri = 0;
    for (int i=0;i<total && ri < k;i++) if (present[i]) rows[ri++] = i;
    if (ri < k) { free(rows); return -1; }
    uint8_t *A = malloc((size_t)k * k);
    uint8_t *B = malloc((size_t)k * shard_size);
    /* build generator rows for selected present indices */
    for (int i=0;i<k;i++) {
        int rowidx = rows[i];
        uint8_t alpha = (uint8_t)(rowidx + 1);
        for (int j=0;j<k;j++) {
            if (j==0) A[i*k + j] = 1;
            else A[i*k + j] = gf_mul(A[i*k + j - 1], alpha);
        }
        memcpy(&B[i*shard_size], shards[rowidx], shard_size);
    }
    int rc = gf_solve(A, k, B, (int)shard_size);
    if (rc != 0) { free(rows); free(A); free(B); return -2; }
    for (int j=0;j<k;j++) {
        if (!present[j]) {
            if (!shards[j]) shards[j] = calloc(1, shard_size);
            memcpy(shards[j], &B[j*shard_size], shard_size);
        }
    }
    free(rows); free(A); free(B);
    return 0;
}