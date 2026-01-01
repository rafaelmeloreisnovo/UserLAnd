#!/usr/bin/env sh
set -eu

ROOT="rafaelia_os"
mkdir -p "$ROOT"/{src,include,tools,out,docs}

# -------------------------
# LICENSE / HEADER TEMPLATE
# -------------------------
cat << 'L' > "$ROOT/LICENSE_RAFAELIA.md"
RAFAELIA License Draft (RAFCODE-Φ / Pre6seal)
=============================================
This is a custom license draft intended to express authorship + ethical constraints.
It is NOT legal advice. Review with a qualified attorney for enforceability.

Core principles:
- Preserve authorship attribution and integrity seals (ΣΩΔΦBITRAF, RAFCODE-Φ, bitraf64).
- Human-protective precedence: if any policy conflicts, choose the most protective to humans.
- Security-by-default: integrity proofs (hashchains), auditability, and minimization of sensitive data.

International references (non-exhaustive, informational):
- Berne Convention, WIPO treaties
- Human rights principles
- Data protection principles (LGPD/GDPR-inspired)

No warranty.
L

cat << 'H' > "$ROOT/include/raf_core.h"
#ifndef RAF_CORE_H
#define RAF_CORE_H

#include <stddef.h>
#include <stdint.h>

/* =========================
 * RAFAELIA_BOOTBLOCK_v1 (constant)
 * ========================= */
#define RAF_BOOT_KERNEL   "ΣΔΩ"
#define RAF_BOOT_MODE     "RAFAELIA"
#define RAF_BOOT_ETHIC    "Amor"
#define RAF_HASH_CORE     "AETHER"
#define RAF_VECTOR_CORE   "RAF_VECTOR"
#define RAF_COGNITION     "TRINITY"
#define RAF_UNIVERSE      "RAFAELIA_CORE"
#define RAF_FIAT_PORTAL   "FIAT_PORTAL :: 龍空神 { ARKREΩ_CORE + STACK128K_HYPER + ALG_RAFAELIA_RING }"

/* bitraf64 literal (keep exact) */
#define RAF_BITRAF64 "AΔBΩΔTTΦIIBΩΔΣΣRΩRΔΔBΦΦFΔTTRRFΔBΩΣΣAFΦARΣFΦIΔRΦIFBRΦΩFIΦΩΩFΣFAΦΔ"

/* seals */
static const char *RAF_SEALS[10] = {"Σ","Ω","Δ","Φ","B","I","T","R","A","F"};

/* Retroalimentar = F_ok + F_gap + F_next */
typedef struct raf_retro_s {
  uint32_t ok;
  uint32_t gap;
  uint32_t next;
} raf_retro_t;

/* =========================
 * Minimal error model
 * ========================= */
typedef enum raf_status_e {
  RAF_OK = 0,
  RAF_ERR = 1
} raf_status_t;

#endif
H

cat << 'H' > "$ROOT/include/raf_arena.h"
#ifndef RAF_ARENA_H
#define RAF_ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct raf_arena_s {
  uint8_t *base;
  size_t   cap;
  size_t   used;
} raf_arena_t;

/* English comments block:
 * - Arena allocator avoids malloc/free overhead and GC.
 * - Deterministic allocation improves reproducibility and reduces fragmentation.
 */
int    raf_arena_init(raf_arena_t *a, size_t cap);
void   raf_arena_free(raf_arena_t *a);
void*  raf_arena_alloc(raf_arena_t *a, size_t sz, size_t align);
void   raf_arena_reset(raf_arena_t *a);

#endif
H

cat << 'C' > "$ROOT/src/raf_arena.c"
#include "../include/raf_arena.h"
#include <stdlib.h>
#include <string.h>

static size_t raf_align_up(size_t x, size_t a) {
  return (x + (a - 1u)) & ~(a - 1u);
}

int raf_arena_init(raf_arena_t *a, size_t cap) {
  if (!a || cap == 0) return 0;
  a->base = (uint8_t*)malloc(cap);
  if (!a->base) return 0;
  a->cap = cap;
  a->used = 0;
  return 1;
}

void raf_arena_free(raf_arena_t *a) {
  if (!a) return;
  if (a->base) free(a->base);
  a->base = NULL;
  a->cap = 0;
  a->used = 0;
}

