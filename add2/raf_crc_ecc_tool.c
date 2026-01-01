/*
 * raf_crc_ecc_tool.c
 * RAFAELIA :: BITRAF64 :: CRC + PARITY (CLI tool)
 *
 * Implements:
 *  - raf_hdr64_t header init/finalize/verify
 *  - CRC32C (tableless), CRC64-ECMA (optional)
 *  - payload checks compute/verify
 *  - parity XOR shard make/recover
 *  - interactive CLI menu (ASCII art), non-penalizing logic, benchmarks, integrity tests
 *
 * Build:
 *   clang -O3 -std=c11 -Wall -Wextra raf_crc_ecc_tool.c -o raf_crc_ecc_tool -lm
 *
 * Notes:
 *  - This tool is intended as a pragmatic, auditable helper for integration in Bitraf-like flows.
 *  - Not cryptographic authenticity. Use BLAKE3/SHA3 for identity/proofs (not included).
 *
 * Author: RAFAELIA Copilot (on request of rafaelmeloreisnovo)
 * License: Please refer to ZIPRAF_OMEGA_FULL licensing module in repo (ativar.txt)
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>

#define RAF_HDR_SIZE 64
#define RAF_MAGIC0 'R'
#define RAF_MAGIC1 'A'
#define RAF_MAGIC2 'F'
#define RAF_MAGIC3 'B'

typedef struct __attribute__((packed)) {
  uint8_t magic[4];      // "RAFB"
  uint8_t ver;
  uint8_t flags;
  uint16_t hdr_len_le;   // 64
  uint64_t session_seed_le;
  uint64_t block_index_le;
  uint64_t payload_len_le;
  uint8_t  payload_prefix8[8]; // e.g. first 8 bytes of BLAKE3(payload) OR other
  uint32_t hdr_crc32c_le;      // CRC32C over header with this field zeroed
  uint8_t  pre6[16];
} raf_hdr64_t;

_Static_assert(sizeof(raf_hdr64_t) == 64, "Header must be exactly 64 bytes");

/* ---------------------------
   Endianness helpers
---------------------------- */
static inline uint16_t raf_le16(uint16_t x) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return x;
#else
  return (uint16_t)((x >> 8) | (x << 8));
#endif
}
static inline uint32_t raf_le32(uint32_t x) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return x;
#else
  return __builtin_bswap32(x);
#endif
}
static inline uint64_t raf_le64(uint64_t x) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return x;
#else
  return __builtin_bswap64(x);
#endif
}

/* =========================================================
   CRC32C (Castagnoli) - compact reflected bitwise
   ========================================================= */
