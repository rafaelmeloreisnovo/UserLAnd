/*
 * fec/include/libziprafa_fec.h
 *
 * Simple public API for RS GF(256) FEC module
 */

#ifndef LIBZIPRAFA_FEC_H
#define LIBZIPRAFA_FEC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t u8;

/* encode k data shards -> m parity shards */
int zfec_encode(int k, int m, size_t shard_size, const u8 **data_shards, u8 **parity_shards);

/* recover up to m missing shards (shards array length = k+m) */
int zfec_recover(u8 **shards, int k, int m, size_t shard_size, int *present);

#ifdef __cplusplus
}
#endif

#endif