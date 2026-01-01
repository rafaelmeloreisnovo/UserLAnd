/*
 * libziprafa_fec.h
 * Public API for FEC (Reed-Solomon GF(256))
 */

#ifndef LIBZIPRAFA_FEC_H
#define LIBZIPRAFA_FEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t u8;

/* encode: data_shards pointers -> parity_shards allocated by caller */
int zfec_encode(int k, int m, size_t shard_size, const u8 **data_shards, u8 **parity_shards);

/* recover: shards array length = k+m, present[i]=1 if present. Reconstruct missing into provided buffers (or allocate) */
int zfec_recover(u8 **shards, int k, int m, size_t shard_size, int *present);

#ifdef __cplusplus
}
#endif

#endif /* LIBZIPRAFA_FEC_H */