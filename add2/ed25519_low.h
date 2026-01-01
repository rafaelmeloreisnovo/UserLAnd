/*
 * ed25519_low.h
 *
 * Minimal, portable Ed25519 implementation header.
 *
 * API:
 *   void ed25519_keypair_from_seed(unsigned char pk[32], unsigned char sk[64], const unsigned char seed[32]);
 *   void ed25519_sign(unsigned char sig[64], const unsigned char *m, size_t mlen, const unsigned char sk[64]);
 *   int  ed25519_verify(const unsigned char sig[64], const unsigned char *m, size_t mlen, const unsigned char pk[32]);
 *
 * Build:
 *   # gcc -O3 -std=c11 ed25519_low.c -o ed25519_low
 *
 * Note:
 *  - This is a compact, dependency-free implementation suitable for embedding.
 *  - For production, replace with an audited library (libsodium / ref10) or run formal review.
 */

#ifndef ED25519_LOW_H
#define ED25519_LOW_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char u8;
typedef unsigned long long u64;

void ed25519_keypair_from_seed(u8 pk[32], u8 sk[64], const u8 seed[32]);
void ed25519_sign(u8 sig[64], const u8 *m, size_t mlen, const u8 sk[64]);
int  ed25519_verify(const u8 sig[64], const u8 *m, size_t mlen, const u8 pk[32]);

/* Quick test harness */
int ed25519_selftest(void); /* returns 0 on success */

#ifdef __cplusplus
}
#endif

#endif /* ED25519_LOW_H */