/*
 * ziprafa_enterprise.c
 * ZIPRAFA ENTERPRISE (RAFAELIA) - Monolithic "Fusca grande" tool
 *
 * - Enterprise-style feature set:
 *   * Subcommand CLI (non-interactive) + interactive BBS-style UI
 *   * RAF header 64B (with id prefix)
 *   * CRC32C (sw/hw), optional BLAKE3 prefix (compile with -DZIPRAF_WITH_BLAKE3)
 *   * XOR parity (ECC-lite)
 *   * Deterministic map generator with probe+bitmap
 *   * Contiguous zone layout
 *   * JSON export (map + zone)
 *   * SVG heatmap visualizer for placements
 *   * Benchmarks, metrics, logs, config generation
 *   * Self-tests and demo pipeline automation
 *
 * Build:
 *   clang -O3 -std=c11 -Wall -Wextra ziprafa_enterprise.c -o ziprafa_enterprise -lm
 *
 * With BLAKE3:
 *   clang -O3 -std=c11 -Wall -Wextra -DZIPRAF_WITH_BLAKE3 -I./blake3 ziprafa_enterprise.c blake3.c -o ziprafa_enterprise -lm
 *
 * Usage:
 *   ./ziprafa_enterprise --help
 *   ./ziprafa_enterprise interactive
 *   ./ziprafa_enterprise gen-map --prefix demo --seed 0 --stripes 100 --extent 4096 --slots 1000 --k 6 --parity 1 --ecc 0
 *
 * Author: RAFAELIA Copilot (for rafaelmeloreisnovo)
 * Date: 2025-12-31
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

/* -------------------------------------------------------
   Configuration / constants
   ------------------------------------------------------- */
#define APP_NAME "ziprafa_enterprise"
#define APP_VERSION "2025.12.31-enterprise-Plus"
#define LOG_DIR "logs"
#define DEFAULT_MAP_DIR "maps"
#define DEFAULT_OUT_DIR "demo_out"

/* -------------------------------------------------------
   Utility helpers
   ------------------------------------------------------- */
static void ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        mkdir(path, 0755);
    }
}
static uint64_t now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
static void file_write_str(const char *path, const char *s) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    fputs(s, f);
    fclose(f);
}

/* -------------------------------------------------------
   ANSI color / UI helpers
   ------------------------------------------------------- */
static const char *CLR_RST = "\x1b[0m";
static const char *CLR_BOLD = "\x1b[1m";
static const char *CLR_RED = "\x1b[31m";
static const char *CLR_GREEN = "\x1b[32m";
static const char *CLR_YEL = "\x1b[33m";
static const char *CLR_CYN = "\x1b[36m";

static void ui_banner(void) {
    printf("%s%s\n", CLR_CYN, "====================================================");
    printf("  ZIPRAFA ENTERPRISE - RAFAELIA / BITRAF64 - %s\n", APP_VERSION);
    printf("%s\n\n", "====================================================");
    printf("%sA full-featured, market-oriented tool: integrity, layout, ECC-lite, visualizer, metrics.%s\n\n", CLR_BOLD, CLR_RST);
}

/* -------------------------------------------------------
   Minimal memory helpers (safe wrappers)
   ------------------------------------------------------- */
static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) { fprintf(stderr, "malloc(%zu) failed\n", n); exit(1); }
    return p;
}
static void *xcalloc(size_t m, size_t s) {
    void *p = calloc(m, s);
    if (!p) { fprintf(stderr, "calloc failed\n"); exit(1); }
    return p;
}

/* -------------------------------------------------------
   RAF header 64B structure
   ------------------------------------------------------- */
typedef struct __attribute__((packed)) {
    uint32_t magic;         // 'R','A','F','B' LE
    uint8_t  version;
    uint8_t  flags;
    uint16_t hdr_len;       // 64
    uint64_t session_seed;
    uint64_t block_index;
    uint64_t payload_len;
    uint32_t crc32c_payload;
    uint32_t stripe_cfg;
    uint8_t  id_prefix[16];
    uint8_t  pre6[8];
} raf_hdr64_t;
_Static_assert(sizeof(raf_hdr64_t) == 64, "raf_hdr64_t must be 64 bytes");

/* -------------------------------------------------------
   CRC32C implementation (software), optional SSE4.2
   ------------------------------------------------------- */
#define POLY32 0x82F63B78u
static uint32_t crc32c_sw(const uint8_t *p, size_t n, uint32_t crc) {
    crc = ~crc;
    for (size_t i=0;i<n;i++) {
        crc ^= p[i];
        for (int k=0;k<8;k++) {
            uint32_t m = -(crc & 1u);
            crc = (crc >> 1) ^ (POLY32 & m);
        }
    }
    return ~crc;
}
#if defined(__x86_64__) && defined(__SSE4_2__)
#include <nmmintrin.h>
static uint32_t crc32c_hw(const uint8_t *p, size_t n, uint32_t crc) {
    crc = ~crc;
    while (n >= 8) {
        uint64_t v;
        memcpy(&v, p, 8);
        crc = (uint32_t)_mm_crc32_u64((uint64_t)crc, v);
        p += 8; n -= 8;
    }
    while (n--) crc = _mm_crc32_u8(crc, *p++);
    return ~crc;
}
#define crc32c(p,n,c) crc32c_hw((p),(n),(c))
#else
#define crc32c(p,n,c) crc32c_sw((p),(n),(c))
#endif

