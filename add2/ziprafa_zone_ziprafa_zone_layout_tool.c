/*
 * ziprafa_zone_layout_tool.c
 * ZIPRAFA :: zone layout helper (CLI)
 *
 * Purpose:
 *  - Provide a small CLI around the header-only logical core (zr_layout_t)
 *  - Compute contiguous zones ([map][data][parity][ecc])
 *  - Print human-readable ASCII summary
 *  - Export zones to JSON
 *  - Compute offsets for data/parity/ecc and validate no-overlap
 *  - Self-tests and a tiny benchmark
 *
 * Build:
 *   clang -O3 -std=c11 -Wall -Wextra ziprafa_zone_layout_tool.c -o ziprafa_zone_layout_tool -lm
 *
 * Author: RAFAELIA Copilot (requested by rafaelmeloreisnovo)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <math.h>

typedef uint64_t u64;
typedef uint32_t u32;

/* zone core (same semantics as your header) */
typedef struct {
  u64 base;
  u64 bytes;
  u64 extent_bytes;
  u64 extent_count;
} zr_zone_t;

typedef struct {
  zr_zone_t map;     // super+map area (usually fixed, duplicated)
  zr_zone_t data;
  zr_zone_t parity;
  zr_zone_t ecc;
  u32 k_data;
  u32 p_parity;  // e.g. 3 beats
  u32 m_ecc;
} zr_layout_t;

/* Compute zones contiguously: [map][data][parity][ecc] */
static inline void zr_layout_build(
  zr_layout_t *L,
  u64 file_payload_bytes,
  u64 extent_bytes,
  u32 k_data,
  u32 p_parity,
  u32 m_ecc,
  u64 map_bytes_reserved
){
  L->k_data = k_data;
  L->p_parity = p_parity;
  L->m_ecc = m_ecc;

  // map zone
  L->map.base = 0;
  L->map.bytes = map_bytes_reserved;
  L->map.extent_bytes = extent_bytes;
  L->map.extent_count = (map_bytes_reserved / extent_bytes);

  // how many data extents needed
  u64 data_extents = (file_payload_bytes + extent_bytes - 1) / extent_bytes;

  // stripes = ceil(data_extents / k_data)
  u64 stripes = (data_extents + (u64)k_data - 1) / (u64)k_data;

  // zone sizes in extents
  u64 data_zone_extents   = stripes * (u64)k_data;     // pad last stripe
  u64 parity_zone_extents = stripes * (u64)p_parity;
  u64 ecc_zone_extents    = stripes * (u64)m_ecc;

  // data zone
  L->data.base = L->map.base + L->map.bytes;
  L->data.extent_bytes = extent_bytes;
  L->data.extent_count = data_zone_extents;
  L->data.bytes = data_zone_extents * extent_bytes;

  // parity zone
  L->parity.base = L->data.base + L->data.bytes;
  L->parity.extent_bytes = extent_bytes;
  L->parity.extent_count = parity_zone_extents;
  L->parity.bytes = parity_zone_extents * extent_bytes;

  // ecc zone
  L->ecc.base = L->parity.base + L->parity.bytes;
  L->ecc.extent_bytes = extent_bytes;
  L->ecc.extent_count = ecc_zone_extents;
  L->ecc.bytes = ecc_zone_extents * extent_bytes;
}

/* Offsets are collision-free by construction */
static inline u64 zr_off_data (const zr_layout_t *L, u64 stripe, u32 shard){
  return L->data.base + (stripe*(u64)L->k_data + (u64)shard) * L->data.extent_bytes;
}
static inline u64 zr_off_parity(const zr_layout_t *L, u64 stripe, u32 beat){
  return L->parity.base + (stripe*(u64)L->p_parity + (u64)beat) * L->parity.extent_bytes;
}
static inline u64 zr_off_ecc  (const zr_layout_t *L, u64 stripe, u32 e){
  return L->ecc.base + (stripe*(u64)L->m_ecc + (u64)e) * L->ecc.extent_bytes;
}

