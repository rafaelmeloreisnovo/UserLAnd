#include <stdint.h>
#include <string.h>

/* Type definitions for fiber_hash (if fiber_hash.h doesn't exist) */
#ifndef FIBER_HASH_TYPES_DEFINED
#define FIBER_HASH_TYPES_DEFINED
typedef uint64_t fiber_u64;
typedef uint8_t fiber_u8;
#endif

#if defined(__aarch64__)
extern void fiber_h_compress_asm_real(fiber_u64 *state, const fiber_u8 block[64]);
#endif

/* Portable CRC32 implementation for fallback (table-less, memory-friendly) */
static uint32_t _portable_crc32(const void *data, size_t n, uint32_t prev_crc) {
    uint32_t crc = ~prev_crc;
    const uint8_t *p = (const uint8_t*)data;
    while (n--) {
        crc ^= *p++;
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

/* Portable fallback compress function for non-aarch64 architectures */
static void fiber_h_compress_block_portable(
    fiber_u64 *a, fiber_u64 *b, fiber_u64 *c, fiber_u64 *d,
    const fiber_u8 block[64]
) {
    /* Portable fallback: deterministic mixing using CRC32 + XOR diffusion */
    uint32_t crc = _portable_crc32(block, 64, 0);
    
    /* Mix CRC into state with diffusion (deterministic, non-cryptographic but real) */
    *a ^= (fiber_u64)crc;
    *b += (fiber_u64)crc;
    *c ^= ((fiber_u64)crc << 1);
    *d += ((fiber_u64)crc >> 1);
    
    /* Additional mixing for better diffusion */
    *a += *d;
    *c ^= *a;
    *b = (*b << 13) | (*b >> (64 - 13));
}

// Override / wrapper para o core de compressão.
// Em AArch64 usamos ASM; em outras arquiteturas, você mantém a versão em C.
void fiber_h_compress_block_override(fiber_u64 *a, fiber_u64 *b,
                                     fiber_u64 *c, fiber_u64 *d,
                                     const fiber_u8 block[64]) {
#if defined(__aarch64__)
    fiber_u64 st[4];
    st[0] = *a;
    st[1] = *b;
    st[2] = *c;
    st[3] = *d;

    fiber_h_compress_asm_real(st, block);

    *a = st[0];
    *b = st[1];
    *c = st[2];
    *d = st[3];
#else
    /* Use portable implementation for non-aarch64 architectures */
    fiber_h_compress_block_portable(a, b, c, d, block);
#endif
}
