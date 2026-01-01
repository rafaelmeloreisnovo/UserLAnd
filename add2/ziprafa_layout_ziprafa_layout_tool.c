/*
 * ziprafa_layout_tool.c
 * RAFAELIA :: ZIPRAFA Layout Tool (CLI)
 *
 * Funcionalidades:
 *  - Define estruturas zr_super_t, zr_mapent_t (compatível com seu core)
 *  - zr_slot / zr_extent_off determinístico
 *  - Gerador de MAP com PROBE linear + bitmap de ocupação (evita colisões)
 *  - Exporta MAP para arquivo binário e JSON simples
 *  - Calcula CRC32C do superblock/map para integridade
 *  - CLI interativo com ASCII art, bench e self-tests
 *
 * Build:
 *   clang -O3 -std=c11 -Wall -Wextra ziprafa_layout_tool.c -o ziprafa_layout_tool -lm
 *
 * Observações:
 *  - Não realiza operações de rede/assinatura.
 *  - Para identidade/assinatura, integre BLAKE3/SHA3 externamente.
 *  - Respeite o ZIPRAF_OMEGA licensing module (ativar.txt) do seu repo.
 *
 * Author: RAFAELIA Copilot (por request de rafaelmeloreisnovo)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <errno.h>
#include <math.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;

#define ZR_MAGIC 0x4152414650495AULL /* "ZIPFARAA" */
#define ZR_VER   1

enum zr_kind : u8 {
  ZR_EXT_DATA   = 1,
  ZR_EXT_PARITY = 2,
  ZR_EXT_ECC    = 3,
  ZR_EXT_MAP    = 4,
  ZR_EXT_SUPER  = 5
};

typedef struct __attribute__((packed)) {
  u64 magic;
  u16 ver;
  u16 hdr_bytes;     // tamanho deste superblock
  u32 flags;

  u64 seed;          // session_seed
  u64 file_bytes;    // tamanho total lógico (payload)
  u64 extent_bytes;  // tamanho fixo de extent
  u32 k_data;        // shards de dados por stripe
  u32 m_ecc;         // shards ecc por stripe (0..)
  u32 parity_n;      // quantas paridades
  u32 map_entries;   // quantidade de extents no mapa

  u64 map_off0;      // offset do MAP primary
  u64 map_off1;      // offset do MAP mirror
  u32 super_crc32c;  // CRC do superblock (campo zerado ao calcular)
  u32 reserved;
} zr_super_t;

typedef struct __attribute__((packed)) {
  u64 stripe_id;
  u32 shard_id;
  u8  kind;          // DATA/PARITY/ECC/MAP
  u8  flags;
  u16 reserved;

  u64 logical_off;   // offset lógico (no payload reconstruído)
  u64 file_off;      // offset físico (no arquivo ZipRafa)
  u32 bytes;         // bytes válidos (último chunk pode ser menor)
  u32 crc32c;        // CRC32C do payload deste extent (ou 0 se vazio)
} zr_mapent_t;