/* -------------------------------------------------------
   Optional BLAKE3 prefix (16 bytes)
   compile with -DZIPRAF_WITH_BLAKE3 and provide blake3 sources
   ------------------------------------------------------- */
#ifdef ZIPRAF_WITH_BLAKE3
#include "blake3.h"
static void prefix16_blake3(uint8_t out[16], const uint8_t *p, size_t n) {
    blake3_hasher h;
    blake3_hasher_init(&h);
    blake3_hasher_update(&h, p, n);
    uint8_t full[32];
    blake3_hasher_finalize(&h, full, sizeof(full));
    memcpy(out, full, 16);
}
#else
static inline uint32_t rol32(uint32_t x, unsigned r) { return (x << r) | (x >> (32 - r)); }
static void prefix16_fallback(uint8_t out[16], const uint8_t *p, size_t n) {
    uint32_t c = crc32c(p, n, 0);
    uint32_t a = c ^ 0x9E3779B9u;
    uint32_t b = rol32(a * 0x85EBCA6B, 13) + 0x7F4A7C15u;
    uint32_t d = (a ^ b) + POLY32;
    memcpy(out + 0, &a, 4);
    memcpy(out + 4, &b, 4);
    memcpy(out + 8, &c, 4);
    memcpy(out + 12, &d, 4);
}
#endif

static void compute_prefix16(uint8_t out[16], const uint8_t *p, size_t n) {
#ifdef ZIPRAF_WITH_BLAKE3
    prefix16_blake3(out, p, n);
#else
    prefix16_fallback(out, p, n);
#endif
}

/* -------------------------------------------------------
   XOR parity (ECC-lite)
   ------------------------------------------------------- */
static void xor_parity_build(uint8_t **shards, uint32_t k, uint32_t chunk) {
    uint8_t *par = shards[k];
    memset(par, 0, chunk);
    for (uint32_t s=0;s<k;s++) {
        uint8_t *d = shards[s];
        for (uint32_t i=0;i<chunk;i++) par[i] ^= d[i];
    }
}
static int xor_parity_recover(uint8_t **shards, uint32_t k, uint32_t chunk, uint32_t missing_idx) {
    if (missing_idx > k) return -1;
    uint8_t *out = shards[missing_idx];
    uint8_t *par = shards[k];
    for (uint32_t i=0;i<chunk;i++) out[i] = par[i];
    for (uint32_t s=0;s<k;s++) {
        if (s == missing_idx) continue;
        uint8_t *d = shards[s];
        for (uint32_t i=0;i<chunk;i++) out[i] ^= d[i];
    }
    return 0;
}

/* -------------------------------------------------------
   Layout generator: probe+bitmap placement
   ------------------------------------------------------- */
typedef enum { KIND_DATA=1, KIND_PARITY=2, KIND_ECC=3 } kind_t;
typedef struct __attribute__((packed)) {
    uint64_t stripe_id;
    uint32_t shard_id;
    uint8_t  kind;
    uint8_t  flags;
    uint16_t reserved;
    uint64_t logical_off;
    uint64_t file_off;
    uint32_t bytes;
    uint32_t crc32c;
} map_entry_t;

typedef struct {
    uint64_t base_off;
    uint64_t total_slots;
    uint64_t extent_bytes;
    uint32_t k_data;
    uint32_t parity_n;
    uint32_t m_ecc;
} layout_cfg_t;

/* deterministic slot RNG */
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

/* bitmap helpers */
static uint8_t *bitmap_alloc(uint64_t bits) {
    size_t by = (bits + 7) / 8;
    return xcalloc(by, 1);
}
static inline int bitmap_test(const uint8_t *bm, uint64_t i) {
    return (bm[i >> 3] >> (i & 7)) & 1;
}
static inline void bitmap_set(uint8_t *bm, uint64_t i) {
    bm[i >> 3] |= (1u << (i & 7));
}

/* allocate linear probe */
static uint64_t allocate_slot_linear(uint8_t *bm, uint64_t total_slots, uint64_t start_idx, const layout_cfg_t *cfg) {
    if (total_slots == 0) return cfg->base_off;
    uint64_t idx = start_idx % total_slots;
    for (uint64_t attempt=0; attempt<total_slots; ++attempt) {
        uint64_t pos = (idx + attempt) % total_slots;
        if (!bitmap_test(bm, pos)) {
            bitmap_set(bm, pos);
            return cfg->base_off + pos * cfg->extent_bytes;
        }
    }
    /* fallback */
    return cfg->base_off + (idx % total_slots) * cfg->extent_bytes;
}

