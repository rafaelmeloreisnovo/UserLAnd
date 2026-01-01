/*
  RAFAELIA_CLI v0.1 (C puro) — BBS-like menu + tests + reference(golden) + benchmark
  Build (Termux/Linux):
    clang -O3 -std=c11 rafaelia_cli.c -o rafaelia -lm
    ./rafaelia
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ANSI colors (BBS vibes)
#define C_RST  "\x1b[0m"
#define C_DIM  "\x1b[2m"
#define C_BLD  "\x1b[1m"
#define C_GRN  "\x1b[32m"
#define C_CYN  "\x1b[36m"
#define C_YLW  "\x1b[33m"
#define C_RED  "\x1b[31m"
#define C_WHT  "\x1b[37m"

// Core types
typedef struct { float x,y,z; } vec3;

typedef struct {
  uint64_t id;
  uint64_t h;
  vec3 v;          // unit vector
  char   text[96]; // small payload
} rec_t;

typedef struct {
  rec_t *a;
  size_t n, cap;
} vecdb_t;

// Timing
static inline uint64_t nsec_now(void){
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec*1000000000ull + (uint64_t)ts.tv_nsec;
}

// Hash djb2 64-bit
static inline uint64_t djb2_64(const char *s){
  uint64_t h = 5381ull;
  unsigned char c;
  while((c = (unsigned char)*s++)){
    h = ((h << 5) + h) + (uint64_t)c; // h*33 + c
  }
  return h;
}

// hash -> vec3 in [-1,1], normalize
static inline float u16_to_f01(uint16_t u){ return (float)u / 65535.0f; }

static inline vec3 hash_to_vec3(uint64_t h){
  uint16_t a = (uint16_t)( h        & 0xFFFFu);
  uint16_t b = (uint16_t)((h >> 16) & 0xFFFFu);
  uint16_t c = (uint16_t)((h >> 32) & 0xFFFFu);

  float x = u16_to_f01(a)*2.0f - 1.0f;
  float y = u16_to_f01(b)*2.0f - 1.0f;
  float z = u16_to_f01(c)*2.0f - 1.0f;

  vec3 v = (vec3){x,y,z};
  float n = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
  if(n < 1e-12f) n = 1.0f;
  v.x /= n; v.y /= n; v.z /= n;
  return v;
}

static inline float dot3(vec3 a, vec3 b){
  return a.x*b.x + a.y*b.y + a.z*b.z;
}

static inline float clampf(float x, float lo, float hi){
  return (x<lo)?lo:((x>hi)?hi:x);
}

static inline float ang_rad(vec3 a, vec3 b){
  float d = clampf(dot3(a,b), -1.0f, 1.0f);
  return acosf(d);
}

// VecDB
static void vecdb_init(vecdb_t *db){
  db->a = NULL; db->n = 0; db->cap = 0;
}

static void vecdb_free(vecdb_t *db){
  free(db->a);
  db->a = NULL; db->n = db->cap = 0;
}

static void vecdb_reserve(vecdb_t *db, size_t cap){
  if(cap <= db->cap) return;
  size_t newcap = db->cap ? db->cap : 16;
  while(newcap < cap) newcap <<= 1;
  rec_t *p = (rec_t*)realloc(db->a, newcap * sizeof(rec_t));
  if(!p){ fprintf(stderr, "OOM\n"); exit(1); }
  db->a = p; db->cap = newcap;
}

static uint64_t g_next_id = 1;

static void vecdb_add(vecdb_t *db, const char *text){
  vecdb_reserve(db, db->n + 1);
  rec_t *r = &db->a[db->n++];
  r->id = g_next_id++;
  strncpy(r->text, text, sizeof(r->text)-1);
  r->text[sizeof(r->text)-1] = 0;
  r->h = djb2_64(r->text);
  r->v = hash_to_vec3(r->h);
}

// top-k query (linear scan)
typedef struct { size_t idx; float score; } hit_t;

static void hits_sort_desc(hit_t *h, size_t n){
  for(size_t i=1;i<n;i++){
    hit_t key = h[i];
    size_t j=i;
    while(j>0 && h[j-1].score < key.score){
      h[j]=h[j-1]; j--;
    }
    h[j]=key;
  }
}

static size_t vecdb_query_topk(vecdb_t *db, const char *q, size_t k, hit_t *out){
  if(db->n==0) return 0;
  uint64_t hq = djb2_64(q);
  vec3 vq = hash_to_vec3(hq);

  size_t m = 0;
  for(size_t i=0;i<db->n;i++){
    float s = dot3(vq, db->a[i].v);
    if(m < k){
      out[m++] = (hit_t){ i, s };
      hits_sort_desc(out, m);
    } else if(s > out[m-1].score){
      out[m-1] = (hit_t){ i, s };
      hits_sort_desc(out, m);
    }
  }
  return m;
}

// UI / BBS
static void banner(void){
  printf(C_CYN C_BLD
"╔══════════════════════════════════════════════════════════════╗\n"
"║   RAFAELIA_CLI  v0.1   ΣΩΔΦBITRAF  |  FAÇA (LER→RETRO→...)   ║\n"
"╠══════════════════════════════════════════════════════════════╣\n"
"║  ♥φ  Ethica[8]  fΩ=963↔999  Spiral√3/2  Trinity633  OWLψ     ║\n"
"╚══════════════════════════════════════════════════════════════╝\n"
  C_RST);
}

static void menu(void){
  printf(C_WHT
"\n"
"[1] Seed demo records\n"
"[2] Add record\n"
"[3] Query top-k\n"
"[4] Run tests (determinism + geometry)\n"
"[5] Benchmark (ingest + query)\n"
"[6] Dump DB\n"
"[7] Reference tests (golden vectors)\n"
"[0] Exit\n"
  C_RST);
  printf(C_DIM "Select> " C_RST);
}

static void read_line(char *buf, size_t n){
  if(!fgets(buf, (int)n, stdin)){ buf[0]=0; return; }
  size_t L = strlen(buf);
  while(L && (buf[L-1]=='\n' || buf[L-1]=='\r')){ buf[--L]=0; }
}

// Tests
static int test_determinism(void){
  const char *s = "RAFAELIA";
  uint64_t h1 = djb2_64(s), h2 = djb2_64(s);
  if(h1 != h2) return 0;
  vec3 v1 = hash_to_vec3(h1);
  vec3 v2 = hash_to_vec3(h2);
  float d = fabsf(v1.x-v2.x)+fabsf(v1.y-v2.y)+fabsf(v1.z-v2.z);
  return d < 1e-9f;
}

static int test_unit_norm(void){
  const char *s = "unit_norm";
  vec3 v = hash_to_vec3(djb2_64(s));
  float n = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
  return fabsf(n - 1.0f) < 1e-4f;
}

static int test_angle_bound(void){
  const char *s = "angle_bound";
  vec3 v = hash_to_vec3(djb2_64(s));
  float dot = dot3(v,v);
  float tau = 0.85f;
  float theta = ang_rad(v,v);
  float bound = acosf(tau);
  return (dot >= tau) && (theta <= bound + 1e-6f);
}

static void run_tests(void){
  int ok1 = test_determinism();
  int ok2 = test_unit_norm();
  int ok3 = test_angle_bound();

  printf("\n" C_BLD "TEST RESULTS" C_RST "\n");
  printf(" - determinism(hash+vec): %s\n", ok1? (C_GRN "OK" C_RST):(C_RED "FAIL" C_RST));
  printf(" - unit_norm(|v|≈1):      %s\n", ok2? (C_GRN "OK" C_RST):(C_RED "FAIL" C_RST));
  printf(" - angle_bound(dot→θ):    %s\n", ok3? (C_GRN "OK" C_RST):(C_RED "FAIL" C_RST));
  printf(" => %s\n", (ok1&&ok2&&ok3)? (C_GRN "ALL PASSED" C_RST):(C_RED "SOME FAILED" C_RST));
}

// Reference (Golden) Tests
static int test_ref_hash(const char *s, uint64_t expected){
  uint64_t got = djb2_64(s);
  if(got != expected){
    printf("   REF_HASH FAIL: \"%s\"\n", s);
    printf("     got = %016llx\n", (unsigned long long)got);
    printf("     exp = %016llx\n", (unsigned long long)expected);
    return 0;
  }
  printf(" - ref_hash OK: \"%s\" = %016llx\n", s, (unsigned long long)got);
  return 1;
}

static void run_ref_tests(void){
  int ok = 1;
  printf("\n" C_BLD "REFERENCE (GOLDEN) TESTS" C_RST "\n");

  // Golden hashes (djb2-64 baseline)
  ok &= test_ref_hash("RAFAELIA",                     0x001AE6355C5B5EFAull);
  ok &= test_ref_hash("amor e ciencia",               0xE390D0E9A7BB3DA5ull);
  ok &= test_ref_hash("toroide delta pi phi",         0x7199BDA008F7501Full);
  ok &= test_ref_hash("Restauratio Gaia: Ethica[8]",  0x37A4862137DCE6A2ull);

  // Geometry self-check
  {
    vec3 v = hash_to_vec3(djb2_64("RAFAELIA"));
    float d = dot3(v,v);
    float th = ang_rad(v,v);
    int g1 = (fabsf(d - 1.0f) < 1e-4f);
    int g2 = (fabsf(th - 0.0f) < 1e-6f);
    printf(" - geometry self-dot≈1: %s\n", g1? (C_GRN "OK" C_RST):(C_RED "FAIL" C_RST));
    printf(" - geometry self-ang≈0: %s\n", g2? (C_GRN "OK" C_RST):(C_RED "FAIL" C_RST));
    ok &= g1 & g2;
  }

  printf(" => %s\n", ok? (C_GRN "ALL REF PASSED" C_RST):(C_RED "REF FAILED" C_RST));
}

// Benchmark
static uint64_t splitmix64(uint64_t *x){
  uint64_t z = (*x += 0x9e3779b97f4a7c15ull);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  return z ^ (z >> 31);
}

static void bench(vecdb_t *db){
  char buf[128], q[128];
  size_t N = 20000, Q = 5000, K = 5;

  printf(C_DIM "\nN records default=20000 (enter to keep): " C_RST);
  read_line(buf, sizeof(buf));
  if(buf[0]) N = (size_t)strtoull(buf, NULL, 10);

  printf(C_DIM "Q queries  default=5000  (enter to keep): " C_RST);
  read_line(buf, sizeof(buf));
  if(buf[0]) Q = (size_t)strtoull(buf, NULL, 10);

  uint64_t seed = 0xC0FFEE1234ull;

  uint64_t t0 = nsec_now();
  vecdb_free(db); vecdb_init(db);
  vecdb_reserve(db, N);

  for(size_t i=0;i<N;i++){
    uint64_t r = splitmix64(&seed);
    snprintf(buf, sizeof(buf), "rec_%zu_%016llx", i, (unsigned long long)r);
    vecdb_add(db, buf);
  }
  uint64_t t1 = nsec_now();

  hit_t hits[16];
  uint64_t t2 = nsec_now();
  for(size_t i=0;i<Q;i++){
    uint64_t r = splitmix64(&seed);
    snprintf(q, sizeof(q), "q_%zu_%016llx", i, (unsigned long long)r);
    (void)vecdb_query_topk(db, q, K, hits);
  }
  uint64_t t3 = nsec_now();

  double ingest_ms = (double)(t1 - t0)/1e6;
  double query_ms  = (double)(t3 - t2)/1e6;

  double ingest_rps = (N>0) ? ((double)N / (ingest_ms/1000.0)) : 0.0;
  double qps        = (Q>0) ? ((double)Q / (query_ms/1000.0))  : 0.0;

  printf("\n" C_BLD "BENCH RESULTS" C_RST "\n");
  printf(" - ingest:  N=%zu  time=%.2f ms  rate=%.2f rec/s\n", N, ingest_ms, ingest_rps);
  printf(" - query:   Q=%zu  time=%.2f ms  rate=%.2f q/s   (topK=%zu)\n", Q, query_ms, qps, K);
  printf(" - note: linear scan O(N) baseline\n");
}

// Actions
static void seed_demo(vecdb_t *db){
  const char *demo[] = {
    "amor e ciencia",
    "toroide delta pi phi",
    "spiral raiz 3 sobre 2",
    "trinity 6 3 3",
    "owl psi heuristica",
    "zipraf bitraf integridade",
    "causalidade e verificacao",
    "entropia e coerencia",
    "memoria vetorial minima",
    "ler retroalimentar expandir validar executar etica"
  };
  for(size_t i=0;i<sizeof(demo)/sizeof(demo[0]);i++) vecdb_add(db, demo[i]);
  printf(C_GRN "\nSeeded %zu demo records.\n" C_RST, sizeof(demo)/sizeof(demo[0]));
}

static void add_record(vecdb_t *db){
  char buf[128];
  printf(C_DIM "\nText> " C_RST);
  read_line(buf, sizeof(buf));
  if(!buf[0]){ printf(C_YLW "empty.\n" C_RST); return; }
  vecdb_add(db, buf);
  rec_t *r = &db->a[db->n-1];
  printf(C_GRN "added id=%llu  h=%016llx  v=(%.4f %.4f %.4f)\n" C_RST,
         (unsigned long long)r->id,
         (unsigned long long)r->h,
         r->v.x, r->v.y, r->v.z);
}

static void query_topk(vecdb_t *db){
  if(db->n==0){ printf(C_YLW "\nDB empty. seed or add.\n" C_RST); return; }
  char q[128], buf[64];
  size_t K = 5;

  printf(C_DIM "\nQuery> " C_RST);
  read_line(q, sizeof(q));
  if(!q[0]){ printf(C_YLW "empty.\n" C_RST); return; }

  printf(C_DIM "TopK (default 5)> " C_RST);
  read_line(buf, sizeof(buf));
  if(buf[0]) K = (size_t)strtoull(buf, NULL, 10);
  if(K<1) K=1; if(K>15) K=15;

  hit_t hits[16];
  size_t m = vecdb_query_topk(db, q, K, hits);

  uint64_t hq = djb2_64(q);
  vec3 vq = hash_to_vec3(hq);
  printf("\n" C_BLD "QUERY" C_RST "  h=%016llx  v=(%.3f %.3f %.3f)\n",
         (unsigned long long)hq, vq.x, vq.y, vq.z);

  printf(C_BLD "RESULTS" C_RST "\n");
  for(size_t i=0;i<m;i++){
    rec_t *r = &db->a[hits[i].idx];
    float theta = ang_rad(vq, r->v);
    printf(" #%zu  score=%.4f  θ=%.2f°  id=%llu  \"%s\"\n",
           i+1,
           hits[i].score,
           (float)(theta * (180.0/M_PI)),
           (unsigned long long)r->id,
           r->text);
  }
}

static void dump_db(vecdb_t *db){
  printf("\n" C_BLD "DB DUMP" C_RST "  n=%zu\n", db->n);
  for(size_t i=0;i<db->n;i++){
    rec_t *r = &db->a[i];
    printf(" [%zu] id=%llu h=%016llx v=(%.3f %.3f %.3f) \"%s\"\n",
      i,
      (unsigned long long)r->id,
      (unsigned long long)r->h,
      r->v.x, r->v.y, r->v.z,
      r->text
    );
  }
}

int main(void){
  vecdb_t db; vecdb_init(&db);
  banner();

  for(;;){
    menu();
    char sel[16];
    read_line(sel, sizeof(sel));
    int c = sel[0];

    switch(c){
      case '1': seed_demo(&db); break;
      case '2': add_record(&db); break;
      case '3': query_topk(&db); break;
      case '4': run_tests(); break;
      case '5': bench(&db); break;
      case '6': dump_db(&db); break;
      case '7': run_ref_tests(); break;
      case '0':
        vecdb_free(&db);
        printf(C_DIM "\nbye.\n" C_RST);
        return 0;
      default:
        printf(C_YLW "\n?\n" C_RST);
        break;
    }
  }
}