/* ------------------------
   SplitMix64 (determinístico)
------------------------- */
static inline u64 zr_splitmix64(u64 *x) {
  u64 z = (*x += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

/* Slot pseudo-aleatório determinístico */
static inline u64 zr_slot(u64 seed, u64 stripe, u32 shard, u8 kind) {
  u64 x = seed
        ^ (stripe * 0xD6E8FEB86659FD93ULL)
        ^ ((u64)shard * 0xA5A5A5A5A5A5A5A5ULL)
        ^ ((u64)kind  * 0x9E3779B97F4A7C15ULL);
  /* mistura via splitmix */
  return zr_splitmix64(&x);
}

/* Map slot -> offset de extent (sem probe).
   total_slots = quantos "blocos" de extent cabem na área de dados do arquivo.
   base_off = início da área de extents.
*/
static inline u64 zr_extent_off_simple(u64 seed, u64 stripe, u32 shard, u8 kind,
                                u64 base_off, u64 extent_bytes, u64 total_slots) {
  u64 s = zr_slot(seed, stripe, shard, kind);
  u64 idx = (total_slots ? (s % total_slots) : 0);
  return base_off + idx * extent_bytes;
}

/* ------------------------
   CRC32C (Castagnoli) - compact bitwise
------------------------- */
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

/* ------------------------
   Bitmap occupancy helpers
------------------------- */
static inline uint8_t *bitmap_alloc(u64 bits) {
  u64 bytes = (bits + 7) / 8;
  uint8_t *b = calloc((size_t)bytes, 1);
  return b;
}
static inline int bitmap_test(const uint8_t *bm, u64 i) {
  return (bm[i >> 3] >> (i & 7)) & 1;
}
static inline void bitmap_set(uint8_t *bm, u64 i) {
  bm[i >> 3] |= (1u << (i & 7));
}

/* ------------------------
   zr_layout_cfg_t
------------------------- */
typedef struct {
  u64 base_off;
  u64 total_slots;
  u64 extent_bytes;
  u32 k_data;
  u32 parity_n;
  u32 m_ecc;
} zr_layout_cfg_t;

/* Build stripe map with linear-probe to avoid collisions.
   - out must have space for k_data + parity_n + m_ecc entries.
   - base file offsets chosen via zr_slot % total_slots; if collision (slot occupied),
     linear probe to next free slot (wrap-around).
*/
static inline void zr_build_stripe_map_with_probe(
  const zr_layout_cfg_t *cfg,
  u64 seed,
  u64 stripe_id,
  u64 logical_base_off,
  zr_mapent_t *out,  // precisa espaço: k_data + parity_n + m_ecc
  u32 *out_count,
  uint8_t *occupancy_bitmap // length = cfg->total_slots bits
) {
  u32 n = 0;
  u64 ts = cfg->total_slots;
  if (ts == 0) { *out_count = 0; return; }

  /* helper to allocate a slot with probe */
  auto allocate_slot = [&](u64 slot_base) -> u64 {
    u64 idx = slot_base % ts;
    /* linear probe up to ts attempts */
    for (u64 attempt = 0; attempt < ts; ++attempt) {
      u64 pos = (idx + attempt) % ts;
      if (!bitmap_test(occupancy_bitmap, pos)) {
        bitmap_set(occupancy_bitmap, pos);
        return cfg->base_off + pos * cfg->extent_bytes;
      }
    }
    /* if full (should not happen), fallback to slot_base */
    return cfg->base_off + idx * cfg->extent_bytes;
  };

  /* DATA shards */
  for (u32 i=0;i<cfg->k_data;i++) {
    zr_mapent_t e;
    memset(&e, 0, sizeof(e));
    e.stripe_id = stripe_id;
    e.shard_id = i;
    e.kind = ZR_EXT_DATA;
    e.logical_off = logical_base_off + (u64)i * cfg->extent_bytes;
    u64 s = zr_slot(seed, stripe_id, i, ZR_EXT_DATA);
    u64 sl = (s % ts);
    e.file_off = allocate_slot(sl);
    e.bytes = (u32)cfg->extent_bytes;
    e.crc32c = 0;
    out[n++] = e;
  }

  /* PARITY shards */
  for (u32 p=0;p<cfg->parity_n;p++) {
    zr_mapent_t e;
    memset(&e, 0, sizeof(e));
    e.stripe_id = stripe_id;
    e.shard_id = p;
    e.kind = ZR_EXT_PARITY;
    e.logical_off = 0;
    u64 s = zr_slot(seed, stripe_id, p, ZR_EXT_PARITY);
    u64 sl = (s % ts);
    e.file_off = allocate_slot(sl);
    e.bytes = (u32)cfg->extent_bytes;
    e.crc32c = 0;
    out[n++] = e;
  }

  /* ECC shards */
  for (u32 m=0;m<cfg->m_ecc;m++) {
    zr_mapent_t e;
    memset(&e, 0, sizeof(e));
    e.stripe_id = stripe_id;
    e.shard_id = m;
    e.kind = ZR_EXT_ECC;
    e.logical_off = 0;
    u64 s = zr_slot(seed, stripe_id, m, ZR_EXT_ECC);
    u64 sl = (s % ts);
    e.file_off = allocate_slot(sl);
    e.bytes = (u32)cfg->extent_bytes;
    e.crc32c = 0;
    out[n++] = e;
  }

  *out_count = n;
}

/* ------------------------
   Export functions: binary and JSON (simple)
------------------------- */
static int write_map_bin(const char *path, const zr_mapent_t *ents, u32 count) {
  FILE *f = fopen(path, "wb");
  if (!f) return -1;
  if (fwrite(ents, sizeof(zr_mapent_t), count, f) != count) { fclose(f); return -2; }
  fclose(f);
  return 0;
}

static int write_map_json(const char *path, const zr_mapent_t *ents, u32 count) {
  FILE *f = fopen(path, "w");
  if (!f) return -1;
  fprintf(f, "{\n  \"map_entries\": %u,\n  \"entries\": [\n", count);
  for (u32 i=0;i<count;i++) {
    const zr_mapent_t *e = &ents[i];
    fprintf(f,
      "    { \"stripe_id\": %" PRIu64 ", \"shard_id\": %u, \"kind\": %u, \"logical_off\": %" PRIu64
      ", \"file_off\": %" PRIu64 ", \"bytes\": %u, \"crc32c\": %u }%s\n",
      e->stripe_id, e->shard_id, (unsigned)e->kind, e->logical_off, e->file_off, e->bytes, e->crc32c,
      (i+1==count) ? "" : ",");
  }
  fprintf(f, "  ]\n}\n");
  fclose(f);
  return 0;
}

/* ------------------------
   CLI / interactive
------------------------- */
static void print_banner(void) {
  puts("=============================================");
  puts("   ZIPRAFA Layout Tool  - RAFAELIA / ZIPRAF");
  puts("   MAP generator with probe/bitmap + JSON");
  puts("=============================================");
}

static void print_menu(void) {
  puts("");
  puts("Menu:");
  puts("  1) Create sample superblock");
  puts("  2) Generate MAP for N stripes (with probe)");
  puts("  3) Export MAP (bin + json)");
  puts("  4) Run simple self-tests");
  puts("  5) Benchmark layout generation speed");
  puts("  0) Exit");
  printf("Choice: ");
}

/* Simple benchmark for layout generation */
static int bench_layout(const zr_layout_cfg_t *cfg, u64 seed, int stripes) {
  struct timespec t0, t1;
  zr_mapent_t *ents = malloc((size_t)(cfg->k_data + cfg->parity_n + cfg->m_ecc) * sizeof(zr_mapent_t));
  uint8_t *bm = bitmap_alloc(cfg->total_slots);
  if (!ents || !bm) { free(ents); free(bm); return -1; }

  clock_gettime(CLOCK_MONOTONIC, &t0);
  for (int i=0;i<stripes;i++) {
    memset(bm, 0, (size_t)((cfg->total_slots+7)/8));
    u32 outc;
    zr_build_stripe_map_with_probe(cfg, seed, (u64)i, (u64)i * cfg->extent_bytes * cfg->k_data, ents, &outc, bm);
    (void)outc;
  }
  clock_gettime(CLOCK_MONOTONIC, &t1);
  double secs = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;
  double persec = stripes / secs;
  printf("Generated %d stripes in %.6f s (%.2f stripes/s)\n", stripes, secs, persec);
  free(ents); free(bm);
  return 0;
}

/* Self-tests:
   - deterministic slot mapping unchanged
   - probe resolves collisions (simulate small total_slots)
*/
static int selftests(void) {
  puts("Running self-tests...");

  /* 1) simple slot determinism */
  u64 seed = 0x123456789abcdef0ULL;
  u64 s1 = zr_slot(seed, 1, 0, ZR_EXT_DATA);
  u64 s2 = zr_slot(seed, 1, 0, ZR_EXT_DATA);
  if (s1 != s2) { printf("Slot determinism FAILED\n"); return -1; }
  printf("Slot determinism PASS\n");

  /* 2) probe resolves collision: set total_slots small and force two items to same slot */
  zr_layout_cfg_t cfg;
  cfg.base_off = 1024;
  cfg.total_slots = 3; /* pequeno para forçar colisões */
  cfg.extent_bytes = 4096;
  cfg.k_data = 3;
  cfg.parity_n = 1;
  cfg.m_ecc = 0;

  uint8_t *bm = bitmap_alloc(cfg.total_slots);
  if (!bm) return -2;
  zr_mapent_t *ents = calloc((size_t)(cfg.k_data + cfg.parity_n + cfg.m_ecc), sizeof(zr_mapent_t));
  u32 outc = 0;
  zr_build_stripe_map_with_probe(&cfg, seed, 7, 0, ents, &outc, bm);

  /* verify that all file_off are unique (no duplicates) */
  int ok = 1;
  for (u32 i=0;i<outc;i++) {
    for (u32 j=i+1;j<outc;j++) {
      if (ents[i].file_off == ents[j].file_off) {
        ok = 0;
        printf("Collision not resolved: i=%u j=%u off=%" PRIu64 "\n", i, j, ents[i].file_off);
      }
    }
  }
  if (!ok) { free(bm); free(ents); return -3; }
  printf("Probe collision-resolution PASS (%u entries)\n", outc);

  free(bm); free(ents);
  puts("All self-tests passed.");
  return 0;
}

/* Create sample superblock helper */
static zr_super_t make_sample_super(u64 seed, u64 file_bytes, u64 extent_bytes, u32 k_data, u32 parity_n, u32 m_ecc, u64 base_off, u64 total_slots) {
  zr_super_t s;
  memset(&s, 0, sizeof(s));
  s.magic = ZR_MAGIC;
  s.ver = ZR_VER;
  s.hdr_bytes = (u16)sizeof(zr_super_t);
  s.flags = 0;
  s.seed = seed;
  s.file_bytes = file_bytes;
  s.extent_bytes = extent_bytes;
  s.k_data = k_data;
  s.m_ecc = m_ecc;
  s.parity_n = parity_n;
  s.map_entries = 0; /* preenchido conforme gerar */
  s.map_off0 = base_off;
  s.map_off1 = base_off + total_slots * extent_bytes; /* exemplo */
  s.super_crc32c = 0;
  s.reserved = 0;
  return s;
}

/* Write superblock to file with CRC32C */
static int write_superblock(const char *path, zr_super_t *s) {
  /* zero crc field, compute, then write with crc filled */
  s->super_crc32c = 0;
  uint32_t c = crc32c((const uint8_t*)s, sizeof(zr_super_t));
  s->super_crc32c = c;
  FILE *f = fopen(path, "wb");
  if (!f) return -1;
  if (fwrite(s, sizeof(zr_super_t), 1, f) != 1) { fclose(f); return -2; }
  fclose(f);
  return 0;
}

/* Main interactive loop */
int main(int argc, char **argv) {
  print_banner();
  if (argc == 1) {
    while (1) {
      print_menu();
      int choice = 0;
      if (scanf("%d", &choice) != 1) { int ch; while ((ch=getchar())!=EOF && ch!='\n'); continue; }
      if (choice == 0) { puts("Bye."); break; }
      if (choice == 1) {
        zr_super_t s = make_sample_super((u64)time(NULL), 1024ull*1024ull*1024ull, 4096, 6, 1, 0, 1024*1024, 10000);
        char path[256];
        printf("Superblock output path: "); scanf("%255s", path);
        if (write_superblock(path, &s) == 0) {
          printf("Wrote superblock to %s (crc32c=0x%08x)\n", path, s.super_crc32c);
        } else {
          printf("Failed to write superblock\n");
        }
      } else if (choice == 2) {
        char prefix[256];
        u64 seed; int stripes;
        u64 base_off; u64 total_slots; u64 extent_bytes;
        int k_data, parity_n, m_ecc;
        printf("Output prefix for map files: "); scanf("%255s", prefix);
        printf("Seed (0 for time): "); scanf("%" SCNu64, &seed);
        if (seed == 0) seed = (u64)time(NULL);
        printf("Stripes to generate: "); scanf("%d", &stripes);
        printf("extent_bytes: "); scanf("%" SCNu64, &extent_bytes);
        printf("total_slots: "); scanf("%" SCNu64, &total_slots);
        printf("base_off: "); scanf("%" SCNu64, &base_off);
        printf("k_data: "); scanf("%d", &k_data);
        printf("parity_n: "); scanf("%d", &parity_n);
        printf("m_ecc: "); scanf("%d", &m_ecc);
        zr_layout_cfg_t cfg = { base_off, total_slots, extent_bytes, (u32)k_data, (u32)parity_n, (u32)m_ecc };
        u32 perstripe = cfg.k_data + cfg.parity_n + cfg.m_ecc;
        zr_mapent_t *all = calloc((size_t)perstripe * (size_t)stripes, sizeof(zr_mapent_t));
        if (!all) { puts("malloc failed"); continue; }
        uint8_t *bm = bitmap_alloc(cfg.total_slots);
        if (!bm) { free(all); puts("bitmap failed"); continue; }
        u32 idx = 0;
        for (int i=0;i<stripes;i++) {
          memset(bm, 0, (size_t)((cfg.total_slots+7)/8));
          u32 outc = 0;
          zr_build_stripe_map_with_probe(&cfg, seed, (u64)i, (u64)i * cfg.extent_bytes * cfg.k_data, &all[idx], &outc, bm);
          idx += outc;
        }
        char bpath[512], jpath[512];
        snprintf(bpath, sizeof(bpath), "%s.map.bin", prefix);
        snprintf(jpath, sizeof(jpath), "%s.map.json", prefix);
        if (write_map_bin(bpath, all, idx) == 0) printf("Wrote binary map %s (%u entries)\n", bpath, idx);
        if (write_map_json(jpath, all, idx) == 0) printf("Wrote json map %s (%u entries)\n", jpath, idx);
        free(all); free(bm);
      } else if (choice == 3) {
        puts("Option 3 (export) handled implicitly in option 2.");
      } else if (choice == 4) {
        if (selftests() != 0) {
          puts("Self-tests FAILED");
        } else {
          puts("Self-tests OK");
        }
      } else if (choice == 5) {
        u64 seed; int stripes;
        u64 extent_bytes; u64 total_slots; u64 base_off; int k_data, parity_n, m_ecc;
        printf("Seed (0 for time): "); scanf("%" SCNu64, &seed);
        if (seed == 0) seed = (u64)time(NULL);
        printf("Stripes to generate for bench: "); scanf("%d", &stripes);
        printf("extent_bytes: "); scanf("%" SCNu64, &extent_bytes);
        printf("total_slots: "); scanf("%" SCNu64, &total_slots);
        printf("base_off: "); scanf("%" SCNu64, &base_off);
        printf("k_data: "); scanf("%d", &k_data);
        printf("parity_n: "); scanf("%d", &parity_n);
        printf("m_ecc: "); scanf("%d", &m_ecc);
        zr_layout_cfg_t cfg = { base_off, total_slots, extent_bytes, (u32)k_data, (u32)parity_n, (u32)m_ecc };
        bench_layout(&cfg, seed, stripes);
      } else {
        puts("Unknown option");
      }
    }
    return 0;
  } else {
    /* Non-interactive quick mode: --selftest or --bench */
    if (argc >= 2 && strcmp(argv[1], "--selftest") == 0) {
      return selftests();
    } else if (argc >= 2 && strcmp(argv[1], "--bench") == 0) {
      if (argc < 6) {
        fprintf(stderr, "Usage: %s --bench seed stripes extent_bytes total_slots k_data parity_n m_ecc\n", argv[0]);
        return 1;
      }
      u64 seed = (u64)strtoull(argv[2], NULL, 10);
      int stripes = atoi(argv[3]);
      u64 extent_bytes = (u64)strtoull(argv[4], NULL, 10);
      u64 total_slots = (u64)strtoull(argv[5], NULL, 10);
      int k_data = (argc>6) ? atoi(argv[6]) : 6;
      int parity_n = (argc>7) ? atoi(argv[7]) : 1;
      int m_ecc = (argc>8) ? atoi(argv[8]) : 0;
      u64 base_off = 1024;
      zr_layout_cfg_t cfg = { base_off, total_slots, extent_bytes, (u32)k_data, (u32)parity_n, (u32)m_ecc };
      return bench_layout(&cfg, seed, stripes);
    } else {
      fprintf(stderr, "Unknown args. Use no args for interactive.\n");
      return 1;
    }
  }
}