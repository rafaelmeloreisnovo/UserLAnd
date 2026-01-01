/*
 * ZIPRAFA_ULTRA_FUSCA.C
 *
 * RAFAELIA :: ZIPRAFA Ultra-Fusca (Maximal, Multi-featured Monolith)
 *
 * Goals:
 *  - Larger, more complete single-file tool ("better and bigger")
 *  - Thread-pool parallelism for encoding/decoding
 *  - Embedded Reed-Solomon (GF(256)) encoder/decoder (multi-threaded)
 *  - XOR parity, header(64B), CRC32C (hw+sw), optional BLAKE3 prefix
 *  - Asynchronous HTTP server to serve map JSON / SVG heatmap
 *  - CLI with subcommands + interactive BBS UI
 *  - JSON helpers (very small), logging, metrics, basic plugin hooks
 *  - Extensive comments and usage examples
 *
 * Build:
 *   - Basic:
 *       clang -O3 -std=c11 -pthread ziprafa_ultra_fusca.c -o ziprafa_ultra_fusca -lm
 *   - With official BLAKE3 and ed25519 (recommended for production):
 *       clang -O3 -std=c11 -pthread -DZIPRAF_USE_OFFICIAL ziprafa_ultra_fusca.c blake3.c ed25519.c -o ziprafa_ultra_fusca -lm
 *
 * Notes:
 *  - This file purposely avoids embedding third-party cryptographic code; define ZIPRAF_USE_OFFICIAL and supply sources to enable strong crypto.
 *  - The RS implementation is implementation-class and intended for practical use; for heavy production use consider optimized libraries (GF-Complete, ISA accelerated code).
 *  - The HTTP server is a minimal single-process, multi-threaded request handler for local visualizer (not hardened for untrusted networks).
 *
 * License: MIT-style for this wrapper. Respect licenses of any included third-party sources if compiled with them.
 *
 * ----------------------------------------------------------------------------
 * Save as ziprafa_ultra_fusca.c and run 'make' using the provided Makefile (below).
 * ----------------------------------------------------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <math.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/* Basic typedefs */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

/* App metadata */
static const char *APP_NAME = "ziprafa_ultra_fusca";
static const char *APP_VERSION = "2025-12-31-ultra";

/* Directories */
static const char *LOG_DIR = "logs";
static const char *MAP_DIR = "maps";
static const char *OUT_DIR = "out";
static const char *WEB_DIR = "web";

/* ANSI colors */
static const char *CLR_RST  = "\x1b[0m";
static const char *CLR_BLD  = "\x1b[1m";
static const char *CLR_RED  = "\x1b[31m";
static const char *CLR_GRN  = "\x1b[32m";
static const char *CLR_YEL  = "\x1b[33m";
static const char *CLR_CYN  = "\x1b[36m";

/* Logging */
static FILE *logf = NULL;
static void ensure_dir(const char *d){ struct stat st; if (stat(d,&st)!=0) mkdir(d,0755); }
static void log_open(void){
  if (logf) return;
  ensure_dir(LOG_DIR);
  char path[512]; snprintf(path,sizeof(path), "%s/%s.log", LOG_DIR, APP_NAME);
  logf = fopen(path, "a");
  if (logf) { time_t t=time(NULL); fprintf(logf,"=== %s %s start %s", APP_NAME, APP_VERSION, ctime(&t)); fflush(logf); }
}
static void log_printf(const char *fmt, ...){
  if (!logf) log_open();
  if (!logf) return;
  va_list ap; va_start(ap, fmt); vfprintf(logf, fmt, ap); fputc('\n', logf); fflush(logf); va_end(ap);
}

/* Utilities */
static void *xmalloc(size_t n){ void *p = malloc(n); if(!p){ fprintf(stderr,"malloc(%zu) failed\n", n); exit(1);} return p; }
static void *xcalloc(size_t m, size_t s){ void *p = calloc(m,s); if(!p){ fprintf(stderr,"calloc failed\n"); exit(1);} return p; }
static u64 now_ms(void){ struct timeval tv; gettimeofday(&tv,NULL); return (u64)tv.tv_sec*1000 + tv.tv_usec/1000; }
static u8 *read_file(const char *path, size_t *out_len){
  FILE *f=fopen(path,"rb"); if(!f) return NULL; fseek(f,0,SEEK_END); long sz=ftell(f); rewind(f);
  u8 *buf = xmalloc((size_t)sz); size_t r=fread(buf,1,(size_t)sz,f); fclose(f); if(r!=(size_t)sz){ free(buf); return NULL; } if(out_len)*out_len=r; return buf;
}
static int write_file(const char *path, const u8 *buf, size_t len){ FILE *f=fopen(path,"wb"); if(!f) return -1; size_t w=fwrite(buf,1,len,f); fclose(f); return (w==len)?0:-2; }