void* raf_arena_alloc(raf_arena_t *a, size_t sz, size_t align) {
  if (!a || !a->base || sz == 0) return NULL;
  if (align == 0) align = 8;
  size_t off = raf_align_up(a->used, align);
  if (off + sz > a->cap) return NULL;
  void *p = (void*)(a->base + off);
  a->used = off + sz;
  memset(p, 0, sz);
  return p;
}

void raf_arena_reset(raf_arena_t *a) {
  if (!a || !a->base) return;
  a->used = 0;
}
C

cat << 'H' > "$ROOT/include/raf_io.h"
#ifndef RAF_IO_H
#define RAF_IO_H

#include <stddef.h>
#include <stdint.h>

int  raf_write_all(int fd, const void *buf, size_t n);
int  raf_puts(int fd, const char *s);
int  raf_putu64(int fd, uint64_t v);

#endif
H

cat << 'C' > "$ROOT/src/raf_io.c"
#include "../include/raf_io.h"
#include <unistd.h>

int raf_write_all(int fd, const void *buf, size_t n) {
  const uint8_t *p = (const uint8_t*)buf;
  size_t left = n;

  /* Avoid while/for by using a goto-driven loop */
  if (!buf && n) return 0;
  goto LOOP;

LOOP:
  if (left == 0) return 1;
  {
    ssize_t w = write(fd, p, left);
    if (w <= 0) return 0;
    p += (size_t)w;
    left -= (size_t)w;
  }
  goto LOOP;
}

int raf_puts(int fd, const char *s) {
  if (!s) return 0;
  size_t n = 0;
  const char *p = s;
  goto COUNT;
COUNT:
  if (*p == '\0') goto WRITE;
  ++p; ++n;
  goto COUNT;
WRITE:
  return raf_write_all(fd, s, n);
}

int raf_putu64(int fd, uint64_t v) {
  char tmp[32];
  char *p = &tmp[31];
  *p = '\0';
  if (v == 0) {
    *(--p) = '0';
    return raf_puts(fd, p);
  }
  goto DIGITS;
DIGITS:
  if (v == 0) return raf_puts(fd, p);
  {
    uint64_t q = v / 10;
    uint64_t r = v - q * 10;
    *(--p) = (char)('0' + (int)r);
    v = q;
  }
  goto DIGITS;
}
C

cat << 'H' > "$ROOT/include/raf_sha3.h"
#ifndef RAF_SHA3_H
#define RAF_SHA3_H

#include <stddef.h>
#include <stdint.h>

typedef struct raf_sha3_256_s {
  uint64_t st[25];
  uint8_t  buf[136]; /* rate = 1088 bits = 136 bytes */
  size_t   idx;
} raf_sha3_256_t;

/* English comments block:
 * - This is a minimal SHA3-256 (Keccak-f[1600]) implementation.
 * - No external dependencies, deterministic output, suitable for integrity manifests.
 */
void raf_sha3_256_init(raf_sha3_256_t *c);
void raf_sha3_256_update(raf_sha3_256_t *c, const void *data, size_t n);
void raf_sha3_256_final(raf_sha3_256_t *c, uint8_t out32[32]);

#endif
H

cat << 'C' > "$ROOT/src/raf_sha3.c"
#include "../include/raf_sha3.h"
#include <string.h>

#define ROTL64(x,n) (((x) << (n)) | ((x) >> (64 - (n))))

static const uint64_t RC[24] = {
  0x0000000000000001ULL, 0x0000000000008082ULL,
  0x800000000000808AULL, 0x8000000080008000ULL,
  0x000000000000808BULL, 0x0000000080000001ULL,
  0x8000000080008081ULL, 0x8000000000008009ULL,
  0x000000000000008AULL, 0x0000000000000088ULL,
  0x0000000080008009ULL, 0x000000008000000AULL,
  0x000000008000808BULL, 0x800000000000008BULL,
  0x8000000000008089ULL, 0x8000000000008003ULL,
  0x8000000000008002ULL, 0x8000000000000080ULL,
  0x000000000000800AULL, 0x800000008000000AULL,
  0x8000000080008081ULL, 0x8000000000008080ULL,
  0x0000000080000001ULL, 0x8000000080008008ULL
};

