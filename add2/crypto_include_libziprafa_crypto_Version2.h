/*
 * crypto/include/libziprafa_crypto.h
 *
 * Public crypto wrapper
 */

#ifndef LIBZIPRAFA_CRYPTO_H
#define LIBZIPRAFA_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t u8;

/* Blake3 wrapper (fallback implemented) */
int zcrypto_blake3_256(const u8 *data, size_t len, u8 out32[32]);

/* Ed25519 sign/verify (low-level impl embedded) */
int zcrypto_ed25519_keypair_from_seed(u8 pk[32], u8 sk[64], const u8 seed[32]);
int zcrypto_ed25519_sign(u8 sig[64], const u8 *m, size_t mlen, const u8 sk[64]);
int zcrypto_ed25519_verify(const u8 sig[64], const u8 *m, size_t mlen, const u8 pk[32]);

#ifdef __cplusplus
}
#endif

#endif