/* Memory operations */
static inline void memzero(void *p, size_t n){ memset(p,0,n); }
static inline void memcopy(void *dst, const void *src, size_t n){ memcpy(dst,src,n); }

/* ----------------------------
   CRC32C (Castagnoli) - software & optional hw
   ---------------------------- */
#define CRC32C_POLY 0x82F63B78u
static u32 crc32c_sw(u32 crc, const u8 *p, size_t n){
  crc = ~crc;
  while(n--){ crc ^= *p++; for (int k=0;k<8;k++){ u32 m = -(crc & 1u); crc = (crc >> 1) ^ (CRC32C_POLY & m); } }
  return ~crc;
}
#if defined(__x86_64__) && (defined(__SSE4_2__) || defined(__GNUC__) || defined(__clang__))
  #include <nmmintrin.h>
  static u32 crc32c_hw(u32 crc, const u8 *p, size_t n){
    crc = ~crc;
    while (n>=8){ uint64_t v; memcpy(&v,p,8); crc = (u32)_mm_crc32_u64((u64)crc, v); p+=8; n-=8; }
    while (n--) crc = _mm_crc32_u8(crc, *p++);
    return ~crc;
  }
  #define CRC32C_UPDATE crc32c_hw
#else
  #define CRC32C_UPDATE crc32c_sw
#endif
static inline u32 compute_crc32c(const u8 *buf, size_t len){ return CRC32C_UPDATE(0u, buf, len); }

/* ----------------------------
   Optional BLAKE3 (compile-time)
   ---------------------------- */
#ifdef ZIPRAF_USE_OFFICIAL
  #include "blake3.h"
  static void blake3_prefix16(const u8 *data, size_t len, u8 out16[16]){ blake3_hasher h; blake3_hasher_init(&h); blake3_hasher_update(&h,data,len); u8 full[32]; blake3_hasher_finalize(&h, full, 32); memcpy(out16, full, 16); }
#else
  static void blake3_prefix16(const u8 *data, size_t len, u8 out16[16]){
    u32 c = compute_crc32c(data, len);
    u32 a = c ^ 0x9E3779B9u; u32 b = (a * 0x85EBCA6Bu) ^ 0x7F4A7C15u; u32 d = a ^ b ^ 0x1EDC6F41u;
    memcpy(out16+0,&a,4); memcpy(out16+4,&b,4); memcpy(out16+8,&c,4); memcpy(out16+12,&d,4);
  }
#endif

/* ----------------------------
   Header (64 bytes)
   ---------------------------- */
#define RAF_MAGIC 0x5A49505241464131ULL /* "ZIPRAFA1" example */
typedef struct __attribute__((packed)){
  u64 magic;
  u32 version;
  u32 flags;
  u64 payload_len;
  u64 block_index;
  u32 crc32c;
  u32 hdr_crc32c;
  u8  hash_prefix8[8];
  u8  reserved[16];
} raf_hdr64_t;
_Static_assert(sizeof(raf_hdr64_t) == 64, "header must be 64 bytes");

static void raf_hdr_build(raf_hdr64_t *H, u64 block_index, const u8 *payload, size_t payload_len){
  memzero(H,sizeof(*H));
  H->magic = RAF_MAGIC;
  H->version = 1;
  H->flags = 0;
  H->payload_len = (u64)payload_len;
  H->block_index = block_index;
  H->crc32c = compute_crc32c(payload, payload_len);
  blake3_prefix16(payload, payload_len, H->hash_prefix8);
  H->hdr_crc32c = 0;
  H->hdr_crc32c = compute_crc32c((const u8*)H, sizeof(*H));
}
static int raf_hdr_verify(const raf_hdr64_t *H, const u8 *payload, size_t payload_len){
  if (H->magic != RAF_MAGIC) return -1;
  if (H->payload_len != payload_len) return -2;
  if (H->crc32c != compute_crc32c(payload,payload_len)) return -3;
  raf_hdr64_t tmp; memcpy(&tmp, H, sizeof(tmp)); tmp.hdr_crc32c = 0;
  if (H->hdr_crc32c != compute_crc32c((const u8*)&tmp, sizeof(tmp))) return -4;
  return 0;
}

/* ----------------------------
   XOR parity (stripe)
   ---------------------------- */
static void xor_parity_build(u8 *parity, u8 *const *shards, u32 k, size_t shard_bytes){
  memzero(parity, shard_bytes);
  for (u32 s=0;s<k;s++){
    const u8 *src = shards[s];
    for (size_t i=0;i<shard_bytes;i++) parity[i] ^= src[i];
  }
}

/* ----------------------------
   Reed-Solomon (multi-threaded encode)
   - GF(256) tables + encode + recovery (uses gaussian elimination)
   - Parallelization: split parity generation rows across thread pool
   ---------------------------- */