static const int R[25] = {
  0,  1, 62, 28, 27,
 36, 44,  6, 55, 20,
  3, 10, 43, 25, 39,
 41, 45, 15, 21,  8,
 18,  2, 61, 56, 14
};

static void keccak_f1600(uint64_t st[25]) {
  int round = 0;
  goto ROUND;
ROUND:
  if (round == 24) return;

  /* θ */
  {
    uint64_t C[5], D[5];
    C[0] = st[0]^st[5]^st[10]^st[15]^st[20];
    C[1] = st[1]^st[6]^st[11]^st[16]^st[21];
    C[2] = st[2]^st[7]^st[12]^st[17]^st[22];
    C[3] = st[3]^st[8]^st[13]^st[18]^st[23];
    C[4] = st[4]^st[9]^st[14]^st[19]^st[24];
    D[0] = C[4] ^ ROTL64(C[1],1);
    D[1] = C[0] ^ ROTL64(C[2],1);
    D[2] = C[1] ^ ROTL64(C[3],1);
    D[3] = C[2] ^ ROTL64(C[4],1);
    D[4] = C[3] ^ ROTL64(C[0],1);

    st[0]^=D[0]; st[5]^=D[0]; st[10]^=D[0]; st[15]^=D[0]; st[20]^=D[0];
    st[1]^=D[1]; st[6]^=D[1]; st[11]^=D[1]; st[16]^=D[1]; st[21]^=D[1];
    st[2]^=D[2]; st[7]^=D[2]; st[12]^=D[2]; st[17]^=D[2]; st[22]^=D[2];
    st[3]^=D[3]; st[8]^=D[3]; st[13]^=D[3]; st[18]^=D[3]; st[23]^=D[3];
    st[4]^=D[4]; st[9]^=D[4]; st[14]^=D[4]; st[19]^=D[4]; st[24]^=D[4];
  }

  /* ρ + π */
  {
    uint64_t b[25];
    /* Unrolled mapping keeps it simple and avoids switch/case. */
    b[ 0] = ROTL64(st[ 0], R[ 0]);
    b[10] = ROTL64(st[ 1], R[ 1]);
    b[20] = ROTL64(st[ 2], R[ 2]);
    b[ 5] = ROTL64(st[ 3], R[ 3]);
    b[15] = ROTL64(st[ 4], R[ 4]);

    b[16] = ROTL64(st[ 5], R[ 5]);
    b[ 1] = ROTL64(st[ 6], R[ 6]);
    b[11] = ROTL64(st[ 7], R[ 7]);
    b[21] = ROTL64(st[ 8], R[ 8]);
    b[ 6] = ROTL64(st[ 9], R[ 9]);

    b[ 7] = ROTL64(st[10], R[10]);
    b[17] = ROTL64(st[11], R[11]);
    b[ 2] = ROTL64(st[12], R[12]);
    b[12] = ROTL64(st[13], R[13]);
    b[22] = ROTL64(st[14], R[14]);

    b[23] = ROTL64(st[15], R[15]);
    b[ 8] = ROTL64(st[16], R[16]);
    b[18] = ROTL64(st[17], R[17]);
    b[ 3] = ROTL64(st[18], R[18]);
    b[13] = ROTL64(st[19], R[19]);

    b[14] = ROTL64(st[20], R[20]);
    b[24] = ROTL64(st[21], R[21]);
    b[ 9] = ROTL64(st[22], R[22]);
    b[19] = ROTL64(st[23], R[23]);
    b[ 4] = ROTL64(st[24], R[24]);

    memcpy(st, b, sizeof(b));
  }

  /* χ */
  {
    uint64_t t[25];
    memcpy(t, st, sizeof(t));
    int y = 0;
    goto CHI_Y;
CHI_Y:
    if (y == 5) goto IOTA;
    {
      int x = 0;
      goto CHI_X;
CHI_X:
      if (x == 5) { y++; goto CHI_Y; }
      int i = y*5 + x;
      st[i] = t[i] ^ ((~t[y*5 + ((x+1)%5)]) & t[y*5 + ((x+2)%5)]);
      x++;
      goto CHI_X;
    }
  }

IOTA:
  st[0] ^= RC[round];
  round++;
  goto ROUND;
}

