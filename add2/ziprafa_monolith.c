/*
 * ziprafa_monolith.c
 * Monolithic ZIPRAFA / RAFAELIA utility
 *
 * - Combines:
 *    * RAF CRC/ECC core (crc32c, crc64, header, payload checks, xor-parity)
 *    * ZIPRAFA layout (slot mapping, probe bitmap, map export bin/json)
 *    * Zone layout (contiguous zones)
 *    * BBS-style CLI with ASCII UI, demo pipeline and self-tests
 *
 * Build:
 *   clang -O3 -std=c11 -Wall -Wextra ziprafa_monolith.c -o ziprafa_monolith -lm
 *
 * Usage:
 *   ./ziprafa_monolith
 *
 * The binary is a self-contained tool to build/demo/test the ZIPRAFA pieces
 * produced in this session. It creates/reads files in the current directory.
 *
 * Author: RAFAELIA Copilot (for rafaelmeloreisnovo)
 * Date: 2025-12-31
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>

/* ----------------------------
   Types & common helpers
   ---------------------------- */
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

static void *xmalloc(size_t s) { void *p = malloc(s); if(!p){fprintf(stderr,"malloc failed\n"); exit(1);} return p; }
static int file_exists(const char *p) { struct stat st; return stat(p,&st)==0; }

/* ----------------------------
   CRC32C (Castagnoli) - bitwise
   ---------------------------- */
static inline uint32_t crc32c_update(uint32_t crc, const uint8_t *p, size_t n) {
  crc = ~crc;
  while (n--) {
    crc ^= *p++;
    for (int k = 0; k < 8; k++) {
      uint32_t m = -(crc & 1u);
      crc = (crc >> 1) ^ (0x82F63B78u & m);
    }
  }
  return ~crc;
}
static inline uint32_t crc32c(const void *data, size_t len) {
  return crc32c_update(0u, (const uint8_t*)data, len);
}

/* ----------------------------
   CRC64-ECMA (optional)
   ---------------------------- */
static inline uint64_t crc64_ecma_update(uint64_t crc, const uint8_t *p, size_t n) {
  crc = ~crc;
  while (n--) {
    crc ^= (uint64_t)(*p++);
    for (int k = 0; k < 8; k++) {
      uint64_t m = -(crc & 1ull);
      crc = (crc >> 1) ^ (0xC96C5795D7870F42ull & m);
    }
  }
  return ~crc;
}
static inline uint64_t crc64_ecma(const void *data, size_t len) {
  return crc64_ecma_update(0ull, (const uint8_t*)data, len);
}

/* ----------------------------
   RAF header + payload checks
   ---------------------------- */
#define RAF_HDR_SIZE 64
typedef struct __attribute__((packed)) {
  uint8_t magic[4];      // "RAFB"
  uint8_t ver;
  uint8_t flags;
  uint16_t hdr_len_le;   // 64
  uint64_t session_seed_le;
  uint64_t block_index_le;
  uint64_t payload_len_le;
  uint8_t  payload_prefix8[8];
  uint32_t hdr_crc32c_le;
  uint8_t  pre6[16];
} raf_hdr64_t;
_Static_assert(sizeof(raf_hdr64_t) == RAF_HDR_SIZE, "Header size must be 64");

typedef struct __attribute__((packed)) {
  uint32_t payload_crc32c_le;
  uint64_t payload_crc64_le;
} raf_payload_checks_t;

/* little endian helpers */
static inline uint16_t le16(uint16_t x){ uint16_t t=1; if(*(uint8_t*)&t) return x; return (x>>8)|(x<<8); }
static inline uint32_t le32(uint32_t x){ uint16_t t=1; if(*(uint8_t*)&t) return x; return __builtin_bswap32(x); }
static inline uint64_t le64(uint64_t x){ uint16_t t=1; if(*(uint8_t*)&t) return x; return __builtin_bswap64(x); }

static void raf_hdr_init(raf_hdr64_t *h, uint8_t ver, uint8_t flags, uint64_t seed, uint64_t idx, uint64_t plen, const uint8_t pre6_16[16]){
  h->magic[0]='R';h->magic[1]='A';h->magic[2]='F';h->magic[3]='B';
  h->ver=ver; h->flags=flags; h->hdr_len_le = le16((uint16_t)RAF_HDR_SIZE);
  h->session_seed_le = le64(seed);
  h->block_index_le  = le64(idx);
  h->payload_len_le  = le64(plen);
  memset(h->payload_prefix8,0,8);
  h->hdr_crc32c_le = 0;
  if(pre6_16) memcpy(h->pre6, pre6_16, 16); else memset(h->pre6,0,16);
}
static uint32_t raf_hdr_crc32c(const raf_hdr64_t *h){
  raf_hdr64_t tmp = *h; tmp.hdr_crc32c_le = 0;
  return crc32c(&tmp, sizeof(tmp));
}
static int raf_hdr_finalize_crc(raf_hdr64_t *h){ if(!h) return -1; uint32_t c = raf_hdr_crc32c(h); h->hdr_crc32c_le = le32(c); return 0; }
static int raf_hdr_verify_crc(const raf_hdr64_t *h){ if(!h) return -1; uint32_t want = le32(h->hdr_crc32c_le); uint32_t got = raf_hdr_crc32c(h); return (want==got)?0:-2; }

