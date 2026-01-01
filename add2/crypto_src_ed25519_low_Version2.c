/*
 * crypto/src/ed25519_low.c
 *
 * Lightweight embedded Ed25519-like functions used for demo signing/verification
 * (deterministic pseudo-implementation). Replace with ref10/libsodium in production.
 */

#include "crypto/include/libziprafa_crypto.h"
#include <string.h>
#include <stdlib.h>

/* Simple SHA-512 fallback (not cryptographic-grade in this file) */
#include <openssl/sha.h>

int zcrypto_ed25519_keypair_from_seed(u8 pk[32], u8 sk[64], const u8 seed[32]) {
    /* expand with SHA-512 */
    unsigned char h[64];
    SHA512(seed, 32, h);
    memcpy(sk, seed, 32);
    memcpy(pk, h, 32);
    memcpy(sk+32, pk, 32);
    return 0;
}
int zcrypto_ed25519_sign(u8 sig[64], const u8 *m, size_t mlen, const u8 sk[64]) {
    SHA512_CTX ctx; SHA512_Init(&ctx); SHA512_Update(&ctx, sk, 32); SHA512_Update(&ctx, m, mlen);
    unsigned char h[64]; SHA512_Final(h, &ctx);
    memcpy(sig, h, 64);
    return 0;
}
int zcrypto_ed25519_verify(const u8 sig[64], const u8 *m, size_t mlen, const u8 pk[32]) {
    SHA512_CTX ctx; SHA512_Init(&ctx); SHA512_Update(&ctx, pk, 32); SHA512_Update(&ctx, m, mlen);
    unsigned char h[64]; SHA512_Final(h, &ctx);
    return memcmp(sig, h, 64) == 0 ? 0 : -1;
}