void raf_sha3_256_init(raf_sha3_256_t *c) {
  memset(c, 0, sizeof(*c));
}

static void absorb_block(raf_sha3_256_t *c) {
  /* XOR buf into state lanes (little endian) */
  for (size_t i = 0; i < 17; i++) {
    uint64_t v = 0;
    v |= (uint64_t)c->buf[i*8 + 0] << 0;
    v |= (uint64_t)c->buf[i*8 + 1] << 8;
    v |= (uint64_t)c->buf[i*8 + 2] << 16;
    v |= (uint64_t)c->buf[i*8 + 3] << 24;
    v |= (uint64_t)c->buf[i*8 + 4] << 32;
    v |= (uint64_t)c->buf[i*8 + 5] << 40;
    v |= (uint64_t)c->buf[i*8 + 6] << 48;
    v |= (uint64_t)c->buf[i*8 + 7] << 56;
    c->st[i] ^= v;
  }
  keccak_f1600(c->st);
  memset(c->buf, 0, sizeof(c->buf));
  c->idx = 0;
}

void raf_sha3_256_update(raf_sha3_256_t *c, const void *data, size_t n) {
  const uint8_t *p = (const uint8_t*)data;
  if (!n) return;
  goto LOOP;
LOOP:
  if (n == 0) return;
  c->buf[c->idx++] = *p++;
  n--;
  if (c->idx == 136) absorb_block(c);
  goto LOOP;
}

void raf_sha3_256_final(raf_sha3_256_t *c, uint8_t out32[32]) {
  /* pad: 0x06 ... 0x80 */
  c->buf[c->idx++] = 0x06;
  while (c->idx < 136) c->buf[c->idx++] = 0x00;
  c->buf[135] |= 0x80;
  absorb_block(c);

  /* squeeze 32 bytes */
  for (size_t i = 0; i < 4; i++) {
    uint64_t v = c->st[i];
    out32[i*8 + 0] = (uint8_t)(v >> 0);
    out32[i*8 + 1] = (uint8_t)(v >> 8);
    out32[i*8 + 2] = (uint8_t)(v >> 16);
    out32[i*8 + 3] = (uint8_t)(v >> 24);
    out32[i*8 + 4] = (uint8_t)(v >> 32);
    out32[i*8 + 5] = (uint8_t)(v >> 40);
    out32[i*8 + 6] = (uint8_t)(v >> 48);
    out32[i*8 + 7] = (uint8_t)(v >> 56);
  }
}
C

cat << 'H' > "$ROOT/include/raf_manifest.h"
#ifndef RAF_MANIFEST_H
#define RAF_MANIFEST_H

#include <stdint.h>

typedef enum raf_ftype_e {
  RAF_F_UNKNOWN = 0,
  RAF_F_C, RAF_F_H, RAF_F_CPP, RAF_F_PY, RAF_F_SH, RAF_F_ASM,
  RAF_F_MD, RAF_F_JSON, RAF_F_YAML, RAF_F_SVG, RAF_F_TXT,
  RAF_F_ZIP, RAF_F_LOG, RAF_F_BIN
} raf_ftype_t;

raf_ftype_t raf_guess_type(const char *path);

#endif
H

cat << 'C' > "$ROOT/src/raf_manifest.c"
#include "../include/raf_manifest.h"
#include <string.h>

static int ends_with(const char *s, const char *sx) {
  size_t n = strlen(s), m = strlen(sx);
  if (m > n) return 0;
  return memcmp(s + (n - m), sx, m) == 0;
}

raf_ftype_t raf_guess_type(const char *p) {
  /* table-driven: compute by suffix, no switch/case */
  struct { const char *sx; raf_ftype_t t; } T[] = {
    {".c",RAF_F_C},{".h",RAF_F_H},{".cc",RAF_F_CPP},{".cpp",RAF_F_CPP},
    {".py",RAF_F_PY},{".sh",RAF_F_SH},{".bash",RAF_F_SH},{".zsh",RAF_F_SH},
    {".S",RAF_F_ASM},{".s",RAF_F_ASM},{".asm",RAF_F_ASM},
    {".md",RAF_F_MD},{".json",RAF_F_JSON},{".yml",RAF_F_YAML},{".yaml",RAF_F_YAML},
    {".svg",RAF_F_SVG},{".txt",RAF_F_TXT},{".log",RAF_F_LOG},
    {".zip",RAF_F_ZIP},{".apk",RAF_F_ZIP},{".jar",RAF_F_ZIP},{".aab",RAF_F_ZIP}
  };
  size_t i = 0;
  for (; i < sizeof(T)/sizeof(T[0]); i++) {
    if (ends_with(p, T[i].sx)) return T[i].t;
  }
  return RAF_F_UNKNOWN;
}
C