static inline uint32_t raf_crc32c_update(uint32_t crc, const uint8_t *p, size_t n) {
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
static inline uint32_t raf_crc32c(const void *data, size_t len) {
  return raf_crc32c_update(0u, (const uint8_t*)data, len);
}

/* =========================================================
   CRC64-ECMA - compact reflected bitwise (optional)
   ========================================================= */
static inline uint64_t raf_crc64_ecma_update(uint64_t crc, const uint8_t *p, size_t n) {
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
static inline uint64_t raf_crc64_ecma(const void *data, size_t len) {
  return raf_crc64_ecma_update(0ull, (const uint8_t*)data, len);
}

/* =========================================================
   Header helpers
   ========================================================= */
static inline void raf_hdr_init(
  raf_hdr64_t *h,
  uint8_t ver,
  uint8_t flags,
  uint64_t seed,
  uint64_t idx,
  uint64_t plen,
  const uint8_t pre6_16[16]
) {
  h->magic[0] = RAF_MAGIC0;
  h->magic[1] = RAF_MAGIC1;
  h->magic[2] = RAF_MAGIC2;
  h->magic[3] = RAF_MAGIC3;
  h->ver = ver;
  h->flags = flags;
  h->hdr_len_le = raf_le16((uint16_t)RAF_HDR_SIZE);
  h->session_seed_le = raf_le64(seed);
  h->block_index_le  = raf_le64(idx);
  h->payload_len_le  = raf_le64(plen);

  for (int i=0;i<8;i++) h->payload_prefix8[i]=0;
  h->hdr_crc32c_le = 0;

  if (pre6_16) {
    for (int i=0;i<16;i++) h->pre6[i]=pre6_16[i];
  } else {
    for (int i=0;i<16;i++) h->pre6[i]=0;
  }
}

static inline uint32_t raf_hdr_crc32c(const raf_hdr64_t *h) {
  raf_hdr64_t tmp = *h;
  tmp.hdr_crc32c_le = 0;
  uint32_t c = raf_crc32c(&tmp, sizeof(tmp));
  return c;
}
static inline int raf_hdr_finalize_crc(raf_hdr64_t *h) {
  if (!h) return -1;
  uint32_t c = raf_hdr_crc32c(h);
  h->hdr_crc32c_le = raf_le32(c);
  return 0;
}
static inline int raf_hdr_verify_crc(const raf_hdr64_t *h) {
  if (!h) return -1;
  uint32_t want = raf_le32(h->hdr_crc32c_le);
  uint32_t got  = raf_hdr_crc32c(h);
  return (want == got) ? 0 : -2;
}

/* =========================================================
   Payload checks
   ========================================================= */
typedef struct __attribute__((packed)) {
  uint32_t payload_crc32c_le;
  uint64_t payload_crc64_le; // optional; can be 0 if not used
} raf_payload_checks_t;

static inline void raf_payload_checks(
  const uint8_t *payload, size_t plen,
  raf_payload_checks_t *out,
  int use_crc64
) {
  uint32_t c32 = raf_crc32c(payload, plen);
  out->payload_crc32c_le = raf_le32(c32);

  if (use_crc64) {
    uint64_t c64 = raf_crc64_ecma(payload, plen);
    out->payload_crc64_le = raf_le64(c64);
  } else {
    out->payload_crc64_le = 0;
  }
}

static inline int raf_payload_verify(
  const uint8_t *payload, size_t plen,
  const raf_payload_checks_t *chk,
  int use_crc64
) {
  uint32_t want32 = raf_le32(chk->payload_crc32c_le);
  uint32_t got32  = raf_crc32c(payload, plen);
  if (want32 != got32) return -1;

  if (use_crc64) {
    uint64_t want64 = raf_le64(chk->payload_crc64_le);
    uint64_t got64  = raf_crc64_ecma(payload, plen);
    if (want64 != got64) return -2;
  }
  return 0;
}

/* =========================================================
   ECC-lite: XOR parity stripes
   ========================================================= */
static inline void raf_xor_buf(uint8_t *dst, const uint8_t *src, size_t n) {
  for (size_t i=0;i<n;i++) dst[i] ^= src[i];
}

static inline void raf_parity_xor_make(
  const uint8_t *const *shards,
  size_t k,
  uint8_t *parity,
  size_t shard_size
) {
  for (size_t i=0;i<shard_size;i++) parity[i]=0;
  for (size_t i=0;i<k;i++) raf_xor_buf(parity, shards[i], shard_size);
}

static inline void raf_parity_xor_recover_one(
  const uint8_t *const *shards, // shards pointers, missing one can be NULL
  size_t k,
  size_t missing_i,
  const uint8_t *parity,
  uint8_t *out_missing,
  size_t shard_size
) {
  for (size_t j=0;j<shard_size;j++) out_missing[j]=parity[j];
  for (size_t i=0;i<k;i++) {
    if (i==missing_i) continue;
    raf_xor_buf(out_missing, shards[i], shard_size);
  }
}

/* =========================================================
   Utilities: file read/write, simple ASCII graphs, timing
   ========================================================= */
static uint8_t *read_file(const char *path, size_t *out_len) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
  long sz = ftell(f);
  if (sz < 0) { fclose(f); return NULL; }
  rewind(f);
  uint8_t *buf = malloc((size_t)sz + 1);
  if (!buf) { fclose(f); return NULL; }
  size_t r = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  if (r != (size_t)sz) { free(buf); return NULL; }
  *out_len = r;
  return buf;
}

static int write_file(const char *path, const uint8_t *buf, size_t len) {
  FILE *f = fopen(path, "wb");
  if (!f) return -1;
  size_t w = fwrite(buf, 1, len, f);
  fclose(f);
  return (w == len) ? 0 : -2;
}

static void ascii_banner(void) {
  puts("========================================");
  puts("  RAF CRC+ECC TOOL  -  RAFAELIA / BITRAF");
  puts("  (crc32c, crc64-ecma, xor-parity, bench)");
  puts("========================================");
}

static void ascii_menu(void) {
  puts("");
  puts("Menu:");
  puts("  1) Compute header+payload checks from file");
  puts("  2) Verify file with given header/checks");
  puts("  3) Create parity shards from file (k shards)");
  puts("  4) Recover a missing shard (XOR parity)");
  puts("  5) Run micro-benchmarks (CRC & parity)");
  puts("  6) Run integrity self-tests");
  puts("  0) Exit");
  printf("Choice: ");
}

/* Simple horizontal ASCII bar for throughput */
static void print_bar(double value, double max, int width) {
  int filled = (int)((value / max) * width + 0.5);
  if (filled < 0) filled = 0;
  if (filled > width) filled = width;
  putchar('[');
  for (int i=0;i<filled;i++) putchar('=');
  for (int i=filled;i<width;i++) putchar(' ');
  putchar(']');
}

/* Time helpers */
static double diff_seconds(struct timespec a, struct timespec b) {
  double sec = (double)(b.tv_sec - a.tv_sec);
  double nsec = (double)(b.tv_nsec - a.tv_nsec) * 1e-9;
  return sec + nsec;
}

/* =========================================================
   CLI operations
   ========================================================= */

static int op_compute_checks(const char *inpath, const char *out_hdr_path, int use_crc64, int ver, int flags) {
  size_t plen;
  uint8_t *payload = read_file(inpath, &plen);
  if (!payload) {
    fprintf(stderr, "Failed to read input file: %s\n", strerror(errno));
    return -1;
  }

  raf_hdr64_t hdr;
  uint8_t pre6[16];
  /* simple: fill pre6 with first 16 bytes (or zeros) */
  for (size_t i=0;i<16;i++) pre6[i] = (i < plen) ? payload[i] : 0;

  raf_hdr_init(&hdr, (uint8_t)ver, (uint8_t)flags, (uint64_t)time(NULL), 0, (uint64_t)plen, pre6);
  raf_hdr_finalize_crc(&hdr);

  raf_payload_checks_t pc;
  raf_payload_checks(payload, plen, &pc, use_crc64);

  /* write header + checks to out_hdr_path as a compact blob (header + checks) */
  uint8_t outbuf[sizeof(hdr) + sizeof(pc)];
  memcpy(outbuf, &hdr, sizeof(hdr));
  memcpy(outbuf + sizeof(hdr), &pc, sizeof(pc));

  int rc = write_file(out_hdr_path, outbuf, sizeof(outbuf));
  if (rc != 0) {
    fprintf(stderr, "Failed to write header/check file: %d\n", rc);
    free(payload);
    return -2;
  }

  printf("Header+checks written to %s (payload %zu bytes). use_crc64=%d\n", out_hdr_path, plen, use_crc64);
  free(payload);
  return 0;
}

static int op_verify(const char *inpath, const char *hdr_path, int use_crc64) {
  size_t plen;
  uint8_t *payload = read_file(inpath, &plen);
  if (!payload) {
    fprintf(stderr, "Failed to read input file: %s\n", strerror(errno));
    return -1;
  }

  size_t hdrlen;
  uint8_t *hdrbuf = read_file(hdr_path, &hdrlen);
  if (!hdrbuf || hdrlen < sizeof(raf_hdr64_t) + sizeof(raf_payload_checks_t)) {
    fprintf(stderr, "Failed to read header/check file\n");
    free(payload);
    free(hdrbuf);
    return -2;
  }
  raf_hdr64_t hdr;
  raf_payload_checks_t pc;
  memcpy(&hdr, hdrbuf, sizeof(hdr));
  memcpy(&pc, hdrbuf + sizeof(hdr), sizeof(pc));

  int rc = raf_hdr_verify_crc(&hdr);
  if (rc != 0) {
    printf("Header CRC verification failed (code %d)\n", rc);
    free(payload); free(hdrbuf);
    return -3;
  } else {
    printf("Header CRC ok\n");
  }

  int rc2 = raf_payload_verify(payload, plen, &pc, use_crc64);
  if (rc2 == 0) {
    printf("Payload checks PASS (crc32c%s)\n", use_crc64 ? " + crc64" : "");
  } else {
    printf("Payload checks FAIL (code %d)\n", rc2);
    free(payload); free(hdrbuf);
    return -4;
  }

  free(payload); free(hdrbuf);
  return 0;
}

/* Shard creation: split file into k shards of equal size (pad with zeros), compute parity and write shards */
static int op_create_shards(const char *inpath, int k, const char *out_prefix) {
  if (k < 1) return -1;
  size_t plen;
  uint8_t *payload = read_file(inpath, &plen);
  if (!payload) { fprintf(stderr,"read failed\n"); return -2; }

  size_t shard_size = (plen + k - 1) / k;
  /* allocate pointer array */
  uint8_t **shards = calloc((size_t)k, sizeof(uint8_t*));
  if (!shards) { free(payload); return -3; }

  for (int i=0;i<k;i++) {
    shards[i] = calloc(shard_size, 1);
    if (!shards[i]) {
      for (int j=0;j<i;j++) free(shards[j]);
      free(shards); free(payload);
      return -4;
    }
    size_t offset = (size_t)i * shard_size;
    size_t tocopy = (offset < plen) ? ( (plen - offset) < shard_size ? (plen - offset) : shard_size ) : 0;
    if (tocopy) memcpy(shards[i], payload + offset, tocopy);
    char fname[512];
    snprintf(fname, sizeof(fname), "%s.shard%03d", out_prefix, i);
    if (write_file(fname, shards[i], shard_size) != 0) {
      fprintf(stderr, "Failed to write shard %d\n", i);
    } else {
      printf("Wrote %s (%zu bytes)\n", fname, shard_size);
    }
  }

  /* parity shard */
  uint8_t *parity = calloc(shard_size,1);
  if (!parity) { for (int i=0;i<k;i++) free(shards[i]); free(shards); free(payload); return -5; }
  const uint8_t *const *shards_const = (const uint8_t *const *)shards;
  raf_parity_xor_make(shards_const, (size_t)k, parity, shard_size);
  char pfn[512];
  snprintf(pfn, sizeof(pfn), "%s.parity", out_prefix);
  if (write_file(pfn, parity, shard_size) != 0) {
    fprintf(stderr, "Failed to write parity\n");
  } else {
    printf("Wrote parity %s (%zu bytes)\n", pfn, shard_size);
  }

  for (int i=0;i<k;i++) free(shards[i]);
  free(shards);
  free(parity);
  free(payload);
  return 0;
}

/* Recover missing shard i given other shards + parity (files named prefix.shardNNN and prefix.parity) */
static int op_recover_shard(const char *out_prefix, int k, int missing_i, const char *out_recovered_path) {
  if (k < 1) return -1;
  /* read parity first to know shard_size */
  char pfn[512];
  snprintf(pfn, sizeof(pfn), "%s.parity", out_prefix);
  size_t shard_size;
  uint8_t *parity = read_file(pfn, &shard_size);
  if (!parity) { fprintf(stderr, "Failed to read parity\n"); return -2; }

  uint8_t **shards = calloc((size_t)k, sizeof(uint8_t*));
  if (!shards) { free(parity); return -3; }

  for (int i=0;i<k;i++) {
    if (i == missing_i) {
      shards[i] = NULL; continue;
    }
    char fname[512];
    snprintf(fname, sizeof(fname), "%s.shard%03d", out_prefix, i);
    size_t rlen;
    shards[i] = read_file(fname, &rlen);
    if (!shards[i] || rlen != shard_size) {
      fprintf(stderr, "Failed to read shard %d or size mismatch\n", i);
      for (int j=0;j<k;j++) if (shards[j]) free(shards[j]);
      free(shards); free(parity);
      return -4;
    }
  }

  uint8_t *recovered = calloc(shard_size,1);
  if (!recovered) { free(parity); for (int j=0;j<k;j++) if (shards[j]) free(shards[j]); free(shards); return -5; }

  const uint8_t *const *shards_const = (const uint8_t *const *)shards;
  raf_parity_xor_recover_one(shards_const, (size_t)k, (size_t)missing_i, parity, recovered, shard_size);

  if (write_file(out_recovered_path, recovered, shard_size) != 0) {
    fprintf(stderr, "Failed to write recovered shard\n");
  } else {
    printf("Recovered shard written to %s (%zu bytes)\n", out_recovered_path, shard_size);
  }

  for (int j=0;j<k;j++) if (shards[j]) free(shards[j]);
  free(shards); free(parity); free(recovered);
  return 0;
}

/* Micro-benchmarks: measure CRC32C and parity speed for sizes up to a few MB */
static int op_bench(void) {
  const size_t sizes[] = {1024, 8*1024, 64*1024, 256*1024, 1024*1024, 4*1024*1024};
  const int nsizes = (int)(sizeof(sizes)/sizeof(sizes[0]));
  const int iter = 16;

  printf("Micro-benchmarks (CRC32C, CRC64 optional, XOR parity)\n");
  printf("Allocating buffer up to %zu bytes\n", sizes[nsizes-1]);
  uint8_t *buf = malloc(sizes[nsizes-1]);
  if (!buf) {
    fprintf(stderr, "Failed to allocate benchmark buffer\n");
    return -1;
  }
  for (size_t i=0;i<sizes[nsizes-1];i++) buf[i] = (uint8_t)(i & 0xff);

  double max_throughput = 0.0;
  for (int si=0; si<nsizes; si++) {
    size_t s = sizes[si];
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    uint32_t crc = 0;
    for (int it=0; it<iter; it++) {
      crc = raf_crc32c_update(crc, buf, s);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double secs = diff_seconds(t0, t1);
    double bytes = (double)s * iter;
    double MBps = (bytes / secs) / (1024.0*1024.0);
    if (MBps > max_throughput) max_throughput = MBps;
    printf("CRC32C %7zu bytes: %7.2f MB/s (crc=0x%08x)\n", s, MBps, crc);
  }

  /* Parity benchmark: create k shards and XOR them */
  const int k = 8;
  size_t shard_size = 256*1024;
  uint8_t **shards = malloc((size_t)k * sizeof(uint8_t*));
  if (!shards) {
    fprintf(stderr, "Failed to allocate shard pointers\n");
    free(buf);
    return -2;
  }
  for (int i=0;i<k;i++) shards[i] = NULL;
  for (int i=0;i<k;i++) {
    shards[i] = malloc(shard_size);
    if (!shards[i]) {
      fprintf(stderr, "Failed to allocate shard %d\n", i);
      for (int j=0;j<k;j++) free(shards[j]);
      free(shards);
      free(buf);
      return -3;
    }
    for (size_t j=0;j<shard_size;j++) shards[i][j] = (uint8_t)((i+j) & 0xff);
  }
  uint8_t *parity = malloc(shard_size);
  if (!parity) {
    fprintf(stderr, "Failed to allocate parity buffer\n");
    for (int i=0;i<k;i++) free(shards[i]);
    free(shards);
    free(buf);
    return -4;
  }
  memset(parity, 0, shard_size);
  struct timespec p0, p1;
  clock_gettime(CLOCK_MONOTONIC, &p0);
  for (int it=0; it<64; it++) {
    for (int i=0;i<k;i++) raf_xor_buf(parity, shards[i], shard_size);
    /* reset parity for next iter */
    for (size_t j=0;j<shard_size;j++) parity[j]=0;
  }
  clock_gettime(CLOCK_MONOTONIC, &p1);
  double psec = diff_seconds(p0, p1);
  double pbytes = (double)k * shard_size * 64;
  double pMBps = (pbytes / psec) / (1024.0*1024.0);
  printf("XOR parity (%d shards x %zu bytes) : %7.2f MB/s\n", k, shard_size, pMBps);

  for (int i=0;i<k;i++) free(shards[i]);
  free(shards);
  free(parity);
  free(buf);
  (void)max_throughput;
  return 0;
}

/* Simple self-tests for correctness */
static int op_selftests(void) {
  puts("Running integrity self-tests...");

  /* Test vector for CRC32C: known vector "123456789" -> 0xe3069283 (Castagnoli) */
  const uint8_t tv1[] = "123456789";
  uint32_t c = raf_crc32c(tv1, 9);
  if (c != 0xE3069283u) {
    printf("CRC32C test FAILED: got 0x%08x expected 0xE3069283\n", c);
    return -1;
  } else {
    printf("CRC32C known-vector PASS\n");
  }

  /* CRC64 known vector "123456789" -> 0x6C40DF5F0B497347 for ECMA-182 */
  uint64_t c64 = raf_crc64_ecma(tv1, 9);
  if (c64 != 0x6C40DF5F0B497347ull) {
    printf("CRC64-ECMA test FAILED: got 0x%016" PRIx64 " expected 0x6C40DF5F0B497347\n", c64);
    return -2;
  } else {
    printf("CRC64-ECMA known-vector PASS\n");
  }

  /* Parity test */
  const int k = 4;
  size_t shard_size = 1024;
  uint8_t *s0 = calloc(shard_size,1);
  uint8_t *s1 = calloc(shard_size,1);
  uint8_t *s2 = calloc(shard_size,1);
  uint8_t *s3 = calloc(shard_size,1);
  if (!s0 || !s1 || !s2 || !s3) {
    fprintf(stderr, "Parity test allocation failed\n");
    free(s0); free(s1); free(s2); free(s3);
    return -3;
  }
  for (size_t i=0;i<shard_size;i++) {
    s0[i] = (uint8_t)(i & 0xff);
    s1[i] = (uint8_t)((i*3) & 0xff);
    s2[i] = (uint8_t)((i*7) & 0xff);
    s3[i] = (uint8_t)((i*13) & 0xff);
  }
  const uint8_t *const shards[] = {s0,s1,s2,s3};
  uint8_t *par = calloc(shard_size,1);
  if (!par) {
    fprintf(stderr, "Parity buffer allocation failed\n");
    free(s0); free(s1); free(s2); free(s3);
    return -3;
  }
  raf_parity_xor_make(shards, (size_t)k, par, shard_size);

  /* simulate missing s2 */
  uint8_t *rec = calloc(shard_size,1);
  if (!rec) {
    fprintf(stderr, "Recovery buffer allocation failed\n");
    free(s0); free(s1); free(s2); free(s3); free(par);
    return -3;
  }
  uint8_t *shards_with_null[4];
  shards_with_null[0]=s0; shards_with_null[1]=s1; shards_with_null[2]=NULL; shards_with_null[3]=s3;
  raf_parity_xor_recover_one((const uint8_t *const *)shards_with_null, (size_t)k, 2, par, rec, shard_size);
  if (memcmp(rec, s2, shard_size) != 0) {
    printf("Parity recover FAILED\n");
    free(s0); free(s1); free(s2); free(s3); free(par); free(rec);
    return -3;
  } else {
    printf("Parity recover PASS\n");
  }
  free(s0); free(s1); free(s2); free(s3); free(par); free(rec);

  puts("All self-tests PASSED");
  return 0;
}

/* Interactive loop (simple) */
static void interactive_loop(void) {
  while (1) {
    ascii_banner();
    ascii_menu();
    int choice = 0;
    if (scanf("%d", &choice) != 1) { /* flush */ int c; while((c=getchar())!=EOF && c!='\n'); continue; }
    if (choice == 0) { puts("Bye."); break; }

    if (choice == 1) {
      char in[512], out[512];
      int use_crc64 = 0;
      int ver = 1, flags = 0;
      printf("Input file path: "); scanf("%511s", in);
      printf("Header output path: "); scanf("%511s", out);
      printf("Use CRC64? (0/1): "); scanf("%d", &use_crc64);
      op_compute_checks(in, out, use_crc64, ver, flags);
      puts("Press Enter to continue...");
      getchar(); getchar();
    } else if (choice == 2) {
      char in[512], hdr[512];
      int use_crc64 = 0;
      printf("Input file path: "); scanf("%511s", in);
      printf("Header/check file path: "); scanf("%511s", hdr);
      printf("Use CRC64? (0/1): "); scanf("%d", &use_crc64);
      op_verify(in, hdr, use_crc64);
      puts("Press Enter to continue...");
      getchar(); getchar();
    } else if (choice == 3) {
      char in[512], pref[256];
      int k;
      printf("Input file path: "); scanf("%511s", in);
      printf("Number of shards (k, >0): "); scanf("%d", &k);
      printf("Output prefix (files will be prefix.shardNNN and prefix.parity): "); scanf("%255s", pref);
      op_create_shards(in, k, pref);
      puts("Press Enter to continue...");
      getchar(); getchar();
    } else if (choice == 4) {
      char pref[256], out_recover[512];
      int k, missing;
      printf("Shard set prefix: "); scanf("%255s", pref);
      printf("Total shards (k): "); scanf("%d", &k);
      printf("Missing shard index (0..k-1): "); scanf("%d", &missing);
      printf("Output recovered file path: "); scanf("%511s", out_recover);
      op_recover_shard(pref, k, missing, out_recover);
      puts("Press Enter to continue...");
      getchar(); getchar();
    } else if (choice == 5) {
      op_bench();
      puts("Press Enter to continue...");
      getchar(); getchar();
    } else if (choice == 6) {
      op_selftests();
      puts("Press Enter to continue...");
      getchar(); getchar();
    } else {
      puts("Unknown option");
    }
  }
}

/* Non-interactive mode: parse simple args */
static void usage_and_exit(const char *p) {
  printf("Usage: %s [--interactive] [--compute in out_hdr [use_crc64]] [--verify in hdr [use_crc64]] [--shards in k out_prefix] [--recover prefix k missing out_recovered] [--bench] [--selftest]\n", p);
  exit(1);
}

int main(int argc, char **argv) {
  if (argc == 1) {
    interactive_loop();
    return 0;
  }

  if (argc >= 2) {
    if (strcmp(argv[1], "--interactive") == 0) {
      interactive_loop();
      return 0;
    } else if (strcmp(argv[1], "--compute") == 0 && argc >= 4) {
      const char *in = argv[2];
      const char *out = argv[3];
      int use_crc64 = (argc >= 5) ? atoi(argv[4]) : 0;
      return op_compute_checks(in, out, use_crc64, 1, 0);
    } else if (strcmp(argv[1], "--verify") == 0 && argc >= 4) {
      const char *in = argv[2];
      const char *hdr = argv[3];
      int use_crc64 = (argc >= 5) ? atoi(argv[4]) : 0;
      return op_verify(in, hdr, use_crc64);
    } else if (strcmp(argv[1], "--shards") == 0 && argc >= 5) {
      const char *in = argv[2];
      int k = atoi(argv[3]);
      const char *pref = argv[4];
      return op_create_shards(in, k, pref);
    } else if (strcmp(argv[1], "--recover") == 0 && argc >= 6) {
      const char *pref = argv[2];
      int k = atoi(argv[3]);
      int missing = atoi(argv[4]);
      const char *outf = argv[5];
      return op_recover_shard(pref, k, missing, outf);
    } else if (strcmp(argv[1], "--bench") == 0) {
      return op_bench();
    } else if (strcmp(argv[1], "--selftest") == 0) {
      return op_selftests();
    }
  }

  usage_and_exit(argv[0]);
  return 0;
}