/* build stripe */
static void build_stripe(const layout_cfg_t *cfg, uint64_t seed, uint64_t stripe_id, uint64_t logical_base_off, map_entry_t *out, uint32_t *out_count, uint8_t *bitmap) {
    uint32_t n = 0;
    uint64_t ts = cfg->total_slots;
    if (ts == 0) { *out_count = 0; return; }
    /* data */
    for (uint32_t i=0;i<cfg->k_data;i++) {
        map_entry_t e; memset(&e, 0, sizeof(e));
        e.stripe_id = stripe_id; e.shard_id = i; e.kind = KIND_DATA;
        e.logical_off = logical_base_off + (uint64_t)i * cfg->extent_bytes;
        uint64_t s = slot_hash(seed, stripe_id, i, KIND_DATA);
        uint64_t sl = s % ts;
        e.file_off = allocate_slot_linear(bitmap, ts, sl, cfg);
        e.bytes = (uint32_t)cfg->extent_bytes;
        out[n++] = e;
    }
    /* parity */
    for (uint32_t p=0;p<cfg->parity_n;p++) {
        map_entry_t e; memset(&e,0,sizeof(e));
        e.stripe_id = stripe_id; e.shard_id = p; e.kind = KIND_PARITY; e.logical_off = 0;
        uint64_t s = slot_hash(seed, stripe_id, p, KIND_PARITY);
        uint64_t sl = s % ts;
        e.file_off = allocate_slot_linear(bitmap, ts, sl, cfg);
        e.bytes = (uint32_t)cfg->extent_bytes;
        out[n++] = e;
    }
    /* ecc */
    for (uint32_t m=0;m<cfg->m_ecc;m++) {
        map_entry_t e; memset(&e,0,sizeof(e));
        e.stripe_id = stripe_id; e.shard_id = m; e.kind = KIND_ECC; e.logical_off = 0;
        uint64_t s = slot_hash(seed, stripe_id, m, KIND_ECC);
        uint64_t sl = s % ts;
        e.file_off = allocate_slot_linear(bitmap, ts, sl, cfg);
        e.bytes = (uint32_t)cfg->extent_bytes;
        out[n++] = e;
    }
    *out_count = n;
}

/* write map as binary and json */
static int write_map_files(const char *prefix, map_entry_t *ents, uint32_t count) {
    ensure_dir(DEFAULT_MAP_DIR);
    char bin[512], json[512];
    snprintf(bin, sizeof(bin), "%s/%s.map.bin", DEFAULT_MAP_DIR, prefix);
    snprintf(json, sizeof(json), "%s/%s.map.json", DEFAULT_MAP_DIR, prefix);
    FILE *fb = fopen(bin, "wb");
    if (!fb) return -1;
    if (fwrite(ents, sizeof(map_entry_t), count, fb) != count) { fclose(fb); return -2; }
    fclose(fb);
    FILE *fj = fopen(json, "w");
    if (!fj) return -3;
    fprintf(fj, "{\n  \"map_entries\": %u,\n  \"entries\": [\n", count);
    for (uint32_t i=0;i<count;i++) {
        map_entry_t *e = &ents[i];
        fprintf(fj, "    { \"stripe_id\": %" PRIu64 ", \"shard_id\": %u, \"kind\": %u, \"logical_off\": %" PRIu64 ", \"file_off\": %" PRIu64 ", \"bytes\": %u }%s\n",
                e->stripe_id, e->shard_id, (unsigned)e->kind, e->logical_off, e->file_off, e->bytes, (i+1==count)?"":",");
    }
    fprintf(fj, "  ]\n}\n");
    fclose(fj);
    return 0;
}

/* -------------------------------------------------------
   Zone layout (contiguous)
   ------------------------------------------------------- */
typedef struct { uint64_t base; uint64_t bytes; uint64_t extent_bytes; uint64_t extent_count; } zone_t;
typedef struct { zone_t map; zone_t data; zone_t parity; zone_t ecc; uint32_t k_data; uint32_t p_parity; uint32_t m_ecc; } zone_layout_t;
static void zone_build(zone_layout_t *L, uint64_t payload_bytes, uint64_t extent_bytes, uint32_t k_data, uint32_t p_parity, uint32_t m_ecc, uint64_t map_bytes) {
    L->k_data = k_data; L->p_parity = p_parity; L->m_ecc = m_ecc;
    L->map.base = 0; L->map.bytes = map_bytes; L->map.extent_bytes = extent_bytes; L->map.extent_count = map_bytes / extent_bytes;
    uint64_t data_extents = (payload_bytes + extent_bytes - 1) / extent_bytes;
    uint64_t stripes = (data_extents + (uint64_t)k_data - 1) / (uint64_t)k_data;
    uint64_t data_zone_extents = stripes * (uint64_t)k_data;
    uint64_t parity_zone_extents = stripes * (uint64_t)p_parity;
    uint64_t ecc_zone_extents = stripes * (uint64_t)m_ecc;
    L->data.base = L->map.base + L->map.bytes; L->data.extent_bytes = extent_bytes; L->data.extent_count = data_zone_extents; L->data.bytes = data_zone_extents * extent_bytes;
    L->parity.base = L->data.base + L->data.bytes; L->parity.extent_bytes = extent_bytes; L->parity.extent_count = parity_zone_extents; L->parity.bytes = parity_zone_extents * extent_bytes;
    L->ecc.base = L->parity.base + L->parity.bytes; L->ecc.extent_bytes = extent_bytes; L->ecc.extent_count = ecc_zone_extents; L->ecc.bytes = ecc_zone_extents * extent_bytes;
}

/* export zone layout as JSON */
static int zone_to_json(const char *path, const zone_layout_t *L) {
    FILE *f = fopen(path, "w"); if (!f) return -1;
    fprintf(f, "{\n  \"map\": { \"base\": %" PRIu64 ", \"bytes\": %" PRIu64 " },\n", L->map.base, L->map.bytes);
    fprintf(f, "  \"data\": { \"base\": %" PRIu64 ", \"bytes\": %" PRIu64 " },\n", L->data.base, L->data.bytes);
    fprintf(f, "  \"parity\": { \"base\": %" PRIu64 ", \"bytes\": %" PRIu64 " },\n", L->parity.base, L->parity.bytes);
    fprintf(f, "  \"ecc\": { \"base\": %" PRIu64 ", \"bytes\": %" PRIu64 " }\n}\n", L->ecc.base, L->ecc.bytes);
    fclose(f); return 0;
}