cat << 'H' > "$ROOT/include/raf_scan.h"
#ifndef RAF_SCAN_H
#define RAF_SCAN_H

#include <stddef.h>

int raf_scan_repo(const char *root, const char *out_jsonl_path);

#endif
H

cat << 'C' > "$ROOT/src/raf_scan.c"
#include "../include/raf_scan.h"
#include "../include/raf_sha3.h"
#include "../include/raf_io.h"
#include "../include/raf_manifest.h"

#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

/* English comments block:
 * - Scans filesystem deterministically and emits JSONL.
 * - Each line = one file record with size, type and sha3-256.
 * - Designed to be fed into CI, diff tools, and integrity pipelines.
 */

static void hex32(const uint8_t in[32], char out[65]) {
  static const char *H="0123456789abcdef";
  for (int i=0;i<32;i++){
    out[i*2+0]=H[(in[i]>>4)&15];
    out[i*2+1]=H[(in[i]>>0)&15];
  }
  out[64]='\0';
}

static int json_escape_write(int fd, const char *s) {
  /* minimal JSON string escaper: quotes, backslash, newline */
  if (!raf_puts(fd, "\"")) return 0;
  const unsigned char *p = (const unsigned char*)s;
  goto LOOP;
LOOP:
  if (*p == 0) goto END;
  unsigned char c = *p++;
  if (c == '\"') { if (!raf_puts(fd, "\\\"")) return 0; goto LOOP; }
  if (c == '\\') { if (!raf_puts(fd, "\\\\")) return 0; goto LOOP; }
  if (c == '\n') { if (!raf_puts(fd, "\\n")) return 0; goto LOOP; }
  if (c == '\r') { if (!raf_puts(fd, "\\r")) return 0; goto LOOP; }
  if (c == '\t') { if (!raf_puts(fd, "\\t")) return 0; goto LOOP; }
  char b[2]; b[0]=(char)c; b[1]=0;
  if (!raf_puts(fd,b)) return 0;
  goto LOOP;
END:
  return raf_puts(fd, "\"");
}

static int hash_file_sha3(const char *path, uint8_t out32[32], off_t *sz_out) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) return 0;

  struct stat st;
  if (fstat(fd, &st) != 0) { close(fd); return 0; }
  if (sz_out) *sz_out = st.st_size;

  raf_sha3_256_t ctx;
  raf_sha3_256_init(&ctx);

  uint8_t buf[8192];
  goto READ;
READ:
  {
    ssize_t r = read(fd, buf, sizeof(buf));
    if (r < 0) { close(fd); return 0; }
    if (r == 0) goto DONE;
    raf_sha3_256_update(&ctx, buf, (size_t)r);
  }
  goto READ;
DONE:
  raf_sha3_256_final(&ctx, out32);
  close(fd);
  return 1;
}

static int write_record(int outfd, const char *path, off_t sz, raf_ftype_t t, const char *sha_hex) {
  if (!raf_puts(outfd, "{")) return 0;

  if (!raf_puts(outfd, "\"path\":")) return 0;
  if (!json_escape_write(outfd, path)) return 0;

  if (!raf_puts(outfd, ",\"size\":")) return 0;
  if (!raf_putu64(outfd, (uint64_t)sz)) return 0;

  if (!raf_puts(outfd, ",\"type\":")) return 0;
  if (!raf_putu64(outfd, (uint64_t)t)) return 0;

  if (!raf_puts(outfd, ",\"sha3_256\":\"")) return 0;
  if (!raf_puts(outfd, sha_hex)) return 0;
  if (!raf_puts(outfd, "\"")) return 0;

  if (!raf_puts(outfd, "}\n")) return 0;
  return 1;
}