static void raf_payload_checks(const uint8_t *payload, size_t plen, raf_payload_checks_t *out, int use_crc64){
  uint32_t c32 = crc32c(payload, plen);
  out->payload_crc32c_le = le32(c32);
  if(use_crc64){ uint64_t c64 = crc64_ecma(payload, plen); out->payload_crc64_le = le64(c64); } else out->payload_crc64_le=0;
}
static int raf_payload_verify(const uint8_t *payload, size_t plen, const raf_payload_checks_t *chk, int use_crc64){
  uint32_t want32 = le32(chk->payload_crc32c_le);
  uint32_t got32  = crc32c(payload, plen);
  if(want32 != got32) return -1;
  if(use_crc64){
    uint64_t want64 = le64(chk->payload_crc64_le);
    uint64_t got64  = crc64_ecma(payload, plen);
    if(want64 != got64) return -2;
  }
  return 0;
}

/* ----------------------------
   XOR parity (ECC-lite)
   ---------------------------- */
static void xor_buf(uint8_t *dst, const uint8_t *src, size_t n){ for(size_t i=0;i<n;i++) dst[i]^=src[i]; }

static void parity_xor_make(const uint8_t *const *shards, size_t k, uint8_t *parity, size_t shard_size){
  for(size_t i=0;i<shard_size;i++) parity[i]=0;
  for(size_t s=0;s<k;s++) xor_buf(parity, shards[s], shard_size);
}
static void parity_xor_recover_one(const uint8_t *const *shards, size_t k, size_t missing_i, const uint8_t *parity, uint8_t *out_missing, size_t shard_size){
  for(size_t j=0;j<shard_size;j++) out_missing[j]=parity[j];
  for(size_t i=0;i<k;i++){ if(i==missing_i) continue; xor_buf(out_missing, shards[i], shard_size); }
}

/* ----------------------------
   SplitMix64 & mapping (slot -> extent)
   ---------------------------- */
