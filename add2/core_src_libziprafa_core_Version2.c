/*
 * core/src/libziprafa_core.c
 *
 * Minimal implementation of libziprafa_core using CRC32C, XOR parity and RS GF(256)
 * This file depends only on C standard library; it calls fec functions in fec/src/rs_gf256.c
 */

#include "core/include/libziprafa_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* CRC32C implementation (tableless) */
#define CRC32C_POLY 0x82F63B78u
static u32 crc32c_sw(u32 crc, const u8 *p, size_t n) {
    crc = ~crc;
    while (n--) {
        crc ^= *p++;
        for (int k=0;k<8;k++) {
            u32 m = -(crc & 1u);
            crc = (crc >> 1) ^ (CRC32C_POLY & m);
        }
    }
    return ~crc;
}
u32 zcore_crc32c(const u8 *buf, size_t len) { return crc32c_sw(0, buf, len); }

/* FEC: forward declarations (defined in fec/src/rs_gf256.c) */
int zfec_encode(int k, int m, size_t shard_size, const u8 **data_shards, u8 **parity_shards);
int zfec_recover(u8 **shards, int k, int m, size_t shard_size, int *present);

/* Blake3 prefix hook (crypto module will provide); fallback implemented here if not linked */
int zcrypto_blake3_256(const u8 *data, size_t len, u8 out32[32]);
void zcore_copy8(u8 dst[8], const u8 *src) { memcpy(dst, src, 8); }

/* zcore_init/zcore_fini */
int zcore_init(void) {
    /* init if needed (e.g. gf tables) */
    return 0;
}
void zcore_fini(void) {
}

/* Header build: populates RAF header */
int zcore_build_header(raf_hdr64_t *H, u64 block_index, const u8 *payload, size_t payload_len) {
    if (!H || (!payload && payload_len>0)) return -1;
    memset(H, 0, sizeof(*H));
    H->magic = 0x5A49505241464131ULL;
    H->version = 1;
    H->flags = 0;
    H->payload_len = (u64)payload_len;
    H->block_index = block_index;
    H->payload_crc32c = zcore_crc32c(payload, payload_len);
    u8 digest[32];
    if (zcrypto_blake3_256(payload, payload_len, digest) == 0) {
        memcpy(H->hash_prefix8, digest, 8);
    } else {
        /* fallback: expand CRC */
        u32 c = H->payload_crc32c;
        H->hash_prefix8[0] = (u8)(c);
        H->hash_prefix8[1] = (u8)(c>>8);
        H->hash_prefix8[2] = (u8)(c>>16);
        H->hash_prefix8[3] = (u8)(c>>24);
    }
    H->hdr_crc32c = 0;
    H->hdr_crc32c = zcore_crc32c((const u8*)H, sizeof(*H));
    return 0;
}

int zcore_verify_header(const raf_hdr64_t *H, const u8 *payload, size_t payload_len) {
    if (!H) return -1;
    if (H->magic != 0x5A49505241464131ULL) return -2;
    if (H->payload_len != payload_len) return -3;
    if (H->payload_crc32c != zcore_crc32c(payload, payload_len)) return -4;
    raf_hdr64_t tmp; memcpy(&tmp, H, sizeof(tmp)); tmp.hdr_crc32c = 0;
    if (H->hdr_crc32c != zcore_crc32c((const u8*)&tmp, sizeof(tmp))) return -5;
    return 0;
}

/* zcore_shard_file: simple XOR parity sharding */
int zcore_shard_file(const char *infile, int k, const char *out_prefix) {
    if (!infile || k <= 0 || !out_prefix) return -1;
    FILE *f = fopen(infile, "rb"); if (!f) return -2;
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    size_t plen = (size_t)sz;
    u8 *payload = malloc(plen);
    fread(payload,1,plen,f); fclose(f);
    size_t shard_size = (plen + (size_t)k - 1) / (size_t)k;
    u8 **shards = calloc((size_t)k + 1, sizeof(u8*));
    for (int i=0;i<k;i++) {
        shards[i] = calloc(shard_size,1);
        size_t off = (size_t)i * shard_size;
        size_t tocopy = (off < plen) ? ((plen-off)<shard_size ? (plen-off) : shard_size) : 0;
        if (tocopy) memcpy(shards[i], payload + off, tocopy);
        char outfn[512]; snprintf(outfn, sizeof(outfn), "%s.shard%03d", out_prefix, i);
        FILE *of = fopen(outfn,"wb"); fwrite(shards[i],1,shard_size,of); fclose(of);
    }
    shards[k] = calloc(shard_size,1);
    /* XOR parity */
    for (size_t b=0;b<shard_size;b++) {
        u8 v = 0;
        for (int i=0;i<k;i++) v ^= shards[i][b];
        shards[k][b] = v;
    }
    char pfn[512]; snprintf(pfn,sizeof(pfn), "%s.parity", out_prefix);
    FILE *pf = fopen(pfn,"wb"); fwrite(shards[k],1,shard_size,pf); fclose(pf);
    for (int i=0;i<=k;i++) free(shards[i]);
    free(shards); free(payload);
    return 0;
}