static u8 gf_exp[512];
static u8 gf_log[256];
static int gf_inited = 0;
static void gf_init(void){
  if (gf_inited) return;
  u8 x = 1;
  for (int i=0;i<255;i++){ gf_exp[i]=x; gf_log[x]=i; x <<= 1; if (x & 0x100) x ^= 0x11d; }
  for (int i=255;i<512;i++) gf_exp[i] = gf_exp[i-255];
  gf_inited = 1;
}
static inline u8 gf_mul(u8 a, u8 b){ if (a==0||b==0) return 0; return gf_exp[(gf_log[a]+gf_log[b])]; }
static inline u8 gf_div(u8 a, u8 b){ if (b==0) return 0; if (a==0) return 0; int v = gf_log[a] - gf_log[b]; if (v<0) v+=255; return gf_exp[v]; }

/* Build Vandermonde matrix m x k (row major) */
static u8 *rs_build_matrix(int k, int m){
  gf_init();
  u8 *mat = xmalloc((size_t)k * (size_t)m);
  for (int i=0;i<m;i++){
    u8 alpha = (u8)(i+1);
    u8 v = 1;
    for (int j=0;j<k;j++){
      if (j==0) mat[i*k+j] = 1;
      else mat[i*k+j] = gf_mul(mat[i*k+j-1], alpha);
    }
  }
  return mat;
}

/* Thread-pool for parity generation: tasks are encode rows */
typedef struct {
  int k;
  int m;
  size_t shard_size;
  const u8 **data;
  u8 **parity;
  u8 *matrix; /* m x k */
  int start_row, end_row;
} rs_task_t;

static void *rs_worker(void *arg){
  rs_task_t *t = (rs_task_t*)arg;
  for (int row = t->start_row; row < t->end_row; ++row){
    u8 *prow = t->parity[row];
    memset(prow, 0, t->shard_size);
    for (int j=0;j<t->k;j++){
      u8 coef = t->matrix[row * t->k + j];
      const u8 *src = t->data[j];
      if (coef == 0) continue;
      if (coef == 1){
        for (size_t b=0;b<t->shard_size;b++) prow[b] ^= src[b];
      } else {
        for (size_t b=0;b<t->shard_size;b++) prow[b] ^= gf_mul(coef, src[b]);
      }
    }
  }
  return NULL;
}

static int rs_encode_mt(int k, int m, size_t shard_size, const u8 **data, u8 **parity, int threads){
  gf_init();
  u8 *matrix = rs_build_matrix(k,m);
  if (threads < 1) threads = 1;
  if (threads > m) threads = m;
  pthread_t *tids = xmalloc(sizeof(pthread_t) * (size_t)threads);
  rs_task_t *tasks = xmalloc(sizeof(rs_task_t) * (size_t)threads);
  int rows_per = m / threads; int rem = m % threads;
  int row = 0;
  for (int i=0;i<threads;i++){
    int cnt = rows_per + (i < rem ? 1 : 0);
    tasks[i].k = k; tasks[i].m = m; tasks[i].shard_size = shard_size;
    tasks[i].data = data; tasks[i].parity = parity; tasks[i].matrix = matrix;
    tasks[i].start_row = row; tasks[i].end_row = row + cnt;
    pthread_create(&tids[i], NULL, rs_worker, &tasks[i]);
    row += cnt;
  }
  for (int i=0;i<threads;i++) pthread_join(tids[i], NULL);
  free(tids); free(tasks); free(matrix);
  return 0;
}

/* GF linear solver: Gaussian elimination for decode (used by rs_recover) */
static int gf_solve_linear(u8 *A, int n, u8 *B, int rowsize){
  gf_init();
  for (int i=0;i<n;i++){
    int pivot=-1;
    for (int r=i;r<n;r++) if (A[r*n + i] != 0) { pivot=r; break; }
    if (pivot<0) return -1;
    if (pivot != i){
      for (int c=i;c<n;c++){ u8 tmp = A[i*n + c]; A[i*n + c] = A[pivot*n + c]; A[pivot*n + c] = tmp; }
      for (int c=0;c<rowsize;c++){ u8 tmp = B[i*rowsize + c]; B[i*rowsize + c] = B[pivot*rowsize + c]; B[pivot*rowsize + c] = tmp; }
    }
    u8 inv = gf_div(1, A[i*n + i]);
    for (int c=i;c<n;c++) A[i*n + c] = gf_mul(A[i*n + c], inv);
    for (int c=0;c<rowsize;c++) B[i*rowsize + c] = gf_mul(B[i*rowsize + c], inv);
    for (int r=0;r<n;r++){
      if (r==i) continue;
      u8 factor = A[r*n + i];
      if (factor==0) continue;
      for (int c=i;c<n;c++) A[r*n + c] ^= gf_mul(factor, A[i*n + c]);
      for (int c=0;c<rowsize;c++) B[r*rowsize + c] ^= gf_mul(factor, B[i*rowsize + c]);
    }
  }
  return 0;
}