static inline u64 splitmix64(u64 *x){
  u64 z = (*x += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}
static inline u64 zr_slot(u64 seed, u64 stripe, u32 shard, u8 kind){
  u64 x = seed ^ (stripe * 0xD6E8FEB86659FD93ULL) ^ ((u64)shard * 0xA5A5A5A5A5A5A5A5ULL) ^ ((u64)kind * 0x9E3779B97F4A7C15ULL);
  return splitmix64(&x);
}
static inline u64 extent_off_simple(u64 seed, u64 stripe, u32 shard, u8 kind, u64 base_off, u64 extent_bytes, u64 total_slots){
  u64 s = zr_slot(seed, stripe, shard, kind);
  u64 idx = (total_slots ? (s % total_slots) : 0);
  return base_off + idx * extent_bytes;
}

/* ----------------------------
   ZIPRAFA layout / map structures
   ---------------------------- */
#define ZR_MAGIC 0x4152414650495AULL
#define ZR_VER 1
enum zr_kind : u8 { ZR_EXT_DATA=1, ZR_EXT_PARITY=2, ZR_EXT_ECC=3, ZR_EXT_MAP=4, ZR_EXT_SUPER=5 };

typedef struct __attribute__((packed)) {
  u64 magic;
  u16 ver;
  u16 hdr_bytes;
  u32 flags;
  u64 seed;
  u64 file_bytes;
  u64 extent_bytes;
  u32 k_data;
  u32 m_ecc;
  u32 parity_n;
  u32 map_entries;
  u64 map_off0;
  u64 map_off1;
  u32 super_crc32c;
  u32 reserved;
} zr_super_t;

typedef struct __attribute__((packed)) {
  u64 stripe_id;
  u32 shard_id;
  u8  kind;
  u8  flags;
  u16 reserved;
  u64 logical_off;
  u64 file_off;
  u32 bytes;
  u32 crc32c;
} zr_mapent_t;

/* layout config */
typedef struct {
  u64 base_off;
  u64 total_slots;
  u64 extent_bytes;
  u32 k_data;
  u32 parity_n;
  u32 m_ecc;
} zr_layout_cfg_t;

/* bitmap helpers */
static uint8_t *bitmap_alloc(u64 bits){ size_t bytes = (size_t)((bits+7)/8); uint8_t *b = calloc(bytes,1); return b; }
static inline int bitmap_test(const uint8_t *bm, u64 i){ return (bm[i>>3] >> (i & 7)) & 1; }
static inline void bitmap_set(uint8_t *bm, u64 i){ bm[i>>3] |= (1u << (i & 7)); }

/* allocate slot with linear probe */
static u64 allocate_slot_linear(uint8_t *bm, u64 ts, u64 start_idx, const zr_layout_cfg_t *cfg){
  if(ts==0) return cfg->base_off;
  u64 idx = start_idx % ts;
  for(u64 attempt=0; attempt<ts; ++attempt){
    u64 pos = (idx + attempt) % ts;
    if(!bitmap_test(bm, pos)){
      bitmap_set(bm, pos);
      return cfg->base_off + pos * cfg->extent_bytes;
    }
  }
  /* fallback */
  return cfg->base_off + (idx % ts) * cfg->extent_bytes;
}

/* build stripe map with probe */
static void zr_build_stripe_map_with_probe(const zr_layout_cfg_t *cfg, u64 seed, u64 stripe_id, u64 logical_base_off, zr_mapent_t *out, u32 *out_count, uint8_t *occupancy_bitmap){
  u32 n=0;
  u64 ts = cfg->total_slots;
  if(ts==0){ *out_count=0; return; }
  for(u32 i=0;i<cfg->k_data;i++){
    zr_mapent_t e; memset(&e,0,sizeof(e));
    e.stripe_id = stripe_id; e.shard_id = i; e.kind = ZR_EXT_DATA;
    e.logical_off = logical_base_off + (u64)i * cfg->extent_bytes;
    u64 s = zr_slot(seed, stripe_id, i, ZR_EXT_DATA);
    u64 sl = s % ts;
    e.file_off = allocate_slot_linear(occupancy_bitmap, ts, sl, cfg);
    e.bytes = (u32)cfg->extent_bytes; e.crc32c=0;
    out[n++]=e;
  }
  for(u32 p=0;p<cfg->parity_n;p++){
    zr_mapent_t e; memset(&e,0,sizeof(e));
    e.stripe_id=stripe_id; e.shard_id=p; e.kind=ZR_EXT_PARITY; e.logical_off=0;
    u64 s = zr_slot(seed, stripe_id, p, ZR_EXT_PARITY); u64 sl = s % ts;
    e.file_off = allocate_slot_linear(occupancy_bitmap, ts, sl, cfg);
    e.bytes=(u32)cfg->extent_bytes; e.crc32c=0; out[n++]=e;
  }
  for(u32 m=0;m<cfg->m_ecc;m++){
    zr_mapent_t e; memset(&e,0,sizeof(e));
    e.stripe_id=stripe_id; e.shard_id=m; e.kind=ZR_EXT_ECC; e.logical_off=0;
    u64 s = zr_slot(seed, stripe_id, m, ZR_EXT_ECC); u64 sl = s % ts;
    e.file_off = allocate_slot_linear(occupancy_bitmap, ts, sl, cfg);
    e.bytes=(u32)cfg->extent_bytes; e.crc32c=0; out[n++]=e;
  }
  *out_count = n;
}

/* write map binary */
static int write_map_bin(const char *path, const zr_mapent_t *ents, u32 count){
  FILE *f = fopen(path,"wb"); if(!f) return -1;
  size_t w = fwrite(ents, sizeof(zr_mapent_t), count, f);
  fclose(f); return (w==count)?0:-2;
}

/* write map JSON (simple) */
static int write_map_json(const char *path, const zr_mapent_t *ents, u32 count){
  FILE *f = fopen(path,"w"); if(!f) return -1;
  fprintf(f,"{\"map_entries\":%u,\"entries\":[\n", count);
  for(u32 i=0;i<count;i++){
    const zr_mapent_t *e=&ents[i];
    fprintf(f,"  {\"stripe_id\":%" PRIu64 ",\"shard_id\":%u,\"kind\":%u,\"logical_off\":%" PRIu64 ",\"file_off\":%" PRIu64 ",\"bytes\":%u,\"crc32c\":%u}%s\n",
      e->stripe_id, e->shard_id, (unsigned)e->kind, e->logical_off, e->file_off, e->bytes, e->crc32c,
      (i+1==count)?"":",");
  }
  fprintf(f,"]}\n"); fclose(f); return 0;
}

/* ----------------------------
   Zone layout helpers (contiguous, no-overlap)
   ---------------------------- */
typedef struct { u64 base; u64 bytes; u64 extent_bytes; u64 extent_count; } zr_zone_t;
typedef struct { zr_zone_t map; zr_zone_t data; zr_zone_t parity; zr_zone_t ecc; u32 k_data; u32 p_parity; u32 m_ecc; } zr_layout_t;

static void zr_layout_build(zr_layout_t *L, u64 file_payload_bytes, u64 extent_bytes, u32 k_data, u32 p_parity, u32 m_ecc, u64 map_bytes_reserved){
  L->k_data=k_data; L->p_parity=p_parity; L->m_ecc=m_ecc;
  L->map.base=0; L->map.bytes=map_bytes_reserved; L->map.extent_bytes=extent_bytes; L->map.extent_count = map_bytes_reserved / extent_bytes;
  u64 data_extents = (file_payload_bytes + extent_bytes - 1) / extent_bytes;
  u64 stripes = (data_extents + (u64)k_data - 1) / (u64)k_data;
  u64 data_zone_extents = stripes * (u64)k_data;
  u64 parity_zone_extents = stripes * (u64)p_parity;
  u64 ecc_zone_extents = stripes * (u64)m_ecc;
  L->data.base = L->map.base + L->map.bytes; L->data.extent_bytes=extent_bytes; L->data.extent_count = data_zone_extents; L->data.bytes = data_zone_extents * extent_bytes;
  L->parity.base = L->data.base + L->data.bytes; L->parity.extent_bytes=extent_bytes; L->parity.extent_count = parity_zone_extents; L->parity.bytes = parity_zone_extents * extent_bytes;
  L->ecc.base = L->parity.base + L->parity.bytes; L->ecc.extent_bytes = extent_bytes; L->ecc.extent_count = ecc_zone_extents; L->ecc.bytes = ecc_zone_extents * extent_bytes;
}
static inline u64 zr_off_data(const zr_layout_t *L, u64 stripe, u32 shard){ return L->data.base + (stripe*(u64)L->k_data + (u64)shard) * L->data.extent_bytes; }
static inline u64 zr_off_parity(const zr_layout_t *L, u64 stripe, u32 beat){ return L->parity.base + (stripe*(u64)L->p_parity + (u64)beat) * L->parity.extent_bytes; }
static inline u64 zr_off_ecc(const zr_layout_t *L, u64 stripe, u32 e){ return L->ecc.base + (stripe*(u64)L->m_ecc + (u64)e) * L->ecc.extent_bytes; }

/* ----------------------------
   File helpers
   ---------------------------- */
static uint8_t *read_file(const char *path, size_t *out_len){
  FILE *f = fopen(path,"rb"); if(!f) return NULL;
  if(fseek(f,0,SEEK_END)!=0){ fclose(f); return NULL; }
  long sz = ftell(f); if(sz<0){ fclose(f); return NULL; }
  rewind(f);
  uint8_t *buf = xmalloc((size_t)sz);
  size_t r = fread(buf,1,(size_t)sz,f);
  fclose(f);
  if(r!=(size_t)sz){ free(buf); return NULL; }
  if(out_len) *out_len = r; return buf;
}
static int write_file(const char *path, const uint8_t *buf, size_t len){
  FILE *f=fopen(path,"wb"); if(!f) return -1;
  size_t w=fwrite(buf,1,len,f); fclose(f); return (w==len)?0:-2;
}

/* ----------------------------
   CLI / BBS UI
   ---------------------------- */
static const char *clr_reset = "\x1b[0m";
static const char *clr_bold = "\x1b[1m";
static const char *clr_cyan = "\x1b[36m";
static const char *clr_green = "\x1b[32m";
static const char *clr_yellow = "\x1b[33m";
static const char *clr_red = "\x1b[31m";

static void clear_screen(){ printf("\x1b[2J\x1b[H"); fflush(stdout); }
static void banner(){
  clear_screen();
  printf("%s%s\n", clr_cyan,
"  ____  _  _____  ____  ____  ____  ____   ____   ___   ___ \n"
" /__  \\| |/__  / / __ \\/ __ \\/ ___|/ ___| / ___| / _ \\ / _ \\\n"
"   / /\\ |   / / / / _` |/ _` \\___ \\\\___ \\| |    | | | | | | |\n"
"  / /_| |  / /_| | (_| | (_| |___) |___) | |___ | |_| | |_| |\n"
" /_____| |/____(_)\\__,_|\\__,_|____/____/ \\____| \\___/ \\___/\n");
  printf("%sZIPRAFA MONOLITH - BBS-like CLI (RAFAELIA)%s\n\n", clr_bold, clr_reset);
}

/* pause */
static void pause_prompt(){ printf("%s\nPress Enter to continue...%s", clr_yellow, clr_reset); getchar(); }

/* Self-tests combining various checks */
static int run_selftests_all(void){
  printf("%sRunning self-tests...%s\n", clr_bold, clr_reset);
  /* CRC32C known vector */
  const uint8_t tv[] = "123456789";
  uint32_t c32 = crc32c(tv,9);
  if(c32 != 0xE3069283u){ printf("%sCRC32C vector fail (got 0x%08x)%s\n", clr_red, c32, clr_reset); return -1; }
  printf("%sCRC32C OK%s\n", clr_green, clr_reset);

  uint64_t c64 = crc64_ecma(tv,9);
  if(c64 != 0x6C40DF5F0B497347ull){ printf("%sCRC64 vector fail%s\n", clr_red, clr_reset); return -2; }
  printf("%sCRC64 OK%s\n", clr_green, clr_reset);

  /* parity test */
  const int k = 4;
  size_t shard_size = 1024;
  uint8_t *s0 = xmalloc(shard_size); uint8_t *s1 = xmalloc(shard_size); uint8_t *s2 = xmalloc(shard_size); uint8_t *s3 = xmalloc(shard_size);
  for(size_t i=0;i<shard_size;i++){ s0[i]=(uint8_t)(i&0xff); s1[i]=(uint8_t)((i*3)&0xff); s2[i]=(uint8_t)((i*7)&0xff); s3[i]=(uint8_t)((i*13)&0xff); }
  const uint8_t *shards[] = {s0,s1,s2,s3};
  uint8_t *par = xmalloc(shard_size); parity_xor_make(shards, k, par, shard_size);
  uint8_t *rec = xmalloc(shard_size);
  uint8_t *shards_with_null[4] = {s0,s1,NULL,s3};
  parity_xor_recover_one((const uint8_t *const*)shards_with_null, k, 2, par, rec, shard_size);
  if(memcmp(rec,s2,shard_size)!=0){ printf("%sParity recovery failed%s\n", clr_red, clr_reset); return -3; }
  printf("%sParity recovery OK%s\n", clr_green, clr_reset);
  free(s0); free(s1); free(s2); free(s3); free(par); free(rec);

  /* Mapping probe test */
  zr_layout_cfg_t cfg; cfg.base_off=1024; cfg.total_slots=3; cfg.extent_bytes=4096; cfg.k_data=3; cfg.parity_n=1; cfg.m_ecc=0;
  uint8_t *bm = bitmap_alloc(cfg.total_slots);
  zr_mapent_t *ents = xmalloc((cfg.k_data+cfg.parity_n+cfg.m_ecc) * sizeof(zr_mapent_t));
  u32 outc=0;
  zr_build_stripe_map_with_probe(&cfg, 0x1234abcdULL, 7, 0, ents, &outc, bm);
  int ok=1;
  for(u32 i=0;i<outc;i++) for(u32 j=i+1;j<outc;j++) if(ents[i].file_off==ents[j].file_off) ok=0;
  free(bm); free(ents);
  if(!ok){ printf("%sProbe collision resolution failed%s\n", clr_red, clr_reset); return -4; }
  printf("%sProbe mapping OK%s\n", clr_green, clr_reset);

  printf("%sAll self-tests passed.%s\n", clr_bold, clr_reset);
  return 0;
}

/* create demo payload */
static int create_demo_payload(const char *path, size_t bytes){
  FILE *f = fopen(path,"wb"); if(!f){ perror("fopen"); return -1; }
  FILE *ur = fopen("/dev/urandom","rb");
  if(!ur){
    /* fallback pseudo-rand */
    uint8_t *buf = xmalloc(bytes);
    for(size_t i=0;i<bytes;i++) buf[i] = (uint8_t)(rand()&0xff);
    fwrite(buf,1,bytes,f); free(buf); fclose(f); return 0;
  }
  size_t w = 0;
  uint8_t buf[4096];
  while(w < bytes){
    size_t to = (bytes-w > sizeof(buf))? sizeof(buf) : (bytes-w);
    size_t r = fread(buf,1,to,ur);
    if(r==0) break;
    fwrite(buf,1,r,f); w+=r;
  }
  fclose(ur); fclose(f);
  return 0;
}

/* create shards (k) and parity */
static int create_shards_from_file(const char *inpath, int k, const char *out_prefix){
  size_t plen; uint8_t *payload = read_file(inpath, &plen);
  if(!payload){ fprintf(stderr,"read_file failed\n"); return -1; }
  size_t shard_size = (plen + (size_t)k - 1) / (size_t)k;
  uint8_t **shards = xmalloc(sizeof(uint8_t*) * k);
  for(int i=0;i<k;i++){
    shards[i] = calloc(1, shard_size);
    size_t off = (size_t)i * shard_size;
    size_t tocopy = (off < plen) ? ((plen - off) < shard_size ? (plen - off) : shard_size) : 0;
    if(tocopy) memcpy(shards[i], payload + off, tocopy);
    char fname[512]; snprintf(fname,sizeof(fname), "%s.shard%03d", out_prefix, i);
    write_file(fname, shards[i], shard_size);
  }
  uint8_t *par = calloc(1, shard_size);
  const uint8_t *const *arr = (const uint8_t *const *)shards;
  parity_xor_make(arr, (size_t)k, par, shard_size);
  char pfn[512]; snprintf(pfn,sizeof(pfn), "%s.parity", out_prefix);
  write_file(pfn, par, shard_size);
  for(int i=0;i<k;i++) free(shards[i]); free(shards); free(par); free(payload);
  printf("%sCreated %d shards + parity (prefix=%s)%s\n", clr_green, k, out_prefix, clr_reset);
  return 0;
}

/* recover missing shard */
static int recover_shard(const char *prefix, int k, int missing_i, const char *out_recovered){
  char pfn[512]; snprintf(pfn,sizeof(pfn), "%s.parity", prefix);
  size_t shard_size; uint8_t *par = read_file(pfn,&shard_size); if(!par){ fprintf(stderr,"parity read failed\n"); return -1; }
  uint8_t **shards = xmalloc(sizeof(uint8_t*) * k);
  for(int i=0;i<k;i++){
    if(i==missing_i){ shards[i]=NULL; continue; }
    char fname[512]; snprintf(fname,sizeof(fname), "%s.shard%03d", prefix, i);
    size_t r; shards[i] = read_file(fname, &r);
    if(!shards[i] || r!=shard_size){ fprintf(stderr,"shard %d read failed\n", i); for(int j=0;j<k;j++) if(shards[j]) free(shards[j]); free(shards); free(par); return -2; }
  }
  uint8_t *rec = calloc(1, shard_size);
  const uint8_t *const *arr = (const uint8_t *const *)shards;
  parity_xor_recover_one(arr, (size_t)k, (size_t)missing_i, par, rec, shard_size);
  write_file(out_recovered, rec, shard_size);
  for(int i=0;i<k;i++) if(shards[i]) free(shards[i]);
  free(shards); free(par); free(rec);
  printf("%sRecovered shard %d -> %s%s\n", clr_green, missing_i, out_recovered, clr_reset);
  return 0;
}

/* generate map (non-interactive): writes prefix.map.bin and prefix.map.json */
static int generate_map_noninteractive(const char *prefix, u64 seed, int stripes, u64 base_off, u64 total_slots, u64 extent_bytes, int k_data, int parity_n, int m_ecc){
  zr_layout_cfg_t cfg = { base_off, total_slots, extent_bytes, (u32)k_data, (u32)parity_n, (u32)m_ecc };
  u32 perstripe = cfg.k_data + cfg.parity_n + cfg.m_ecc;
  zr_mapent_t *all = xmalloc((size_t)perstripe * (size_t)stripes * sizeof(zr_mapent_t));
  uint8_t *bm = bitmap_alloc(cfg.total_slots);
  u32 idx = 0;
  for(int si=0; si<stripes; si++){
    memset(bm,0,(size_t)((cfg.total_slots+7)/8));
    u32 outc=0;
    zr_build_stripe_map_with_probe(&cfg, seed, (u64)si, (u64)si * cfg.extent_bytes * cfg.k_data, &all[idx], &outc, bm);
    idx += outc;
  }
  char bpath[512], jpath[512];
  snprintf(bpath,sizeof(bpath), "%s.map.bin", prefix);
  snprintf(jpath,sizeof(jpath), "%s.map.json", prefix);
  int rc1 = write_map_bin(bpath, all, idx);
  int rc2 = write_map_json(jpath, all, idx);
  free(all); free(bm);
  printf("%sWrote map: %s (bin:%d json:%d)%s\n", clr_green, prefix, rc1==0, rc2==0, clr_reset);
  return (rc1==0 && rc2==0)?0:-1;
}

/* write superblock */
static int write_superblock_file(const char *path, zr_super_t *s){
  s->super_crc32c = 0;
  uint32_t c = crc32c(s, sizeof(zr_super_t));
  s->super_crc32c = c;
  FILE *f = fopen(path,"wb"); if(!f) return -1;
  fwrite(s, sizeof(zr_super_t), 1, f); fclose(f); return 0;
}

/* CLI main loop */
static void main_loop(void){
  while(1){
    banner();
    printf("%sMenu:%s\n", clr_bold, clr_reset);
    printf(" 1) Self-tests (crc/parity/map)\n");
    printf(" 2) Create demo payload (16 KiB)\n");
    printf(" 3) Create shards from payload (k)\n");
    printf(" 4) Simulate missing shard & recover\n");
    printf(" 5) Generate MAP (non-interactive)\n");
    printf(" 6) Export zone layout JSON (contiguous zones)\n");
    printf(" 7) Run demo pipeline (auto build-run demo)\n");
    printf(" 0) Exit\n");
    printf("%sChoice:%s ", clr_yellow, clr_reset);
    int c = getchar();
    while(c=='\n') c = getchar();
    int ch = c - '0';
    while(getchar()!= '\n'); /* consume rest line */
    if(ch==0) { printf("%sBye.%s\n", clr_green, clr_reset); break; }
    if(ch==1){
      run_selftests_all();
      pause_prompt();
    } else if(ch==2){
      const char *p = "demo_payload.bin";
      if(create_demo_payload(p, 16*1024)==0) printf("%sCreated %s%s\n", clr_green, p, clr_reset);
      pause_prompt();
    } else if(ch==3){
      char in[256]; int k;
      printf("Input payload path [demo_payload.bin]: "); if(!fgets(in,sizeof(in),stdin)) break;
      if(in[0]=='\n') strncpy(in,"demo_payload.bin\n",sizeof(in));
      in[strcspn(in,"\n")] = 0;
      printf("Number of shards k [6]: "); char sk[32]; if(!fgets(sk,sizeof(sk),stdin)) break;
      if(sk[0]=='\n') strcpy(sk,"6\n");
      k = atoi(sk);
      char prefix[256]; printf("Output prefix [demo_payload]: "); if(!fgets(prefix,sizeof(prefix),stdin)) break;
      if(prefix[0]=='\n') strncpy(prefix,"demo_payload\n",sizeof(prefix));
      prefix[strcspn(prefix,"\n")] = 0;
      create_shards_from_file(in,k,prefix);
      pause_prompt();
    } else if(ch==4){
      char prefix[256]; printf("Shard set prefix [demo_payload]: "); if(!fgets(prefix,sizeof(prefix),stdin)) break;
      if(prefix[0]=='\n') strncpy(prefix,"demo_payload\n",sizeof(prefix));
      prefix[strcspn(prefix,"\n")] = 0;
      char ks[16]; printf("Total shards k [6]: "); if(!fgets(ks,sizeof(ks),stdin)) break;
      if(ks[0]=='\n') strcpy(ks,"6\n"); int k = atoi(ks);
      char mi[16]; printf("Missing index [2]: "); if(!fgets(mi,sizeof(mi),stdin)) break;
      if(mi[0]=='\n') strcpy(mi,"2\n"); int missing = atoi(mi);
      char outp[256]; printf("Output recovered [recovered.shard]: "); if(!fgets(outp,sizeof(outp),stdin)) break;
      if(outp[0]=='\n') strncpy(outp,"recovered.shard\n",sizeof(outp));
      outp[strcspn(outp,"\n")] = 0;
      recover_shard(prefix,k,missing,outp);
      pause_prompt();
    } else if(ch==5){
      char prefix[128]; printf("Map prefix [demo]: "); if(!fgets(prefix,sizeof(prefix),stdin)) break; if(prefix[0]=='\n') strncpy(prefix,"demo\n",sizeof(prefix)); prefix[strcspn(prefix,"\n")] = 0;
      char seeds[32]; printf("Seed (0=time) [0]: "); if(!fgets(seeds,sizeof(seeds),stdin)) break; if(seeds[0]=='\n') strcpy(seeds,"0\n");
      u64 seed = (u64)strtoull(seeds,NULL,10); if(seed==0) seed = (u64)time(NULL);
      char sstr[32]; printf("Stripes [10]: "); if(!fgets(sstr,sizeof(sstr),stdin)) break; if(sstr[0]=='\n') strcpy(sstr,"10\n"); int stripes = atoi(sstr);
      char eb[32]; printf("extent_bytes [4096]: "); if(!fgets(eb,sizeof(eb),stdin)) break; if(eb[0]=='\n') strcpy(eb,"4096\n"); u64 extent_bytes = (u64)strtoull(eb,NULL,10);
      char ts[32]; printf("total_slots [1000]: "); if(!fgets(ts,sizeof(ts),stdin)) break; if(ts[0]=='\n') strcpy(ts,"1000\n"); u64 total_slots = (u64)strtoull(ts,NULL,10);
      char bo[32]; printf("base_off [1024]: "); if(!fgets(bo,sizeof(bo),stdin)) break; if(bo[0]=='\n') strcpy(bo,"1024\n"); u64 base_off = (u64)strtoull(bo,NULL,10);
      char kd[16]; printf("k_data [6]: "); if(!fgets(kd,sizeof(kd),stdin)) break; if(kd[0]=='\n') strcpy(kd,"6\n"); int k_data = atoi(kd);
      char pn[16]; printf("parity_n [1]: "); if(!fgets(pn,sizeof(pn),stdin)) break; if(pn[0]=='\n') strcpy(pn,"1\n"); int parity_n = atoi(pn);
      char me[16]; printf("m_ecc [0]: "); if(!fgets(me,sizeof(me),stdin)) break; if(me[0]=='\n') strcpy(me,"0\n"); int m_ecc = atoi(me);
      generate_map_noninteractive(prefix, seed, stripes, base_off, total_slots, extent_bytes, k_data, parity_n, m_ecc);
      pause_prompt();
    } else if(ch==6){
      char out[128]; printf("Output JSON [zone_layout.json]: "); if(!fgets(out,sizeof(out),stdin)) break; if(out[0]=='\n') strncpy(out,"zone_layout.json\n",sizeof(out)); out[strcspn(out,"\n")]=0;
      char pbytes[64]; printf("Payload bytes [10485760]: "); if(!fgets(pbytes,sizeof(pbytes),stdin)) break; if(pbytes[0]=='\n') strncpy(pbytes,"10485760\n",sizeof(pbytes)); u64 payload = (u64)strtoull(pbytes,NULL,10);
      char ebs[32]; printf("extent bytes [4096]: "); if(!fgets(ebs,sizeof(ebs),stdin)) break; if(ebs[0]=='\n') strncpy(ebs,"4096\n",sizeof(ebs)); u64 extent = (u64)strtoull(ebs,NULL,10);
      char kd[16]; printf("k_data [6]: "); if(!fgets(kd,sizeof(kd),stdin)) break; if(kd[0]=='\n') strcpy(kd,"6\n"); int k = atoi(kd);
      char pn[16]; printf("parity [1]: "); if(!fgets(pn,sizeof(pn),stdin)) break; if(pn[0]=='\n') strcpy(pn,"1\n"); int p = atoi(pn);
      char mecc[16]; printf("m_ecc [0]: "); if(!fgets(mecc,sizeof(mecc),stdin)) break; if(mecc[0]=='\n') strcpy(mecc,"0\n"); int m = atoi(mecc);
      zr_layout_t L; zr_layout_build(&L, payload, extent, k, p, m, 4096*4);
      FILE *f = fopen(out,"w"); if(!f) { perror("fopen"); } else {
        fprintf(f,"{\"map_base\":%" PRIu64 ",\"data_base\":%" PRIu64 ",\"parity_base\":%" PRIu64 ",\"ecc_base\":%" PRIu64 ",\"total_reserved\":%" PRIu64 "}\n",
                L.map.base, L.data.base, L.parity.base, L.ecc.base, L.ecc.base + L.ecc.bytes);
        fclose(f); printf("%sWrote %s%s\n", clr_green, out, clr_reset);
      }
      pause_prompt();
    } else if(ch==7){
      /* Demo: build not needed (single binary). Run full demo pipeline */
      printf("%sRunning demo pipeline...%s\n", clr_bold, clr_reset);
      const char *outdir = "demo_out";
      mkdir(outdir, 0755);
      char demo_payload[256]; snprintf(demo_payload,sizeof(demo_payload), "%s/demo_payload.bin", outdir);
      create_demo_payload(demo_payload, 16*1024);
      create_shards_from_file(demo_payload, 6, "demo_out/demo_payload");
      /* remove shard 002 to simulate loss */
      remove("demo_out/demo_payload.shard002");
      recover_shard("demo_out/demo_payload", 6, 2, "demo_out/demo_payload.recovered");
      generate_map_noninteractive("demo_out/demo", (u64)time(NULL), 10, 1024, 1000, 4096, 6, 1, 0);
      printf("%sDemo finished. See demo_out/%s%s\n", clr_green, "", clr_reset);
      pause_prompt();
    } else {
      printf("%sUnknown option%s\n", clr_red, clr_reset);
      pause_prompt();
    }
  }
}

/* ----------------------------
   main
   ---------------------------- */
int main(int argc, char **argv){
  srand((unsigned)time(NULL) ^ (unsigned)getpid());
  main_loop();
  return 0;
}