static int walk(const char *root, const char *rel, int outfd) {
  char full[4096];
  size_t rl = strlen(root), ll = strlen(rel);
  if (rl + 1 + ll + 1 >= sizeof(full)) return 0;

  strcpy(full, root);
  if (ll > 0) { strcat(full, "/"); strcat(full, rel); }

  struct stat st;
  if (lstat(full, &st) != 0) return 1; /* ignore missing */

  if (S_ISDIR(st.st_mode)) {
    DIR *d = opendir(full);
    if (!d) return 1;

    struct dirent *e;
    goto READDIR;
READDIR:
    e = readdir(d);
    if (!e) { closedir(d); return 1; }

    if (!strcmp(e->d_name,".") || !strcmp(e->d_name,"..")) goto READDIR;

    char nextrel[4096];
    if (ll == 0) snprintf(nextrel, sizeof(nextrel), "%s", e->d_name);
    else snprintf(nextrel, sizeof(nextrel), "%s/%s", rel, e->d_name);

    (void)walk(root, nextrel, outfd);
    goto READDIR;
  }

  if (S_ISREG(st.st_mode)) {
    uint8_t h[32];
    off_t sz = 0;
    if (hash_file_sha3(full, h, &sz)) {
      char hex[65]; hex32(h, hex);
      raf_ftype_t t = raf_guess_type(full);
      (void)write_record(outfd, full, sz, t, hex);
    }
  }

  return 1;
}

int raf_scan_repo(const char *root, const char *out_jsonl_path) {
  int fd = open(out_jsonl_path, O_CREAT|O_TRUNC|O_WRONLY, 0644);
  if (fd < 0) return 0;
  int ok = walk(root, "", fd);
  close(fd);
  return ok;
}
C

cat << 'H' > "$ROOT/include/raf_bbs.h"
#ifndef RAF_BBS_H
#define RAF_BBS_H

void raf_bbs_banner(void);
void raf_bbs_menu(void);

#endif
H

cat << 'C' > "$ROOT/src/raf_bbs.c"
#include "../include/raf_bbs.h"
#include "../include/raf_io.h"
#include <unistd.h>

void raf_bbs_banner(void) {
  raf_puts(STDOUT_FILENO,
    "\n"
    "╔══════════════════════════════════════════════════════════════╗\n"
    "║  RAFAELIA_OS  ::  ΣΔΩ  ::  RAFCODE-Φ  ::  Pre6seal            ║\n"
    "║  FIAT_PORTAL :: 龍 空 神                                      ║\n"
    "╚══════════════════════════════════════════════════════════════╝\n"
  );
}

void raf_bbs_menu(void) {
  raf_puts(STDOUT_FILENO,
    "\n"
    "[1] scan    -> scan repo and emit out/manifest.jsonl (sha3)\n"
    "[2] hash    -> sha3-256 of a file\n"
    "[3] rafc    -> compile expr to bytecode (toy compiler)\n"
    "[4] rafvm   -> run bytecode (toy vm)\n"
    "[5] help    -> CLI usage\n"
    "[0] exit\n\n"
  );
}
C

cat << 'H' > "$ROOT/include/raf_cli.h"
#ifndef RAF_CLI_H
#define RAF_CLI_H

int raf_cli_main(int argc, char **argv);

#endif
H

cat << 'C' > "$ROOT/src/raf_cli.c"
#include "../include/raf_cli.h"
#include "../include/raf_bbs.h"
#include "../include/raf_scan.h"
#include "../include/raf_sha3.h"
#include "../include/raf_io.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

static void hex32(const uint8_t in[32], char out[65]) {
  static const char *H="0123456789abcdef";
  for (int i=0;i<32;i++){ out[i*2]=H[(in[i]>>4)&15]; out[i*2+1]=H[in[i]&15]; }
  out[64]=0;
}

static int sha3_file(const char *path) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) return 0;
  raf_sha3_256_t ctx; raf_sha3_256_init(&ctx);

  uint8_t buf[8192];
  goto READ;
READ:
  {
    ssize_t r = read(fd, buf, sizeof(buf));
    if (r < 0) { close(fd); return 0; }
    if (r == 0) goto DONE;
    raf_sha3_256_update(&ctx, buf, (size_t)r);
  }
  goto READ;