/* Pretty print layout */
static void print_layout(const zr_layout_t *L) {
  puts("ZIPRAFA ZONE LAYOUT");
  puts("-------------------");
  printf("k_data: %u, parity beats: %u, m_ecc: %u\n", L->k_data, L->p_parity, L->m_ecc);
  puts("");
  printf("MAP   : base=%" PRIu64 " bytes=%" PRIu64 " extent=%" PRIu64 " count=%" PRIu64 "\n",
         L->map.base, L->map.bytes, L->map.extent_bytes, L->map.extent_count);
  printf("DATA  : base=%" PRIu64 " bytes=%" PRIu64 " extent=%" PRIu64 " count=%" PRIu64 "\n",
         L->data.base, L->data.bytes, L->data.extent_bytes, L->data.extent_count);
  printf("PARITY: base=%" PRIu64 " bytes=%" PRIu64 " extent=%" PRIu64 " count=%" PRIu64 "\n",
         L->parity.base, L->parity.bytes, L->parity.extent_bytes, L->parity.extent_count);
  printf("ECC   : base=%" PRIu64 " bytes=%" PRIu64 " extent=%" PRIu64 " count=%" PRIu64 "\n",
         L->ecc.base, L->ecc.bytes, L->ecc.extent_bytes, L->ecc.extent_count);
  puts("");
  printf("Total reserved bytes (file image): %" PRIu64 "\n",
         L->ecc.base + L->ecc.bytes);
}

/* Export layout to JSON file */
static int layout_to_json(const zr_layout_t *L, const char *path) {
  FILE *f = fopen(path, "w");
  if (!f) return -1;
  fprintf(f, "{\n");
  fprintf(f, "  \"k_data\": %u,\n", L->k_data);
  fprintf(f, "  \"p_parity\": %u,\n", L->p_parity);
  fprintf(f, "  \"m_ecc\": %u,\n", L->m_ecc);
  fprintf(f, "  \"map\": { \"base\": %" PRIu64 ", \"bytes\": %" PRIu64 ", \"extent_bytes\": %" PRIu64 ", \"extent_count\": %" PRIu64 " },\n",
          L->map.base, L->map.bytes, L->map.extent_bytes, L->map.extent_count);
  fprintf(f, "  \"data\": { \"base\": %" PRIu64 ", \"bytes\": %" PRIu64 ", \"extent_bytes\": %" PRIu64 ", \"extent_count\": %" PRIu64 " },\n",
          L->data.base, L->data.bytes, L->data.extent_bytes, L->data.extent_count);
  fprintf(f, "  \"parity\": { \"base\": %" PRIu64 ", \"bytes\": %" PRIu64 ", \"extent_bytes\": %" PRIu64 ", \"extent_count\": %" PRIu64 " },\n",
          L->parity.base, L->parity.bytes, L->parity.extent_bytes, L->parity.extent_count);
  fprintf(f, "  \"ecc\": { \"base\": %" PRIu64 ", \"bytes\": %" PRIu64 ", \"extent_bytes\": %" PRIu64 ", \"extent_count\": %" PRIu64 " }\n",
          L->ecc.base, L->ecc.bytes, L->ecc.extent_bytes, L->ecc.extent_count);
  fprintf(f, "}\n");
  fclose(f);
  return 0;
}

/* Validate no overlap among zones and sizes consistent */
static int validate_layout(const zr_layout_t *L) {
  u64 map_end = L->map.base + L->map.bytes;
  u64 data_end = L->data.base + L->data.bytes;
  u64 parity_end = L->parity.base + L->parity.bytes;
  u64 ecc_end = L->ecc.base + L->ecc.bytes;

  if (L->map.base != 0) return -1;
  if (map_end != L->data.base) return -2;
  if (L->data.base + L->data.bytes != L->parity.base) return -3;
  if (L->parity.base + L->parity.bytes != L->ecc.base) return -4;
  if (L->ecc.base + L->ecc.bytes < L->ecc.base) return -5; // overflow
  return 0;
}

/* Simple self-tests */
static int selftest(void) {
  puts("Running ziprafa_zone_layout self-test...");

  zr_layout_t L;
  zr_layout_build(&L, /*payload*/ 10ULL*1024ULL*1024ULL, /*extent*/ 4096, /*k*/ 6, /*parity*/ 2, /*ecc*/ 1, /*map_bytes_reserved*/ 4096*4);
  if (validate_layout(&L) != 0) { puts("Validation failed"); return -1; }

  /* compute some offsets and verify monotonicity */
  u64 last = 0;
  u64 stripes = (L.data.extent_count + L.k_data - 1) / L.k_data;
  for (u64 s=0; s<stripes; s++) {
    for (u32 sh=0; sh < L.k_data; sh++) {
      u64 off = zr_off_data(&L, s, sh);
      if (off < last) { printf("Monotonicity fail at stripe %" PRIu64 " shard %u\n", s, sh); return -2; }
      last = off;
    }
  }
  puts("Self-test OK");
  return 0;
}