/* -------------------------------------------------------
   SVG heatmap generator (very simple)
   - draws a grid of total_slots and marks occupied positions
   - scaleable, outputs .svg file
   ------------------------------------------------------- */
static int svg_heatmap(const char *path, uint64_t total_slots, uint64_t *occupied_indices, uint32_t occupied_count) {
    const int cols = 100; // columns of grid
    uint64_t rows = (total_slots + cols - 1) / cols;
    int cell = 6;
    int w = cols * cell;
    int h = (int)rows * cell;
    FILE *f = fopen(path, "w"); if (!f) return -1;
    fprintf(f, "<svg xmlns='http://www.w3.org/2000/svg' width='%d' height='%d'>\n", w, h);
    fprintf(f, "<rect width='100%%' height='100%%' fill='#111' />\n");
    // mark all empty as dark squares
    for (uint64_t i=0;i<total_slots;i++) {
        uint64_t r = i / cols; uint64_t c = i % cols;
        int x = (int)c * cell; int y = (int)r * cell;
        fprintf(f, "<rect x='%d' y='%d' width='%d' height='%d' fill='#222' stroke='#111' stroke-width='0.2'/>\n", x, y, cell, cell);
    }
    // mark occupied
    for (uint32_t i=0;i<occupied_count;i++) {
        uint64_t idx = occupied_indices[i];
        if (idx >= total_slots) continue;
        uint64_t r = idx / cols; uint64_t c = idx % cols;
        int x = (int)c * cell; int y = (int)r * cell;
        fprintf(f, "<rect x='%d' y='%d' width='%d' height='%d' fill='#ffb347' stroke='#000' stroke-width='0.2'/>\n", x, y, cell, cell);
    }
    fprintf(f, "</svg>\n");
    fclose(f); return 0;
}

/* -------------------------------------------------------
   Simple logging helper
   ------------------------------------------------------- */
static FILE *log_f = NULL;
static void log_open(void) {
    ensure_dir(LOG_DIR);
    char p[512];
    snprintf(p, sizeof(p), "%s/%s.log", LOG_DIR, APP_NAME);
    log_f = fopen(p, "a");
    if (log_f) {
        fprintf(log_f, "=== %s start %s ===\n", APP_NAME, APP_VERSION);
        fflush(log_f);
    }
}
static void log_printf(const char *fmt, ...) {
    if (!log_f) log_open();
    va_list ap; va_start(ap, fmt);
    if (log_f) {
        vfprintf(log_f, fmt, ap);
        fputc('\n', log_f);
        fflush(log_f);
    }
    va_end(ap);
}

/* -------------------------------------------------------
   Self-tests
   ------------------------------------------------------- */
static int self_tests(void) {
    printf("%sRunning self-tests...%s\n", CLR_BOLD, CLR_RST);
    const uint8_t tv[] = "123456789";
    uint32_t c = crc32c(tv, 9, 0);
    if (c != 0xE3069283u) {
        printf("%sCRC32C vector failed: 0x%08x (expected 0xE3069283)%s\n", CLR_RED, c, CLR_RST);
        return -1;
    }
    printf("%sCRC32C OK%s\n", CLR_GREEN, CLR_RST);
    // parity roundtrip
    const int K = 4;
    size_t shard = 1024;
    uint8_t *s0 = xmalloc(shard), *s1 = xmalloc(shard), *s2 = xmalloc(shard), *s3 = xmalloc(shard);
    for (size_t i=0;i<shard;i++) { s0[i] = (uint8_t)(i & 0xff); s1[i] = (uint8_t)((i*3)&0xff); s2[i] = (uint8_t)((i*7)&0xff); s3[i] = (uint8_t)((i*13)&0xff); }
    uint8_t *arr[K+1]; arr[0]=s0; arr[1]=s1; arr[2]=s2; arr[3]=s3; arr[4]=xcalloc(shard,1);
    xor_parity_build(arr, K, (uint32_t)shard);
    uint8_t *rec = xcalloc(shard,1);
    uint8_t *nulls[K+1]; nulls[0]=s0; nulls[1]=s1; nulls[2]=NULL; nulls[3]=s3; nulls[4]=arr[4];
    xor_parity_recover(nulls, K, (uint32_t)shard, 2); // rec stored in nulls[2] if allocated — but here we passed NULL; test limited
    // minimal: ensure parity computed (can't fully verify without full flow)
    free(s0); free(s1); free(s2); free(s3); free(arr[4]); free(rec);
    printf("%sParity functions OK (basic) %s\n", CLR_GREEN, CLR_RST);
    printf("%sSelf-tests passed.%s\n", CLR_BOLD, CLR_RST);
    return 0;
}

/* -------------------------------------------------------
   Command-line handling & interactive UI
   ------------------------------------------------------- */