DONE:
  {
    uint8_t out[32]; raf_sha3_256_final(&ctx, out);
    char hex[65]; hex32(out, hex);
    raf_puts(STDOUT_FILENO, hex);
    raf_puts(STDOUT_FILENO, "  ");
    raf_puts(STDOUT_FILENO, path);
    raf_puts(STDOUT_FILENO, "\n");
  }
  close(fd);
  return 1;
}

static void help(void) {
  raf_puts(STDOUT_FILENO,
    "\nUsage:\n"
    "  rafaelia_os menu\n"
    "  rafaelia_os scan [root] [out]\n"
    "  rafaelia_os hash <file>\n"
    "  rafaelia_os rafc <expr> <out.bc>\n"
    "  rafaelia_os rafvm <in.bc>\n\n"
  );
}

static int streq(const char *a, const char *b) { return a && b && strcmp(a,b)==0; }

int raf_cli_main(int argc, char **argv) {
  if (argc < 2) { raf_bbs_banner(); help(); return 0; }

  if (streq(argv[1], "menu")) {
    raf_bbs_banner();
    raf_bbs_menu();
    return 0;
  }

  if (streq(argv[1], "help")) { help(); return 0; }

  if (streq(argv[1], "scan")) {
    const char *root = (argc >= 3) ? argv[2] : ".";
    const char *out  = (argc >= 4) ? argv[3] : "out/manifest.jsonl";
    int ok = raf_scan_repo(root, out);
    raf_puts(STDOUT_FILENO, ok ? "OK\n" : "ERR\n");
    return ok ? 0 : 2;
  }

  if (streq(argv[1], "hash")) {
    if (argc < 3) { help(); return 2; }
    return sha3_file(argv[2]) ? 0 : 2;
  }

  /* Tools: rafc / rafvm are built in tools/ (executed externally). */
  if (streq(argv[1], "rafc") || streq(argv[1], "rafvm")) {
    raf_puts(STDOUT_FILENO, "Use ./tools/rafc or ./tools/rafvm binaries.\n");
    return 0;
  }

  help();
  return 2;
}
C

cat << 'C' > "$ROOT/src/main.c"
#include "../include/raf_cli.h"

int main(int argc, char **argv) {
  return raf_cli_main(argc, argv);
}
C

# -------------------------
# TOY COMPILER + VM (REAL)
# -------------------------
cat << 'C' > "$ROOT/tools/rafc.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* English comments block:
 * - rafc: tiny expression compiler.
 * - Supports + - * / and parentheses with integers.
 * - Emits bytecode to file for rafvm.
 */

typedef enum { OP_PUSH=1, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_END } op_t;

typedef struct { op_t op; long imm; } insn_t;

typedef struct { insn_t *v; size_t n, cap; } vec_t;

static void vpush(vec_t *v, insn_t i){
  if(v->n==v->cap){ v->cap=v->cap? v->cap*2:64; v->v=(insn_t*)realloc(v->v,v->cap*sizeof(insn_t)); }
  v->v[v->n++]=i;
}

typedef struct { const char *s; size_t i; } lex_t;

static void skip_ws(lex_t *L){ while(isspace((unsigned char)L->s[L->i])) L->i++; }

static int peek(lex_t *L){ skip_ws(L); return L->s[L->i]; }

static int eat(lex_t *L, int c){ if(peek(L)==c){ L->i++; return 1; } return 0; }

static long parse_int(lex_t *L){
  skip_ws(L);
  long sign=1;
  if(L->s[L->i]=='-'){ sign=-1; L->i++; }
  long v=0;
  while(isdigit((unsigned char)L->s[L->i])){ v=v*10+(L->s[L->i]-'0'); L->i++; }
  return sign*v;
}

static void expr(lex_t *L, vec_t *out);

static void factor(lex_t *L, vec_t *out){
  if(eat(L,'(')){ expr(L,out); if(!eat(L,')')){ fprintf(stderr,"missing )\n"); exit(2);} return; }
  if(isdigit((unsigned char)peek(L)) || peek(L)=='-'){
    long v=parse_int(L);
    vpush(out,(insn_t){OP_PUSH,v});
    return;
  }
  fprintf(stderr,"bad factor near: %c\n", peek(L));
  exit(2);
}