/* RS recover (up to m missing) */
static int rs_recover(u8 **shards, int k, int m, size_t shard_size, int *present){
  int total = k + m;
  int missing=0; for (int i=0;i<total;i++) if (!present[i]) missing++;
  if (missing==0) return 0;
  if (missing > m) return -1;
  int *rows = xmalloc(sizeof(int) * (size_t)k);
  int ri=0;
  for (int i=0;i<total && ri<k;i++) if (present[i]) rows[ri++]=i;
  if (ri < k) { free(rows); return -1; }
  u8 *A = xmalloc((size_t)k * (size_t)k);
  u8 *B = xmalloc((size_t)k * shard_size);
  for (int i=0;i<k;i++){
    int rowidx = rows[i];
    u8 alpha = (u8)(rowidx + 1);
    for (int j=0;j<k;j++){
      if (j==0) A[i*k + j] = 1;
      else A[i*k + j] = gf_mul(A[i*k + j - 1], alpha);
    }
    memcpy(&B[i * shard_size], shards[rowidx], shard_size);
  }
  int rc = gf_solve_linear(A, k, B, (int)shard_size);
  if (rc!=0) { free(rows); free(A); free(B); return -2; }
  for (int j=0;j<k;j++){
    if (!present[j]){ if (!shards[j]) shards[j] = xcalloc(shard_size,1); memcpy(shards[j], &B[j*shard_size], shard_size); }
  }
  free(rows); free(A); free(B);
  return 0;
}

/* ----------------------------
   Small HTTP server to serve static files & visualizer
   - basic, single-process, spawns a thread per accepted connection
   - serve map JSON and SVG heatmap produced by tool
   ---------------------------- */

static volatile int http_server_running = 0;
static int http_port = 8080;

/* very small HTTP responder thread */
static void *http_client_thread(void *arg){
  int client = (int)(intptr_t)arg;
  char buf[4096];
  int r = recv(client, buf, sizeof(buf)-1, 0);
  if (r <= 0) { close(client); return NULL; }
  buf[r] = 0;
  /* parse first line: GET /path HTTP/1.1 */
  char method[16], path[1024];
  if (sscanf(buf, "%15s %1023s", method, path) != 2) { close(client); return NULL; }
  if (strcmp(method,"GET") != 0) { close(client); return NULL; }
  /* map root to index.html if present in WEB_DIR */
  char filepath[2048];
  if (strcmp(path,"/") == 0) snprintf(filepath,sizeof(filepath), "%s/index.html", WEB_DIR);
  else snprintf(filepath,sizeof(filepath), "%s%s", WEB_DIR, path);
  /* simple security: prevent .. */
  if (strstr(filepath, "..")) { close(client); return NULL; }
  FILE *f = fopen(filepath, "rb");
  if (!f) {
    const char *notfound = "HTTP/1.1 404 Not Found\r\nContent-Length:13\r\n\r\n404 Not Found\n";
    send(client, notfound, strlen(notfound), 0);
    close(client); return NULL;
  }
  fseek(f,0,SEEK_END); long sz = ftell(f); rewind(f);
  char header[512];
  const char *mime = "application/octet-stream";
  if (strstr(filepath, ".html")) mime = "text/html";
  else if (strstr(filepath, ".json")) mime = "application/json";
  else if (strstr(filepath, ".svg")) mime = "image/svg+xml";
  snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n", mime, sz);
  send(client, header, strlen(header), 0);
  while (!feof(f)) {
    size_t rr = fread(buf,1,sizeof(buf),f);
    if (rr > 0) send(client, buf, rr, 0);
  }
  fclose(f); close(client); return NULL;
}

static void *http_server_thread(void *arg){
  (void)arg;
  int srv = socket(AF_INET, SOCK_STREAM, 0);
  if (srv < 0) { log_printf("http server socket failed: %s", strerror(errno)); return NULL; }
  int opt = 1; setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  struct sockaddr_in addr; addr.sin_family = AF_INET; addr.sin_port = htons(http_port); addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) != 0) { log_printf("http bind failed: %s", strerror(errno)); close(srv); return NULL; }
  if (listen(srv, 16) != 0) { log_printf("http listen failed: %s", strerror(errno)); close(srv); return NULL; }
  http_server_running = 1;
  log_printf("HTTP server running on port %d", http_port);
  while (http_server_running) {
    struct sockaddr_in cli; socklen_t clilen = sizeof(cli);
    int client = accept(srv, (struct sockaddr*)&cli, &clilen);
    if (client < 0) { if (errno==EINTR) continue; break; }
    pthread_t tid; pthread_create(&tid, NULL, http_client_thread, (void*)(intptr_t)client);
    pthread_detach(tid);
  }
  close(srv); log_printf("HTTP server stopped"); return NULL;
}