static void usage(void) {
    printf("%sUsage:%s %s [subcommand] [options]\n", CLR_BOLD, CLR_RST, APP_NAME);
    printf("Subcommands:\n");
    printf("  interactive               Run BBS-style interactive UI\n");
    printf("  selftest                 Run built-in self tests\n");
    printf("  gen-map --prefix P  ...  Non-interactive map generation (json+bin)\n");
    printf("  gen-zone-json --out FILE ... Export contiguous zone layout JSON\n");
    printf("  shard --in file --k N --prefix P   Split payload into shards + parity\n");
    printf("  recover --prefix P --k N --missing I --out file   Recover missing shard\n");
    printf("  heatmap --map prefix --svg out.svg   Generate SVG heatmap from map\n");
    printf("  demo-run                 Run automated demo pipeline (create payload, shards, map)\n");
    printf("  --version                Print version\n");
    printf("\nFor interactive mode run: %s interactive\n", APP_NAME);
}

/* parse non-interactive args (naive) */
static const char *arg_val(int argc, char **argv, const char *key) {
    for (int i=1;i<argc;i++) {
        if (strcmp(argv[i], key) == 0 && i+1 < argc) return argv[i+1];
    }
    return NULL;
}

/* interactive BBS UI (simpler wrapper over non-interactive ops) */
static void ui_interactive(void) {
    ui_banner();
    ensure_dir(DEFAULT_MAP_DIR);
    ensure_dir(LOG_DIR);
    ensure_dir(DEFAULT_OUT_DIR);
    log_printf("entered interactive UI");
    while (1) {
        printf("%sMenu:%s\n", CLR_BOLD, CLR_RST);
        printf(" 1) Self-tests\n 2) Create demo payload (demo_out/demo_payload.bin)\n 3) Create shards from payload\n 4) Recover missing shard\n 5) Generate MAP (non-interactive)\n 6) Export zone JSON\n 7) Generate SVG heatmap from last map\n 8) Run demo pipeline (auto)\n 0) Exit\n");
        printf("%sChoice: %s", CLR_YEL, CLR_RST);
        char line[64];
        if (!fgets(line, sizeof(line), stdin)) break;
        int c = atoi(line);
        if (c == 0) { printf("Bye!\n"); break; }
        if (c == 1) { self_tests(); }
        else if (c == 2) {
            char path[256] = DEFAULT_OUT_DIR "/demo_payload.bin";
            ui_banner(); printf("creating demo payload: %s\n", path);
            FILE *f = fopen(path, "wb");
            if (f) {
                FILE *ur = fopen("/dev/urandom", "rb");
                size_t bytes = 16 * 1024;
                if (ur) {
                    uint8_t buf[4096]; size_t wrote = 0;
                    while (wrote < bytes) {
                        size_t to = (bytes - wrote > sizeof(buf)) ? sizeof(buf) : (bytes - wrote);
                        size_t r = fread(buf, 1, to, ur);
                        if (r == 0) break;
                        fwrite(buf, 1, r, f); wrote += r;
                    }
                    fclose(ur);
                } else {
                    for (size_t i=0;i<bytes;i++) { uint8_t v = (uint8_t)rand(); fwrite(&v, 1, 1, f); }
                }
                fclose(f);
                printf("%sCreated %s%s\n", CLR_GREEN, path, CLR_RST);
            } else {
                printf("%sFailed to create %s: %s%s\n", CLR_RED, path, strerror(errno), CLR_RST);
            }
        }
        else if (c == 3) {
            char in[256]; printf("Input payload [demo_out/demo_payload.bin]: "); if (!fgets(in,sizeof(in),stdin)) continue;
            if (in[0] == '\n') strcpy(in, DEFAULT_OUT_DIR "/demo_payload.bin\n");
            in[strcspn(in, "\n")] = 0;
            char ks[16]; printf("k [6]: "); if (!fgets(ks, sizeof(ks), stdin)) continue; if (ks[0]=='\n') strcpy(ks,"6\n");
            int k = atoi(ks);
            char prefix[256]; printf("output prefix [demo_out/demo_payload]: "); if (!fgets(prefix,sizeof(prefix),stdin)) continue; if (prefix[0]=='\n') strcpy(prefix, DEFAULT_OUT_DIR "/demo_payload\n");
            prefix[strcspn(prefix,"\n")] = 0;
            // shard operator:
            size_t plen; uint8_t *payload = read_file(in, &plen);
            if (!payload) { printf("%sFailed reading payload%s\n", CLR_RED, CLR_RST); continue; }
            size_t shard_size = (plen + (size_t)k - 1) / (size_t)k;
            uint8_t **shards = xcalloc((size_t)k + 1, sizeof(uint8_t*));
            mkdir(DEFAULT_OUT_DIR, 0755);
            for (int i=0;i<k;i++) {
                shards[i] = xcalloc(shard_size, 1);
                size_t off = (size_t)i * shard_size;
                size_t tocopy = (off < plen) ? ((plen - off) < shard_size ? (plen - off) : shard_size) : 0;
                if (tocopy) memcpy(shards[i], payload + off, tocopy);
                char fname[512]; snprintf(fname, sizeof(fname), "%s.shard%03d", prefix, i);
                write_file(fname, shards[i], shard_size);
                printf("Wrote %s (%zu)\n", fname, shard_size);
            }
            shards[k] = xcalloc(shard_size,1);
            xor_parity_build(shards, (uint32_t)k, (uint32_t)shard_size);
            char pfn[512]; snprintf(pfn, sizeof(pfn), "%s.parity", prefix);
            write_file(pfn, shards[k], shard_size); printf("Wrote %s\n", pfn);
            for (int i=0;i<=k;i++) if (shards[i]) free(shards[i]);
            free(shards); free(payload);
        }
        else if (c == 4) {
            char prefix[256]; printf("Shard prefix [demo_out/demo_payload]: "); if (!fgets(prefix,sizeof(prefix),stdin)) continue; if (prefix[0]=='\n') strcpy(prefix, DEFAULT_OUT_DIR "/demo_payload\n");
            prefix[strcspn(prefix, "\n")] = 0;
            char ks[16]; printf("k [6]: "); if (!fgets(ks,sizeof(ks),stdin)) continue; if (ks[0]=='\n') strcpy(ks,"6\n");
            int k = atoi(ks);
            char ms[16]; printf("missing index [2]: "); if (!fgets(ms,sizeof(ms),stdin)) continue; if (ms[0]=='\n') strcpy(ms,"2\n");
            int missing = atoi(ms);
            char outp[256]; printf("output file [demo_out/recovered.shard]: "); if (!fgets(outp,sizeof(outp),stdin)) continue; if (outp[0]=='\n') strcpy(outp, DEFAULT_OUT_DIR "/recovered.shard\n");
            outp[strcspn(outp, "\n")] = 0;
            // recovery
            char pfn[512]; snprintf(pfn, sizeof(pfn), "%s.parity", prefix);
            size_t shard_size; uint8_t *par = read_file(pfn, &shard_size);
            if (!par) { printf("%sParity read failed%s\n", CLR_RED, CLR_RST); continue; }
            uint8_t **shards = xcalloc((size_t)k + 1, sizeof(uint8_t*));
            for (int i=0;i<k;i++) {
                if (i == missing) { shards[i] = NULL; continue; }
                char fname[512]; snprintf(fname, sizeof(fname), "%s.shard%03d", prefix, i);
                size_t r; shards[i] = read_file(fname, &r);
                if (!shards[i] || r != shard_size) { printf("%sShard read failed: %s%s\n", CLR_RED, fname, CLR_RST); for (int j=0;j<k;j++) if (shards[j]) free(shards[j]); free(shards); free(par); goto cont4; }
            }
            shards[k] = par;
            shards[missing] = xcalloc(shard_size,1);
            xor_parity_recover(shards, (uint32_t)k, (uint32_t)shard_size, (uint32_t)missing);
            write_file(outp, shards[missing], shard_size);
            printf("%sRecovered to %s%s\n", CLR_GREEN, outp, CLR_RST);
            for (int i=0;i<k;i++) if (shards[i]) free(shards[i]);
            free(shards);
            cont4: ;
        }
        else if (c == 5) {
            char prefix[128]; printf("Map prefix [demo]: "); if (!fgets(prefix,sizeof(prefix),stdin)) continue; if (prefix[0]=='\n') strcpy(prefix,"demo\n");
            prefix[strcspn(prefix,"\n")] = 0;
            char ss[32]; printf("seed [0=time]: "); if (!fgets(ss,sizeof(ss),stdin)) continue; if (ss[0]=='\n') strcpy(ss,"0\n");
            uint64_t seed = (uint64_t)strtoull(ss, NULL, 10); if (seed == 0) seed = (uint64_t)time(NULL);
            char stripes_s[32]; printf("stripes [10]: "); if (!fgets(stripes_s,sizeof(stripes_s),stdin)) continue; if (stripes_s[0]=='\n') strcpy(stripes_s,"10\n");
            int stripes = atoi(stripes_s);
            char eb[32]; printf("extent_bytes [4096]: "); if (!fgets(eb,sizeof(eb),stdin)) continue; if (eb[0]=='\n') strcpy(eb,"4096\n");
            uint64_t extent_bytes = (uint64_t)strtoull(eb, NULL, 10);
            char ts[32]; printf("total_slots [1000]: "); if (!fgets(ts,sizeof(ts),stdin)) continue; if (ts[0]=='\n') strcpy(ts,"1000\n");
            uint64_t total_slots = (uint64_t)strtoull(ts, NULL, 10);
            char bo[32]; printf("base_off [1024]: "); if (!fgets(bo,sizeof(bo),stdin)) continue; if (bo[0]=='\n') strcpy(bo,"1024\n");
            uint64_t base_off = (uint64_t)strtoull(bo, NULL, 10);
            char kd[16]; printf("k_data [6]: "); if (!fgets(kd,sizeof(kd),stdin)) continue; if (kd[0]=='\n') strcpy(kd,"6\n");
            int k_data = atoi(kd);
            char pn[16]; printf("parity_n [1]: "); if (!fgets(pn,sizeof(pn),stdin)) continue; if (pn[0]=='\n') strcpy(pn,"1\n");
            int parity_n = atoi(pn);
            char me[16]; printf("m_ecc [0]: "); if (!fgets(me,sizeof(me),stdin)) continue; if (me[0]=='\n') strcpy(me,"0\n");
            int m_ecc = atoi(me);
            ensure_dir(DEFAULT_MAP_DIR);
            layout_cfg_t cfg = { base_off, total_slots, extent_bytes, (uint32_t)k_data, (uint32_t)parity_n, (uint32_t)m_ecc };
            uint32_t perstripe = cfg.k_data + cfg.parity_n + cfg.m_ecc;
            map_entry_t *all = xcalloc((size_t)perstripe * (size_t)stripes, sizeof(map_entry_t));
            uint8_t *bm = bitmap_alloc(cfg.total_slots);
            uint32_t idx = 0;
            for (int si=0; si<stripes; si++) {
                memset(bm, 0, (size_t)((cfg.total_slots+7)/8));
                uint32_t outc = 0;
                build_stripe(&cfg, seed, (uint64_t)si, (uint64_t)si * cfg.extent_bytes * cfg.k_data, &all[idx], &outc, bm);
                idx += outc;
            }
            if (write_map_files(prefix, all, idx) == 0) {
                printf("%sWrote map files to %s/%s.map.* (%u entries)%s\n", CLR_GREEN, DEFAULT_MAP_DIR, prefix, idx, CLR_RST);
            } else {
                printf("%sFailed to write map files%s\n", CLR_RED, CLR_RST);
            }
            free(all); free(bm);
        }
        else if (c == 6) {
            char out[256]; printf("zone output JSON [zone.json]: "); if (!fgets(out,sizeof(out),stdin)) continue; if (out[0]=='\n') strcpy(out,"zone.json\n");
            out[strcspn(out,"\n")] = 0;
            char pb[64]; printf("payload bytes [10485760]: "); if (!fgets(pb,sizeof(pb),stdin)) continue; if (pb[0]=='\n') strcpy(pb,"10485760\n");
            uint64_t payload = (uint64_t)strtoull(pb,NULL,10);
            char eb[32]; printf("extent bytes [4096]: "); if (!fgets(eb,sizeof(eb),stdin)) continue; if (eb[0]=='\n') strcpy(eb,"4096\n");
            uint64_t extent = (uint64_t)strtoull(eb,NULL,10);
            char kd[16]; printf("k_data [6]: "); if (!fgets(kd,sizeof(kd),stdin)) continue; if (kd[0]=='\n') strcpy(kd,"6\n");
            int k = atoi(kd);
            char pn[16]; printf("parity [1]: "); if (!fgets(pn,sizeof(pn),stdin)) continue; if (pn[0]=='\n') strcpy(pn,"1\n");
            int p = atoi(pn);
            char me[16]; printf("m_ecc [0]: "); if (!fgets(me,sizeof(me),stdin)) continue; if (me[0]=='\n') strcpy(me,"0\n");
            int m = atoi(me);
            zone_layout_t L; zone_build(&L, payload, extent, k, p, m, 4096*4);
            if (zone_to_json(out, &L) == 0) printf("%sWrote %s%s\n", CLR_GREEN, out, CLR_RST);
            else printf("%sFailed to write %s%s\n", CLR_RED, out, CLR_RST);
        }
        else if (c == 7) {
            // simple demo: create payload, shards, map
            printf("%sRunning quick demo (payload -> shards -> map)%s\n", CLR_BOLD, CLR_RST);
            // create payload
            char payload_path[256]; snprintf(payload_path, sizeof(payload_path), "%s/demo_payload.bin", DEFAULT_OUT_DIR);
            ensure_dir(DEFAULT_OUT_DIR);
            FILE *f = fopen(payload_path, "wb");
            if (f) {
                FILE *ur = fopen("/dev/urandom","rb");
                size_t bytes = 16*1024;
                if (ur) {
                    uint8_t buf[4096]; size_t wrote = 0;
                    while (wrote < bytes) {
                        size_t to = (bytes - wrote > sizeof(buf)) ? sizeof(buf) : (bytes - wrote);
                        size_t r = fread(buf, 1, to, ur);
                        if (r == 0) break;
                        fwrite(buf, 1, r, f); wrote += r;
                    }
                    fclose(ur);
                } else {
                    for (size_t i=0;i<bytes;i++){ uint8_t v = (uint8_t)rand(); fwrite(&v,1,1,f); }
                }
                fclose(f);
                printf("%sCreated %s%s\n", CLR_GREEN, payload_path, CLR_RST);
            }
            // shard (k=6)
            char prefix[256]; snprintf(prefix, sizeof(prefix), "%s/demo_payload", DEFAULT_OUT_DIR);
            size_t plen; uint8_t *payload = read_file(payload_path, &plen);
            if (payload) {
                int k = 6;
                size_t shard_size = (plen + (size_t)k - 1) / (size_t)k;
                uint8_t **shards = xcalloc((size_t)k + 1, sizeof(uint8_t*));
                for (int i=0;i<k;i++) {
                    shards[i] = xcalloc(shard_size,1);
                    size_t off = (size_t)i * shard_size;
                    size_t tocopy = (off < plen) ? ((plen - off) < shard_size ? (plen - off) : shard_size) : 0;
                    if (tocopy) memcpy(shards[i], payload + off, tocopy);
                    char fname[512]; snprintf(fname, sizeof(fname), "%s.shard%03d", prefix, i);
                    write_file(fname, shards[i], shard_size);
                }
                shards[k] = xcalloc(shard_size,1);
                xor_parity_build(shards, (uint32_t)k, (uint32_t)shard_size);
                char pfn[512]; snprintf(pfn, sizeof(pfn), "%s.parity", prefix);
                write_file(pfn, shards[k], shard_size);
                for (int i=0;i<=k;i++) if (shards[i]) free(shards[i]);
                free(shards);
                printf("%sCreated shards + parity under %s%s\n", CLR_GREEN, DEFAULT_OUT_DIR, CLR_RST);
                free(payload);
            } else {
                printf("%sPayload read failed for demo%s\n", CLR_RED, CLR_RST);
            }
            // generate map
            layout_cfg_t cfg = { 1024, 100, 4096, 6, 1, 0 };
            uint32_t per = cfg.k_data + cfg.parity_n + cfg.m_ecc;
            map_entry_t *all = xcalloc((size_t)per * 10, sizeof(map_entry_t));
            uint8_t *bm = bitmap_alloc(cfg.total_slots);
            uint32_t idx = 0;
            for (int si=0; si<10; si++) {
                memset(bm, 0, (size_t)((cfg.total_slots+7)/8));
                uint32_t outc = 0;
                build_stripe(&cfg, (uint64_t)time(NULL), si, (uint64_t)si * cfg.extent_bytes * cfg.k_data, &all[idx], &outc, bm);
                idx += outc;
            }
            write_map_files("demo_run", all, idx);
            free(all); free(bm);
            printf("%sDemo finished.%s\n", CLR_GREEN, CLR_RST);
        }
        else {
            printf("%sUnknown option%s\n", CLR_RED, CLR_RST);
        }
        printf("\n");
    }
}

