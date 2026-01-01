/*
 * core/include/libziprafa_core.h
 *
 * Public API for libziprafa_core
 */

#ifndef LIBZIPRAFA_CORE_H
#define LIBZIPRAFA_CORE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;

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

typedef struct {
    u64 base_off;
    u64 total_slots;
    u64 extent_bytes;
    u32 k_data;
    u32 parity_n;
    u32 m_ecc;
} layout_cfg_t;

typedef struct {
    u64 stripe_id;
    u32 shard_id;
    u8  kind;
    u64 logical_off;
    u64 file_off;
    u32 bytes;
} map_entry_t;

/* Initialization */
int zcore_init(void);
void zcore_fini(void);

/* Header */
int zcore_build_header(raf_hdr64_t *H, u64 block_index, const u8 *payload, size_t payload_len);
int zcore_verify_header(const raf_hdr64_t *H, const u8 *payload, size_t payload_len);

/* Sharding/parity */
int zcore_shard_file(const char *infile, int k, const char *out_prefix);
int zcore_recover_xor(const char *prefix, int k, int missing_index, const char *out_file);

/* Reed-Solomon wrappers */
int zcore_rs_encode(int k, int m, size_t shard_size, const u8 **data_shards, u8 **parity_shards);
int zcore_rs_recover(u8 **shards, int k, int m, size_t shard_size, int *present_flags);

/* Map builder */
int zcore_build_map(const layout_cfg_t *cfg, uint64_t seed, const char *out_json_path);

/* Self-test */
int zcore_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBZIPRAFA_CORE_H */