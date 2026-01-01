/*
 * libziprafa_core.h
 *
 * Public header for libziprafa_core (core primitives).
 * Include this in CLI / showroom / other consumers.
 */

#ifndef LIBZIPRAFA_CORE_H
#define LIBZIPRAFA_CORE_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint32_t u32;
typedef uint64_t u64;

/* RAF header (64 bytes) */
typedef struct __attribute__((packed)) {
    u64 magic;
    u32 version;
    u32 flags;
    u64 payload_len;
    u64 block_index;
    u32 payload_crc32c;
    u32 hdr_crc32c;
    u8  hash_prefix8[8];
    u8  reserved[16];
} raf_hdr64_t;

/* basic utilities */
u32 ziprafa_crc32c(const u8 *buf, size_t len);
void ziprafa_blake3_prefix16(const u8 *data, size_t len, u8 out16[16]);
int ziprafa_build_header(raf_hdr64_t *H, u64 block_index, const u8 *payload, size_t payload_len);

/* XOR parity */
void ziprafa_xor_parity(u8 *parity, u8 *const *shards, u32 k, size_t shard_bytes);

/* Reed-Solomon (compact) */
u8 *ziprafa_rs_build_matrix(int k, int m);
int ziprafa_rs_encode(int k, int m, size_t shard_size, const u8 **data_shards, u8 **parity_shards);
int ziprafa_rs_recover(u8 **shards, int k, int m, size_t shard_size, int *present); /* wrapper to rs_recover */

/* layout / map builder */
typedef struct {
    u64 stripe_id;
    u32 shard_id;
    u8  kind;
    u64 logical_off;
    u64 file_off;
    u32 bytes;
} map_entry_t;

typedef struct {
    u64 base_off;
    u64 total_slots;
    u64 extent_bytes;
    u32 k_data;
    u32 parity_n;
    u32 m_ecc;
} layout_cfg_t;

void ziprafa_build_stripe_map(const layout_cfg_t *cfg, uint64_t seed, uint64_t stripe_id, uint64_t logical_base_off, map_entry_t *out_entries, uint32_t *out_count, u8 *bitmap);

/* map export (JSON) */
int ziprafa_write_map_json(const char *path, map_entry_t *entries, uint32_t count);

/* selftest */
int ziprafa_selftest(void);

#endif /* LIBZIPRAFA_CORE_H */