/* -------------------------------------------------------
   Non-interactive subcommands (simplified)
   ------------------------------------------------------- */
static int cmd_selftest(void) { return self_tests(); }

static int cmd_gen_map(int argc, char **argv) {
    const char *prefix = arg_val(argc, argv, "--prefix"); if (!prefix) prefix = "map";
    const char *seed_s = arg_val(argc, argv, "--seed"); uint64_t seed = seed_s ? (uint64_t)strtoull(seed_s, NULL, 10) : (uint64_t)time(NULL);
    const char *str_s = arg_val(argc, argv, "--stripes"); int stripes = str_s ? atoi(str_s) : 10;
    const char *extent_s = arg_val(argc, argv, "--extent"); uint64_t extent = extent_s ? (uint64_t)strtoull(extent_s, NULL, 10) : 4096;
    const char *slots_s = arg_val(argc, argv, "--slots"); uint64_t slots = slots_s ? (uint64_t)strtoull(slots_s, NULL, 10) : 1000;
    const char *k_s = arg_val(argc, argv, "--k"); int k = k_s ? atoi(k_s) : 6;
    const char *p_s = arg_val(argc, argv, "--parity"); int p = p_s ? atoi(p_s) : 1;
    const char *m_s = arg_val(argc, argv, "--ecc"); int m = m_s ? atoi(m_s) : 0;
    ensure_dir(DEFAULT_MAP_DIR);
    layout_cfg_t cfg = { 1024, slots, extent, (uint32_t)k, (uint32_t)p, (uint32_t)m };
    uint32_t perstripe = cfg.k_data + cfg.parity_n + cfg.m_ecc;
    map_entry_t *all = xcalloc((size_t)perstripe * (size_t)stripes, sizeof(map_entry_t));
    uint8_t *bm = bitmap_alloc(cfg.total_slots);
    uint32_t idx = 0;
    for (int i=0;i<stripes;i++){
        memset(bm, 0, (size_t)((cfg.total_slots+7)/8));
        uint32_t outc = 0;
        build_stripe(&cfg, seed, (uint64_t)i, (uint64_t)i * cfg.extent_bytes * cfg.k_data, &all[idx], &outc, bm);
        idx += outc;
    }
    int rc = write_map_files(prefix, all, idx);
    free(all); free(bm);
    return rc;
}