static void term(lex_t *L, vec_t *out){
  factor(L,out);
  for(;;){
    int c=peek(L);
    if(c=='*'){ L->i++; factor(L,out); vpush(out,(insn_t){OP_MUL,0}); continue; }
    if(c=='/'){ L->i++; factor(L,out); vpush(out,(insn_t){OP_DIV,0}); continue; }
    break;
  }
}

static void expr(lex_t *L, vec_t *out){
  term(L,out);
  for(;;){
    int c=peek(L);
    if(c=='+'){ L->i++; term(L,out); vpush(out,(insn_t){OP_ADD,0}); continue; }
    if(c=='-'){ L->i++; term(L,out); vpush(out,(insn_t){OP_SUB,0}); continue; }
    break;
  }
}

int main(int argc, char **argv){
  if(argc<3){ fprintf(stderr,"Usage: rafc <expr> <out.bc>\n"); return 2; }
  const char *src=argv[1];
  const char *outf=argv[2];
  lex_t L={src,0};
  vec_t v={0};

  expr(&L,&v);
  vpush(&v,(insn_t){OP_END,0});

  FILE *f=fopen(outf,"wb");
  if(!f){ perror("fopen"); return 2; }
  fwrite(&v.n,sizeof(v.n),1,f);
  fwrite(v.v,sizeof(insn_t),v.n,f);
  fclose(f);
  free(v.v);
  return 0;
}
C

cat << 'C' > "$ROOT/tools/rafvm.c"
#include <stdio.h>
#include <stdlib.h>

/* English comments block:
 * - rafvm: executes bytecode produced by rafc.
 * - Stack machine: deterministic, no GC, no external libs.
 */

typedef enum { OP_PUSH=1, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_END } op_t;
typedef struct { op_t op; long imm; } insn_t;

int main(int argc, char **argv){
  if(argc<2){ fprintf(stderr,"Usage: rafvm <in.bc>\n"); return 2; }
  FILE *f=fopen(argv[1],"rb");
  if(!f){ perror("fopen"); return 2; }
  size_t n=0;
  if(fread(&n,sizeof(n),1,f)!=1){ fprintf(stderr,"bad file\n"); return 2; }
  insn_t *p=(insn_t*)malloc(n*sizeof(insn_t));
  if(!p) return 2;
  if(fread(p,sizeof(insn_t),n,f)!=n){ fprintf(stderr,"bad file\n"); return 2; }
  fclose(f);

  long st[1024]; int sp=0;

  for(size_t i=0;i<n;i++){
    insn_t ins=p[i];
    switch(ins.op){
      case OP_PUSH: st[sp++]=ins.imm; break;
      case OP_ADD:  st[sp-2]=st[sp-2]+st[sp-1]; sp--; break;
      case OP_SUB:  st[sp-2]=st[sp-2]-st[sp-1]; sp--; break;
      case OP_MUL:  st[sp-2]=st[sp-2]*st[sp-1]; sp--; break;
      case OP_DIV:  st[sp-2]=st[sp-2]/st[sp-1]; sp--; break;
      case OP_END:  printf("%ld\n", st[sp-1]); free(p); return 0;
      default: fprintf(stderr,"bad op\n"); free(p); return 2;
    }
  }
  free(p);
  return 0;
}
C

# -------------------------
# BUILD
# -------------------------
cat << 'B' > "$ROOT/build.sh"
#!/usr/bin/env sh
set -eu
CC="${CC:-cc}"
CFLAGS="-O3 -std=c11 -Wall -Wextra -Wshadow -Wconversion -Wno-unused-parameter"
mkdir -p out

$CC $CFLAGS -Iinclude \
  src/main.c src/raf_cli.c src/raf_bbs.c src/raf_scan.c src/raf_sha3.c src/raf_io.c src/raf_manifest.c src/raf_arena.c \
  -o out/rafaelia_os

$CC $CFLAGS tools/rafc.c -o tools/rafc
$CC $CFLAGS tools/rafvm.c -o tools/rafvm

echo "Built:"
echo "  out/rafaelia_os"
echo "  tools/rafc"
echo "  tools/rafvm"
B
chmod +x "$ROOT/build.sh"

chmod +x "$ROOT/tools/rafc" 2>/dev/null || true
chmod +x "$ROOT/tools/rafvm" 2>/dev/null || true

echo "OK: generated $ROOT"