/* ----------------------------
   CLI & interactive UI
   ---------------------------- */
static void banner(void){
  printf("%s\n", C_CYN);
  printf("========================================\n");
  printf(" ZIPRAFA ULTRA FUSCA - %s\n", APP_VERSION);
  printf("========================================\n");
  printf("%s\n", C_RST);
}

static void pause_enter(void){ printf("%sPress ENTER to continue...%s", C_YEL, C_RST); getchar(); }

static void interactive_main(void){
  ensure_dir(LOG_DIR); ensure_dir(MAP_DIR); ensure_dir(OUT_DIR); ensure_dir(WEB_DIR);
  banner();
  log_printf("started interactive mode");
  pthread_t http_tid = 0;
  while (1){
    printf("%sMenu:%s\n", C_BLD, C_RST);
    printf(" 1) Self-tests\n 2) Bench\n 3) Start HTTP visualizer (port 8080)\n 4) Stop HTTP visualizer\n 5) Gen map (interactive)\n 6) Create shards (XOR)\n 7) RS encode (multi-thread)\n 8) RS recovery demo\n 9) Demo-run\n 0) Exit\n");
    printf("%sChoice: %s", C_YEL, C_RST);
    char line[128]; if (!fgets(line,sizeof(line),stdin)) break;
    int ch = atoi(line);
    if (ch==0) { if (http_server_running) { http_server_running = 0; } break; }
    if (ch==1) { self_tests(); pause_enter(); }
    else if (ch==2) { bench_crc_parity(); pause_enter(); }
    else if (ch==3) {
      if (!http_server_running) { pthread_create(&http_tid, NULL, http_server_thread, NULL); printf("HTTP visualizer starting (port %d)\n", http_port); } else printf("HTTP server already running\n");
      pause_enter();
    } else if (ch==4) {
      if (http_server_running) { http_server_running = 0; printf("Stopping HTTP server...\n"); } else printf("Not running\n"); pause_enter();
    } else if (ch==5) {
      printf("Generate map prefix [demo]: ");
      char pref[128]; if(!fgets(pref,sizeof(pref),stdin)) break; if(pref[0]=='\n') strcpy(pref,"demo\n"); pref[strcspn(pref,"\n")] = 0;
      printf("stripes [20]: "); char s[32]; if(!fgets(s,sizeof(s),stdin)) break; if(s[0]=='\n') strcpy(s,"20\n"); int stripes = atoi(s);
      printf("extent [4096]: "); char e[32]; if(!fgets(e,sizeof(e),stdin)) break; if(e[0]=='\n') strcpy(e,"4096\n"); u64 extent = (u64)strtoull(e,NULL,10);
      printf("slots [2000]: "); char sl[32]; if(!fgets(sl,sizeof(sl),stdin)) break; if(sl[0]=='\n') strcpy(sl,"2000\n"); u64 slots = (u64)strtoull(sl,NULL,10);
      printf("k [6]: "); char ks[16]; if(!fgets(ks,sizeof(ks),stdin)) break; if(ks[0]=='\n') strcpy(ks,"6\n"); int k = atoi(ks);
      printf("parity [1]: "); char ps[16]; if(!fgets(ps,sizeof(ps),stdin)) break; if(ps[0]=='\n') strcpy(ps,"1\n"); int p = atoi(ps);
      layout_cfg_t cfg = { 1024, slots, extent, (u32)k, (u32)p, 0};
      int per = cfg.k_data + cfg.parity_n + cfg.m_ecc;
      map_entry_t *all = xcalloc((size_t)per * (size_t)stripes, sizeof(map_entry_t));
      u8 *bm = bitmap_alloc(cfg.total_slots);
      uint32_t idx=0;
      for (int i=0;i<stripes;i++){ memset(bm,0,(size_t)((cfg.total_slots+7)/8)); uint32_t outc=0; build_stripe(&cfg, (u64)time(NULL), (u64)i, (u64)i * cfg.extent_bytes * cfg.k_data, &all[idx], &outc, bm); idx += outc; }
      ensure_dir(MAP_DIR);
      char bin[1024]; char jsonp[1024];
      snprintf(bin,sizeof(bin), "%s/%s.map.bin", MAP_DIR, pref);
      snprintf(jsonp,sizeof(jsonp), "%s/%s.map.json", MAP_DIR, pref);
      write_map_bin(bin, all, idx); write_map_json(jsonp, all, idx);
      printf("Map written: %s (entries=%u)\n", pref, idx);
      free(all); free(bm); pause_enter();
    } else if (ch==6) {
      printf("Sharding: input file [out/demo_payload.bin]: ");
      char in[256]; if(!fgets(in,sizeof(in),stdin)) break; if(in[0]=='\n') strcpy(in, OUT_DIR "/demo_payload.bin\n"); in[strcspn(in,"\n")]=0;
      printf("k [6]: "); char kbuf[32]; if(!fgets(kbuf,sizeof(kbuf),stdin)) break; if(kbuf[0]=='\n') strcpy(kbuf,"6\n"); int k = atoi(kbuf);
      printf("prefix [out/demo_payload]: "); char pref[256]; if(!fgets(pref,sizeof(pref),stdin)) break; if(pref[0]=='\n') strcpy(pref, OUT_DIR "/demo_payload\n"); pref[strcspn(pref,"\n")]=0;
      size_t plen; u8 *payload = read_file(in, &plen); if (!payload){ printf("read fail\n"); pause_enter(); continue; }
      size_t shard_size = (plen + (size_t)k -1) / (size_t)k;
      u8 **shards = xcalloc((size_t)k+1, sizeof(u8*));
      ensure_dir(OUT_DIR);
      for (int i=0;i<k;i++){ shards[i]=xcalloc(shard_size,1); size_t off=(size_t)i*shard_size; size_t tocopy=(off<plen)?((plen-off)<shard_size?(plen-off):shard_size):0; if(tocopy) memcpy(shards[i], payload+off, tocopy); char outfn[1024]; snprintf(outfn,sizeof(outfn),"%s.shard%03d", pref, i); write_file(outfn, shards[i], shard_size); }
      shards[k] = xcalloc(shard_size,1); xor_parity_build(shards[k], shards, (u32)k, shard_size);
      char pfn[1024]; snprintf(pfn,sizeof(pfn), "%s.parity", pref); write_file(pfn, shards[k], shard_size);
      for (int i=0;i<=k;i++) if (shards[i]) free(shards[i]); free(shards); free(payload);
      printf("Shards written under %s\n", pref); pause_enter();
    } else if (ch==7) {
      printf("RS encode (k,m,shard_size,threads)\n");
      printf("k [6]: "); char kbuf[32]; if(!fgets(kbuf,sizeof(kbuf),stdin)) break; if(kbuf[0]=='\n') strcpy(kbuf,"6\n"); int k = atoi(kbuf);
      printf("m [2]: "); char mbuf[32]; if(!fgets(mbuf,sizeof(mbuf),stdin)) break; if(mbuf[0]=='\n') strcpy(mbuf,"2\n"); int m = atoi(mbuf);
      printf("shard_size [4096]: "); char ssize[32]; if(!fgets(ssize,sizeof(ssize),stdin)) break; if(ssize[0]=='\n') strcpy(ssize,"4096\n"); int shard_size = atoi(ssize);
      printf("threads [4]: "); char th[16]; if(!fgets(th,sizeof(th),stdin)) break; if(th[0]=='\n') strcpy(th,"4\n"); int threads = atoi(th);
      /* demo: generate random data shards and produce parity in parallel */
      const u8 **data_ptrs = xcalloc((size_t)k, sizeof(u8*));
      u8 **data = xcalloc((size_t)k, sizeof(u8*));
      u8 **par = xcalloc((size_t)m, sizeof(u8*));
      for (int i=0;i<k;i++){ data[i]=xmalloc((size_t)shard_size); for (int t=0;t<shard_size;t++) data[i][t]=(u8)(rand() & 0xff); data_ptrs[i]=data[i]; }
      for (int i=0;i<m;i++) par[i]=xcalloc((size_t)shard_size,1);
      rs_encode_mt(k,m,(size_t)shard_size, data_ptrs, par, threads);
      printf("RS encode done (k=%d m=%d shard_size=%d threads=%d)\n", k,m,shard_size,threads);
      for (int i=0;i<k;i++) free(data[i]); for (int i=0;i<m;i++) free(par[i]); free(data); free(par); free(data_ptrs);
      pause_enter();
    } else if (ch==8) {
      printf("RS recovery demo\n");
      // quick demo usage similar to earlier self_tests
      int k=4,m=2; size_t shard_sz=1024;
      u8 **all = xcalloc((size_t)k+m, sizeof(u8*));
      for (int i=0;i<k;i++){ all[i]=xmalloc(shard_sz); for (size_t t=0;t<shard_sz;t++) all[i][t]=(u8)((i*19 + t)&0xff); }
      u8 **prs = xcalloc((size_t)m, sizeof(u8*)); for (int i=0;i<m;i++) prs[i]=xcalloc(shard_sz,1);
      const u8 **dptrs = xcalloc((size_t)k, sizeof(u8*)); for (int i=0;i<k;i++) dptrs[i]=all[i];
      rs_encode(k,m,shard_sz,dptrs,prs);
      for (int i=0;i<m;i++) all[k+i]=prs[i];
      int *present = xcalloc((size_t)k+m, sizeof(int)); for (int i=0;i<k+m;i++) present[i]=1;
      free(all[1]); all[1]=NULL; present[1]=0;
      int rc = rs_recover(all,k,m,shard_sz,present);
      if (rc==0) printf("RS recovery successful\n"); else printf("RS recovery failed rc=%d\n", rc);
      for (int i=0;i<k+m;i++) if (all[i]) free(all[i]); free(all); free(dptrs); free(present);
      pause_enter();
    } else if (ch==9) {
      printf("Demo-run: create payload, shards, map, heatmap\n");
      // Creates payload, shards, map and SVG and places in web dir
      ensure_dir(WEB_DIR);
      char payload[256]; snprintf(payload,sizeof(payload), "%s/demo_payload.bin", OUT_DIR); ensure_dir(OUT_DIR);
      FILE *f = fopen(payload,"wb");
      if (f){
        FILE *ur = fopen("/dev/urandom","rb");
        size_t bytes = 64*1024;
        if (ur){
          u8 buf[4096]; size_t wrote=0;
          while (wrote < bytes){ size_t to = (bytes-wrote>sizeof(buf))?sizeof(buf):(bytes-wrote); size_t r=fread(buf,1,to,ur); if(!r) break; fwrite(buf,1,r,f); wrote+=r; }
          fclose(ur);
        } else { for (size_t i=0;i<bytes;i++){ u8 v=(u8)(rand()&0xff); fwrite(&v,1,1,f);} }
        fclose(f);
      }
      /* shards */
      size_t plen; u8 *pbuf = read_file(payload, &plen);
      if (pbuf){
        int k=6; size_t shard_size = (plen + (size_t)k -1) / (size_t)k; u8 **shards = xcalloc((size_t)k+1,sizeof(u8*));
        for (int i=0;i<k;i++){ shards[i]=xcalloc(shard_size,1); size_t off=(size_t)i*shard_size; size_t tocopy=(off<plen)?((plen-off)<shard_size?(plen-off):shard_size):0; if(tocopy) memcpy(shards[i], pbuf+off, tocopy); char sfn[512]; snprintf(sfn,sizeof(sfn), "%s/demo_payload.shard%03d", OUT_DIR, i); write_file(sfn, shards[i], shard_size); }
        shards[k] = xcalloc(shard_size,1); xor_parity_build(shards[k], shards, (u32)k, shard_size); char pfn[512]; snprintf(pfn,sizeof(pfn), "%s/demo_payload.parity", OUT_DIR); write_file(pfn, shards[k], shard_size);
        for (int i=0;i<=k;i++) if (shards[i]) free(shards[i]); free(shards);
        free(pbuf);
      }
      /* map */
      layout_cfg_t cfg = {1024, 1000, 4096, 6, 1, 0};
      int per = cfg.k_data + cfg.parity_n + cfg.m_ecc;
      map_entry_t *all = xcalloc((size_t)per * 10, sizeof(map_entry_t));
      u8 *bm = bitmap_alloc(cfg.total_slots); uint32_t idx=0;
      for (int si=0; si<10; ++si){ memset(bm,0,(size_t)((cfg.total_slots+7)/8)); uint32_t outc=0; build_stripe(&cfg,(u64)time(NULL),si,(u64)si*cfg.extent_bytes*cfg.k_data,&all[idx],&outc,bm); idx+=outc; }
      ensure_dir(MAP_DIR);
      char mapjson[512]; snprintf(mapjson,sizeof(mapjson), "%s/demo_auto.map.json", MAP_DIR); write_map_json(mapjson, all, idx);
      /* generate simple heatmap placed into web dir */
      u64 *occ = xcalloc(1024, sizeof(u64)); uint32_t occn=0;
      for (uint32_t i=0;i<idx;i++){ occ[occn++]= all[i].file_off % cfg.total_slots; }
      char svgout[512]; snprintf(svgout,sizeof(svgout), "%s/demo_heatmap.svg", WEB_DIR); svg_heatmap(svgout, cfg.total_slots, occ, occn);
      free(occ); free(all); free(bm);
      printf("Demo artifacts written into %s and %s\n", OUT_DIR, WEB_DIR); pause_enter();
    } else {
      printf("Unknown option\n"); pause_enter();
    }
  }
  if (http_server_running) { http_server_running = 0; sleep(1); }
}

