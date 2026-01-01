/*
 * ed25519_fallback.c
 *
 * Deterministic "signature" fallback for dev/proofs only.
 * Produces deterministic 64-byte pseudo-signatures using the blake3_fallback prefix.
 * NOT SECURE. Replace with a real ed25519 implementation for production.
 *
 * Exports (simple):
 *   int ed25519_sign(unsigned char *sig_out, const unsigned char *m, size_t mlen, const unsigned char *sk);
 *   int ed25519_verify(const unsigned char *sig, const unsigned char *m, size_t mlen, const unsigned char *pk);
 *
 * Behavior:
 *  - sign: fills sig_out[0..63] with deterministic digest of m & sk using ziprafa_blake3_prefix16-like mixing
 *  - verify: recomputes and compares equality
 */

#include "libziprafa_core.h"
#include <string.h>
#include <stdint.h>

static void pseudo_hash_64(const unsigned char *m, size_t mlen, const unsigned char *key, size_t keylen, unsigned char out[64]) {
    u8 tmp[16];
    ziprafa_blake3_prefix16(m, mlen, tmp);
    /* expand to 64 bytes deterministically */
    for (int i=0;i<64;i++) {
        unsigned char a = tmp[i % 16];
        unsigned char b = (unsigned char)((i < (int)keylen) ? key[i % keylen] : 0xA5);
        out[i] = (unsigned char)( (a ^ b) + (i * 37) );
    }
}

int ed25519_sign(unsigned char *sig_out, const unsigned char *m, size_t mlen, const unsigned char *sk) {
    if (!sig_out) return -1;
    pseudo_hash_64(m, mlen, sk ? sk : (const unsigned char*)"fallback", sk ? 32 : 8, sig_out);
    return 0;
}

int ed25519_verify(const unsigned char *sig, const unsigned char *m, size_t mlen, const unsigned char *pk) {
    unsigned char expected[64];
    pseudo_hash_64(m, mlen, pk ? pk : (const unsigned char*)"fallback", pk ? 32 : 8, expected);
    return (memcmp(expected, sig, 64) == 0) ? 0 : -1;
}