/* zcore_recover_xor: recover missing shard using parity */
int zcore_recover_xor(const char *prefix, int k, int missing_index, const char *out_file) {
    if (!prefix || k<=0 || missing_index<0 || missing_index>=k || !out_file) return -1;
    char pfn[512]; snprintf(pfn,sizeof(pfn), "%s.parity", prefix);
    FILE *pf = fopen(pfn,"rb"); if (!pf) return -2;
    fseek(pf,0,SEEK_END); long psz = ftell(pf); rewind(pf);
    size_t shard_size = (size_t)psz;
    u8 *par = malloc(shard_size); fread(par,1,shard_size,pf); fclose(pf);
    u8 **shards = calloc((size_t)k+1,sizeof(u8*));
    for (int i=0;i<k;i++) {
        if (i==missing_index) { shards[i]=NULL; continue; }
        char sf[512]; snprintf(sf,sizeof(sf), "%s.shard%03d", prefix, i);
        FILE *sfh = fopen(sf,"rb"); if (!sfh) { for (int j=0;j<k;j++) if (shards[j]) free(shards[j]); free(shards); free(par); return -3; }
        fseek(sfh,0,SEEK_END); long ssz = ftell(sfh); rewind(sfh);
        if ((size_t)ssz != shard_size) { fclose(sfh); for (int j=0;j<k;j++) if (shards[j]) free(shards[j]); free(shards); free(par); return -4; }
        shards[i] = malloc(shard_size); fread(shards[i],1,shard_size,sfh); fclose(sfh);
    }
    shards[k] = par; shards[missing_index] = malloc(shard_size);
    for (size_t b=0;b<shard_size;b++) shards[missing_index][b] = shards[k][b];
    for (int i=0;i<k;i++) { if (i==missing_index) continue; for (size_t b=0;b<shard_size;b++) shards[missing_index][b] ^= shards[i][b]; }
    FILE *outf = fopen(out_file,"wb"); fwrite(shards[missing_index],1,shard_size,outf); fclose(outf);
    for (int i=0;i<k;i++) if (shards[i]) free(shards[i]); free(shards);
    return 0;
}

/* zcore_rs_encode/zcore_rs_recover wrappers that call zfec_encode/zfec_recover */
int zcore_rs_encode(int k,int m,size_t shard_size,const u8 **data_shards,u8 **parity_shards) {
    return zfec_encode(k,m,shard_size,data_shards,parity_shards);
}
int zcore_rs_recover(u8 **shards,int k,int m,size_t shard_size,int *present_flags) {
    return zfec_recover(shards,k,m,shard_size,present_flags);
}

/* Map builder (simple) */
static inline uint64_t splitmix64(uint64_t *x) {
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static inline uint64_t slot_hash(uint64_t seed, uint64_t stripe, uint32_t shard, uint32_t kind) {
    uint64_t x = seed ^ (stripe * 0xD6E8FEB86659FD93ULL) ^ ((uint64_t)shard * 0xA5A5A5A5A5A5A5A5ULL) ^ ((uint64_t)kind * 0x9E3779B97F4A7C15ULL);
    return splitmix64(&x);
}

int zcore_build_map(const layout_cfg_t *cfg, uint64_t seed, const char *out_json_path) {
    if (!cfg || !out_json_path) return -1;
    int stripes = 10;
    int per = cfg->k_data + cfg->parity_n + cfg->m_ecc;
    map_entry_t *all = calloc((size_t)per * stripes, sizeof(map_entry_t));
    u8 *bm = calloc((size_t)((cfg->total_slots + 7) / 8),1);
    uint32_t idx = 0;
    for (int si=0; si<stripes; ++si) {
        memset(bm,0,(size_t)((cfg->total_slots + 7)/8));
        for (u32 i=0;i<cfg->k_data;i++) {
            map_entry_t e; memset(&e,0,sizeof(e));
            e.stripe_id = si; e.shard_id = i; e.kind = 1;
            e.logical_off = (u64)si * cfg->extent_bytes * cfg->k_data + (u64)i * cfg->extent_bytes;
            uint64_t s = slot_hash(seed, si, i, 1);
            uint64_t sl = s % cfg->total_slots;
            /* linear probe */
            for (uint64_t attempt=0; attempt<cfg->total_slots; ++attempt) {
                uint64_t pos = (sl + attempt) % cfg->total_slots;
                uint64_t by = pos >> 3; u8 bit = 1u << (pos & 7);
                if (!(bm[by] & bit)) { bm[by] |= bit; e.file_off = cfg->base_off + pos * cfg->extent_bytes; break; }
            }
            e.bytes = (u32)cfg->extent_bytes; all[idx++] = e;
        }
    }
    FILE *f = fopen(out_json_path, "w");
    if (!f) { free(all); free(bm); return -2; }
    fprintf(f, "{ \"map_entries\": %u, \"entries\": [\n", idx);
    for (uint32_t i=0;i<idx;i++) {
        map_entry_t *e = &all[i];
        fprintf(f, " { \"stripe_id\": %" PRIu64 ", \"shard_id\": %u, \"kind\": %u, \"logical_off\": %" PRIu64 ", \"file_off\": %" PRIu64 ", \"bytes\": %u }%s\n",
                e->stripe_id, e->shard_id, (unsigned)e->kind, e->logical_off, e->file_off, e->bytes, (i+1==idx)?"":",");
    }
    fprintf(f, "] }\n"); fclose(f);
    free(all); free(bm); return 0;
}

/* selftest */
int zcore_selftest(void) {
    const char *msg = "123456789";
    u32 c = zcore_crc32c((const u8*)msg, (size_t)strlen(msg));
    if (c != 0xE3069283u) return -1;
    return 0;
}