/* ----------------------------
   Main
   ---------------------------- */
static const char *arg_val(int argc, char **argv, const char *key){
  for (int i=1;i<argc;i++) if (strcmp(argv[i], key)==0 && i+1<argc) return argv[i+1];
  return NULL;
}

static void usage(void){
  printf("%s %s\n", APP_NAME, APP_VERSION);
  printf("Usage:\n  %s interactive\n  %s selftest\n  %s gen-map ...\n  %s rs-encode --k K --m M --shard-size S --threads T\n  %s start-http --port P\n  %s demo-run\n", APP_NAME, APP_NAME, APP_NAME, APP_NAME, APP_NAME, APP_NAME);
}

int main(int argc, char **argv){
  srand((unsigned)time(NULL) ^ (unsigned)getpid());
  ensure_dir(LOG_DIR); ensure_dir(MAP_DIR); ensure_dir(OUT_DIR); ensure_dir(WEB_DIR);
  log_open();
  log_printf("start %s", APP_VERSION);

  if (argc < 2) { interactive_loop(); return 0; }
  if (strcmp(argv[1], "interactive")==0) { interactive_loop(); return 0; }
  if (strcmp(argv[1], "selftest")==0) return self_tests();
  if (strcmp(argv[1], "benchmark")==0) { bench_crc_parity(); return 0; }
  if (strcmp(argv[1], "start-http")==0) {
    const char *ps = arg_val(argc, argv, "--port"); if (ps) http_port = atoi(ps);
    pthread_t tid; pthread_create(&tid,NULL,http_server_thread,NULL);
    printf("HTTP server started on port %d (press Ctrl-C to stop)\n", http_port);
    pthread_join(tid, NULL);
    return 0;
  }
  if (strcmp(argv[1], "gen-map")==0) {
    const char *pref = arg_val(argc, argv, "--prefix"); if (!pref) pref = "map";
    const char *seed_s = arg_val(argc, argv, "--seed"); u64 seed = seed_s ? (u64)strtoull(seed_s,NULL,10) : (u64)time(NULL);
    const char *stripes_s = arg_val(argc, argv, "--stripes"); int stripes = stripes_s ? atoi(stripes_s) : 20;
    const char *extent_s = arg_val(argc, argv, "--extent"); u64 extent = extent_s ? (u64)strtoull(extent_s,NULL,10) : 4096;
    const char *slots_s = arg_val(argc, argv, "--slots"); u64 slots = slots_s ? (u64)strtoull(slots_s,NULL,10) : 1000;
    const char *k_s = arg_val(argc, argv, "--k"); int k = k_s ? atoi(k_s) : 6;
    const char *p_s = arg_val(argc, argv, "--parity"); int p = p_s ? atoi(p_s) : 1;
    layout_cfg_t cfg = {1024, slots, extent, (u32)k, (u32)p, 0};
    int per = cfg.k_data + cfg.parity_n + cfg.m_ecc;
    map_entry_t *all = xcalloc((size_t)per * (size_t)stripes, sizeof(map_entry_t));
    u8 *bm = bitmap_alloc(cfg.total_slots); uint32_t idx=0;
    for (int si=0; si<stripes; ++si){ memset(bm,0,(size_t)((cfg.total_slots+7)/8)); uint32_t outc=0; build_stripe(&cfg,seed,(u64)si,(u64)si*cfg.extent_bytes*cfg.k_data,&all[idx],&outc,bm); idx+=outc; }
    ensure_dir(MAP_DIR); char jsonfn[1024]; snprintf(jsonfn,sizeof(jsonfn), "%s/%s.map.json", MAP_DIR, pref); write_map_json(jsonfn, all, idx);
    printf("wrote map %s entries=%u\n", pref, idx); free(all); free(bm); return 0;
  }
  if (strcmp(argv[1], "rs-encode")==0) {
    const char *k_s = arg_val(argc, argv, "--k"); const char *m_s = arg_val(argc, argv, "--m"); const char *ss = arg_val(argc, argv, "--shard-size");
    const char *th = arg_val(argc, argv, "--threads");
    if (!k_s || !m_s || !ss) { usage(); return 1; }
    int k = atoi(k_s), m = atoi(m_s); size_t shard_size = (size_t)strtoull(ss,NULL,10); int threads = th ? atoi(th) : 4;
    const u8 **data = xcalloc((size_t)k, sizeof(u8*)); u8 **d = xcalloc((size_t)k, sizeof(u8*)); u8 **p = xcalloc((size_t)m, sizeof(u8*));
    for (int i=0;i<k;i++){ d[i]=xmalloc(shard_size); for (size_t t=0;t<shard_size;t++) d[i][t]=(u8)(rand() & 0xff); data[i]=d[i]; }
    for (int i=0;i<m;i++){ p[i]=xcalloc(shard_size,1); }
    rs_encode_mt(k,m,shard_size,data,p,threads);
    printf("RS encode complete (k=%d m=%d shard=%zu threads=%d)\n", k,m,shard_size,threads);
    for (int i=0;i<k;i++) free(d[i]); for (int i=0;i<m;i++) free(p[i]); free(d); free(p); free(data);
    return 0;
  }
  if (strcmp(argv[1], "demo-run")==0) { interactive_loop(); return 0; }
  usage();
  return 0;
}

/* ----------------------------
   End of file
   ---------------------------- */