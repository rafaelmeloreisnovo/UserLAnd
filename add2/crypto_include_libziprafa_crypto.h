/*
 * libziprafa_crypto.h
 * Crypto primitives wrapper (BLAKE3 + Ed25519)
 *
 * Implementation note:
 * - In dev: blake3_fallback + ed25519_fallback
 * - In release: compile with -DZIPRAF_USE_OFFICIAL and provide blake3.c + ed25519_ref10.c
 */

#ifndef LIBZIPRAFA_CRYPTO_H
#define LIBZIPRAFA_CRYPTO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t u8;

/* Blake3 */
int zcrypto_blake3_256(const u8 *data, size_t len, u8 out32[32]);

/* Ed25519 sign/verify */
int zcrypto_sign(const u8 *msg, size_t mlen, const u8 sk[64], u8 sig[64]);
int zcrypto_verify(const u8 sig[64], const u8 *msg, size_t mlen, const u8 pk[32]);

#ifdef __cplusplus
}
#endif

#endif /* LIBZIPRAFA_CRYPTO_H */