/* Benchmark layout build speed for N iterations */
static void bench(u64 payload_bytes, u64 extent_bytes, u32 k, u32 p, u32 m, u64 map_bytes, int iter) {
  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  for (int i=0;i<iter;i++) {
    zr_layout_t L;
    zr_layout_build(&L, payload_bytes, extent_bytes, k, p, m, map_bytes);
    (void)L;
  }
  clock_gettime(CLOCK_MONOTONIC, &t1);
  double secs = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;
  printf("Built layout %d times in %.6f s (%.2f builds/s)\n", iter, secs, iter / secs);
}

/* Print usage */
static void usage(const char *p) {
  printf("Usage: %s [options]\n", p);
  puts("Options:");
  puts("  --print payload_bytes extent_bytes k p m map_bytes");
  puts("  --json out.json payload_bytes extent_bytes k p m map_bytes");
  puts("  --offsets payload_bytes extent_bytes k p m map_bytes stripe shard");
  puts("  --selftest");
  puts("  --bench payload_bytes extent_bytes k p m map_bytes iterations");
}

/* main */
int main(int argc, char **argv) {
  if (argc < 2) { usage(argv[0]); return 1; }

  if (strcmp(argv[1], "--selftest") == 0) {
    return selftest() == 0 ? 0 : 2;
  } else if (strcmp(argv[1], "--bench") == 0) {
    if (argc < 9) { usage(argv[0]); return 1; }
    u64 payload = (u64)strtoull(argv[2], NULL, 10);
    u64 extent = (u64)strtoull(argv[3], NULL, 10);
    u32 k = (u32)atoi(argv[4]);
    u32 p = (u32)atoi(argv[5]);
    u32 m = (u32)atoi(argv[6]);
    u64 map_bytes = (u64)strtoull(argv[7], NULL, 10);
    int iter = atoi(argv[8]);
    bench(payload, extent, k, p, m, map_bytes, iter);
    return 0;
  } else if (strcmp(argv[1], "--print") == 0) {
    if (argc < 8) { usage(argv[0]); return 1; }
    u64 payload = (u64)strtoull(argv[2], NULL, 10);
    u64 extent = (u64)strtoull(argv[3], NULL, 10);
    u32 k = (u32)atoi(argv[4]);
    u32 p = (u32)atoi(argv[5]);
    u32 m = (u32)atoi(argv[6]);
    u64 map_bytes = (u64)strtoull(argv[7], NULL, 10);
    zr_layout_t L;
    zr_layout_build(&L, payload, extent, k, p, m, map_bytes);
    print_layout(&L);
    return 0;
  } else if (strcmp(argv[1], "--json") == 0) {
    if (argc < 9) { usage(argv[0]); return 1; }
    const char *out = argv[2];
    u64 payload = (u64)strtoull(argv[3], NULL, 10);
    u64 extent = (u64)strtoull(argv[4], NULL, 10);
    u32 k = (u32)atoi(argv[5]);
    u32 p = (u32)atoi(argv[6]);
    u32 m = (u32)atoi(argv[7]);
    u64 map_bytes = (u64)strtoull(argv[8], NULL, 10);
    zr_layout_t L;
    zr_layout_build(&L, payload, extent, k, p, m, map_bytes);
    if (layout_to_json(&L, out) != 0) {
      fprintf(stderr, "Failed to write %s: %s\n", out, strerror(errno));
      return 2;
    }
    printf("Wrote layout JSON to %s\n", out);
    return 0;
  } else if (strcmp(argv[1], "--offsets") == 0) {
    if (argc < 11) { usage(argv[0]); return 1; }
    u64 payload = (u64)strtoull(argv[2], NULL, 10);
    u64 extent = (u64)strtoull(argv[3], NULL, 10);
    u32 k = (u32)atoi(argv[4]);
    u32 p = (u32)atoi(argv[5]);
    u32 m = (u32)atoi(argv[6]);
    u64 map_bytes = (u64)strtoull(argv[7], NULL, 10);
    u64 stripe = (u64)strtoull(argv[8], NULL, 10);
    u32 shard  = (u32)atoi(argv[9]);
    zr_layout_t L;
    zr_layout_build(&L, payload, extent, k, p, m, map_bytes);
    u64 off_d = zr_off_data(&L, stripe, shard);
    printf("data offset stripe=%" PRIu64 " shard=%u => %" PRIu64 "\n", stripe, shard, off_d);
    return 0;
  } else {
    usage(argv[0]);
    return 1;
  }
}