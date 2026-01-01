/*
 * blake3_fallback.c
 *
 * Deterministic fallback "blake3-like" prefix generator.
 * NOT CRYPTOGRAPHIC — only used for deterministic proofs / dev when
 * official blake3 binary/lib is not provided.
 *
 * Exports:
 *  void ziprafa_blake3_prefix16(const u8 *data, size_t len, u8 out16[16]);
 *
 * Replace this file with real blake3.c (and compile with -DZIPRAF_USE_OFFICIAL)
 * to enable true cryptographic identity.
 */

#include "libziprafa_core.h"
#include <string.h>

static inline u32 rol32(u32 x, unsigned r) { return (x << r) | (x >> (32 - r)); }

void ziprafa_blake3_prefix16(const u8 *data, size_t len, u8 out16[16]) {
    /* simple mixing based on CRC32C to produce stable 16 bytes */
    u32 crc = ziprafa_crc32c(data, len);
    u32 a = crc ^ 0x9E3779B9u;
    u32 b = rol32(a * 0x85EBCA6Bu, 13) + 0x7F4A7C15u;
    u32 c = crc;
    u32 d = (a ^ b) + 0x1EDC6F41u;
    memcpy(out16 + 0, &a, 4);
    memcpy(out16 + 4, &b, 4);
    memcpy(out16 + 8, &c, 4);
    memcpy(out16 + 12, &d, 4);
}