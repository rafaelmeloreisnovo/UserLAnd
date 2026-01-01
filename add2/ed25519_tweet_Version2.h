/*
 * ed25519_tweet.h
 *
 * Minimal Ed25519 interface (TweetNaCl-style compact implementation).
 * Public-domain / permissive reference suitable for embedding in demos.
 *
 * Functions:
 *  - void ed25519_keypair_from_seed(unsigned char *pk, unsigned char *sk, const unsigned char *seed);
 *  - void ed25519_sign(unsigned char *sig, const unsigned char *m, size_t mlen, const unsigned char *sk);
 *  - int  ed25519_verify(const unsigned char *sig, const unsigned char *m, size_t mlen, const unsigned char *pk);
 *
 * Notes:
 *  - sk is 64 bytes (private scalar + public key appended) as common in many libs.
 *  - pk is 32 bytes.
 *
 * For production: replace with audited library (libsodium / ref10).
 */

#ifndef ED25519_TWEET_H
#define ED25519_TWEET_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void ed25519_keypair_from_seed(unsigned char pk[32], unsigned char sk[64], const unsigned char seed[32]);
void ed25519_sign(unsigned char sig[64], const unsigned char *m, size_t mlen, const unsigned char sk[64]);
int  ed25519_verify(const unsigned char sig[64], const unsigned char *m, size_t mlen, const unsigned char pk[32]);

#ifdef __cplusplus
}
#endif

#endif /* ED25519_TWEET_H */