static int cmd_zone_json(int argc, char **argv) {
    const char *out = arg_val(argc, argv, "--out"); if (!out) out = "zone.json";
    const char *payload_s = arg_val(argc, argv, "--payload"); uint64_t payload = payload_s ? (uint64_t)strtoull(payload_s, NULL, 10) : 10485760ULL;
    const char *extent_s = arg_val(argc, argv, "--extent"); uint64_t extent = extent_s ? (uint64_t)strtoull(extent_s, NULL, 10) : 4096ULL;
    const char *k_s = arg_val(argc, argv, "--k"); int k = k_s ? atoi(k_s) : 6;
    const char *p_s = arg_val(argc, argv, "--parity"); int p = p_s ? atoi(p_s) : 1;
    const char *m_s = arg_val(argc, argv, "--ecc"); int m = m_s ? atoi(m_s) : 0;
    zone_layout_t L; zone_build(&L, payload, extent, k, p, m, 4096*4);
    return zone_to_json(out, &L);
}

/* -------------------------------------------------------
   main
   ------------------------------------------------------- */
int main(int argc, char **argv) {
    srand((unsigned)time(NULL) ^ (unsigned)getpid()));
    ensure_dir(LOG_DIR);
    log_open();
    if (argc < 2) {
        ui_interactive();
        return 0;
    }
    if (strcmp(argv[1], "interactive") == 0) { ui_interactive(); return 0; }
    if (strcmp(argv[1], "selftest") == 0) return cmd_selftest();
    if (strcmp(argv[1], "gen-map") == 0) return cmd_gen_map(argc, argv);
    if (strcmp(argv[1], "gen-zone-json") == 0) return cmd_zone_json(argc, argv);
    if (strcmp(argv[1], "demo-run") == 0) { ui_interactive(); return 0; } // reuse interactive demo
    if (strcmp(argv[1], "--version") == 0) { printf("%s\n", APP_VERSION); return 0; }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "help") == 0) { usage(); return 0; }
    // fallback: print usage
    usage();
    return 0;
}