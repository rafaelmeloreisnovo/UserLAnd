#!/data/data/com.termux/files/usr/bin/sh
set -eu

ROOT="${ROOT:-CatOS_RAFAELIA}"
CC="${CC:-clang}"
CFLAGS="${CFLAGS:--O3 -DNDEBUG}"
LDFLAGS="${LDFLAGS:-}"

mkdir -p "$ROOT/bin" "$ROOT/src" "$ROOT/tools" "$ROOT/docs" "$ROOT/out" "$ROOT/ativo" "$ROOT/barra" "$ROOT/patches"

# ────────────────────────────────────────────────────────────────
# tools: MID replace (auditável por faixa)
# ────────────────────────────────────────────────────────────────
cat > "$ROOT/tools/mid_replace.sh" <<'SH'
#!/usr/bin/env sh
# mid_replace.sh — replace content between MID markers (in-place)
# Usage: mid_replace.sh FILE MIDNAME NEWFILE
# Markers (line-exact):
#   /*<MID:NAME>*/
#   /*</MID:NAME>*/
set -eu
F="$1"; MID="$2"; NEW="$3"
TMP="${F}.tmp.$$"

awk -v mid="$MID" -v newfile="$NEW" '
BEGIN{
  start="/*<MID:" mid ">*/";
  end  ="/*</MID:" mid ">*/";
  in=0; seen=0;
}
{
  if($0==start){
    print;
    while((getline l < newfile) > 0) print l;
    close(newfile);
    in=1; seen=1;
    next;
  }
  if(in==1){
    if($0==end){ print; in=0; }
    next;
  }
  print;
}
END{
  if(seen==0) exit 2;
  if(in==1) exit 3;
}
' "$F" > "$TMP" || { rc=$?; rm -f "$TMP"; exit $rc; }

mv "$TMP" "$F"
SH
chmod +x "$ROOT/tools/mid_replace.sh"

# ────────────────────────────────────────────────────────────────
# tools: json_find (sem jq) — grep com contexto e offsets
# ────────────────────────────────────────────────────────────────
cat > "$ROOT/tools/json_find.sh" <<'SH'
#!/usr/bin/env sh
# json_find.sh — find patterns inside large JSON exports (no jq)
# Usage:
#   json_find.sh FILE "pattern"
# Tips:
#   - Use a unique token/phrase from your prompt
#   - Combine with head/tail/sed -n 'N,Mp'
set -eu
F="$1"
P="$2"
# show line numbers + 2 lines of context
grep -n -C 2 -- "$P" "$F" || true
SH
chmod +x "$ROOT/tools/json_find.sh"

# ────────────────────────────────────────────────────────────────
# docs: MID map
# ────────────────────────────────────────────────────────────────
cat > "$ROOT/docs/MID_MAP.md" <<'MD'
# CatOS MID Map (faixas auditáveis)

**Regra:** toda edição grande vira *replace* entre âncoras.

Formato (linha-exata):
- `/*<MID:NOME>*/`
- `/*</MID:NOME>*/`

Ferramenta:
- `tools/mid_replace.sh src/raf_bench_bbs.c NOME novo_trecho.txt`

Exemplo:
1) `cat > novo.txt <<'EOF' ... EOF`
2) `./tools/mid_replace.sh src/raf_bench_bbs.c LOADAVG novo.txt`
MD

# ────────────────────────────────────────────────────────────────
# src: raf_bench_bbs.c (compatível; sem getloadavg; best-effort affinity)
# ────────────────────────────────────────────────────────────────
cat > "$ROOT/src/raf_bench_bbs.c" <<'C'
/* raf_bench_bbs.c — RAFAELIA Low-Level Bench (BBS/CLI) 🧮⚡🦉
   - single-file C (no deps)
   - Linux/Android/Termux friendly
   - avoids getloadavg(); reads /proc/loadavg
   - affinity/nice best-effort; never breaks compile
   Build (Termux): clang -O3 -DNDEBUG raf_bench_bbs.c -o raf_bench
*/

#define _POSIX_C_SOURCE 200809L
#if defined(__linux__) || defined(__ANDROID__)
  #ifndef _GNU_SOURCE
    #define _GNU_SOURCE
  #endif
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#if defined(__linux__) || defined(__ANDROID__)
  #include <unistd.h>
  #include <sys/resource.h>
  #include <sys/time.h>
  #include <sched.h>
#endif

/* =========================== BBS UI =========================== */
static int g_clear = 1;
static int g_color = 1;

static void term_clear(void){
  if(!g_clear) return;
  fputs("\033[2J\033[H", stdout);
}

static void bbs_banner(void){
  const char *c1 = g_color ? "\033[1;36m" : "";
  const char *c2 = g_color ? "\033[1;33m" : "";
  const char *c0 = g_color ? "\033[0m" : "";
  printf("%s╔══════════════════════════════════════════════════════════════╗%s\n", c1, c0);
  printf("%s║  RAFAELIA :: raf_bench_bbs  (C puro / auditável / no-deps)   ║%s\n", c2, c0);
  printf("%s║  ψ→χ→ρ→Δ→Σ→Ω   |  Bitraf64 / CRC / BW / Latência / CPU        ║%s\n", c2, c0);
  printf("%s╚══════════════════════════════════════════════════════════════╝%s\n", c1, c0);
}

/* =========================== time =========================== */
static inline uint64_t ns_now(void){
  struct timespec ts;
#if defined(CLOCK_MONOTONIC_RAW)
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
  clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
  return (uint64_t)ts.tv_sec*1000000000ull + (uint64_t)ts.tv_nsec;
}
static inline double ns_to_s(uint64_t ns){ return (double)ns/1e9; }

static inline uint64_t ns_proc_cpu(void){
  struct timespec ts;
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
  return (uint64_t)ts.tv_sec*1000000000ull + (uint64_t)ts.tv_nsec;
}

/* =========================== RNG =========================== */
static inline uint64_t xs64(uint64_t *s){
  uint64_t x=*s;
  x ^= x<<13; x ^= x>>7; x ^= x<<17;
  *s=x; return x;
}

/* =========================== DJB2 64 =========================== */
static inline uint64_t djb2_64(const uint8_t *p, size_t n){
  uint64_t h=5381u;
  for(size_t i=0;i<n;i++) h = ((h<<5)+h) + (uint64_t)p[i];
  return h;
}

/* =========================== CRC32 =========================== */
static uint32_t crc32_t[256];
static void crc32_init(void){
  for(uint32_t i=0;i<256;i++){
    uint32_t c=i;
    for(int k=0;k<8;k++){
      c = (c & 1) ? (0xEDB88320u ^ (c>>1)) : (c>>1);
    }
    crc32_t[i]=c;
  }
}
static inline uint32_t crc32_ieee(const uint8_t *p, size_t n){
  uint32_t c=0xFFFFFFFFu;
  for(size_t i=0;i<n;i++) c = crc32_t[(c ^ p[i]) & 0xFFu] ^ (c>>8);
  return c ^ 0xFFFFFFFFu;
}

/* =========================== alloc =========================== */
static void* xmalloc_aligned(size_t align, size_t sz){
  void *ptr=NULL;
#if defined(_POSIX_VERSION)
  if(posix_memalign(&ptr, align, sz)!=0) ptr=NULL;
#else
  (void)align; ptr=malloc(sz);
#endif
  if(!ptr){
    fprintf(stderr,"alloc failed: %zu bytes\n", sz);
    exit(1);
  }
  return ptr;
}

/* =========================== sys read =========================== */
static int read_file_snip(const char *path, char *out, size_t out_sz, size_t maxb){
  FILE *f=fopen(path,"rb");
  if(!f) return 0;
  size_t n=fread(out,1,(maxb<out_sz?maxb:out_sz-1),f);
  fclose(f);
  out[n]=0;
  return (int)n;
}
static int read_first_line(const char *path, char *out, size_t out_sz){
  FILE *f=fopen(path,"rb");
  if(!f) return 0;
  if(!fgets(out,(int)out_sz,f)){ fclose(f); return 0; }
  fclose(f);
  size_t n=strlen(out);
  while(n && (out[n-1]=='\n' || out[n-1]=='\r')) out[--n]=0;
  return 1;
}

/* =========================== system context =========================== */
typedef struct {
  char cpu_model[256];
  char mem_total[64];
  char mem_avail[64];
  char freq_khz[64];
  double load1, load5, load15;
  int nicev;
  int aff_one; /* -1 unknown; >=0 pinned core */
} sysctx;

static void sysctx_init(sysctx *s){
  memset(s,0,sizeof(*s));
  s->load1=s->load5=s->load15=-1.0;
  s->aff_one=-1;
}

static void sysctx_collect(sysctx *s){
#if defined(__linux__) || defined(__ANDROID__)
  /* cpu model */
  {
    char buf[4096];
    if(read_file_snip("/proc/cpuinfo",buf,sizeof(buf),4095)){
      const char *p=strstr(buf,"model name");
      if(!p) p=strstr(buf,"Hardware");
      if(!p) p=strstr(buf,"Processor");
      if(p){
        const char *c=strchr(p,':');
        if(c){
          while(*c==':'||*c==' '||*c=='\t') c++;
          strncpy(s->cpu_model,c,sizeof(s->cpu_model)-1);
          char *nl=strchr(s->cpu_model,'\n'); if(nl) *nl=0;
        }
      }
    }
    if(!s->cpu_model[0]) strncpy(s->cpu_model,"unknown",sizeof(s->cpu_model)-1);
  }

  /* meminfo */
  {
    FILE *f=fopen("/proc/meminfo","rb");
    if(f){
      char line[256];
      while(fgets(line,sizeof(line),f)){
        if(!s->mem_total[0] && strncmp(line,"MemTotal:",9)==0){
          char *p=line+9; while(*p==' '||*p=='\t') p++;
          strncpy(s->mem_total,p,sizeof(s->mem_total)-1);
          char *nl=strchr(s->mem_total,'\n'); if(nl) *nl=0;
        }
        if(!s->mem_avail[0] && strncmp(line,"MemAvailable:",13)==0){
          char *p=line+13; while(*p==' '||*p=='\t') p++;
          strncpy(s->mem_avail,p,sizeof(s->mem_avail)-1);
          char *nl=strchr(s->mem_avail,'\n'); if(nl) *nl=0;
        }
        if(s->mem_total[0] && s->mem_avail[0]) break;
      }
      fclose(f);
    }
  }

  /*<MID:LOADAVG>*/
  /* loadavg via /proc/loadavg (no getloadavg) */
  {
    char l[256];
    if(read_first_line("/proc/loadavg", l, sizeof(l))){
      double a=0,b=0,c=0;
      if(sscanf(l,"%lf %lf %lf",&a,&b,&c)==3){
        s->load1=a; s->load5=b; s->load15=c;
      }
    }
  }
  /*</MID:LOADAVG>*/

  /* freq (best-effort) */
  {
    char b[128];
    if(read_first_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq",b,sizeof(b)))
      strncpy(s->freq_khz,b,sizeof(s->freq_khz)-1);
    else if(read_first_line("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq",b,sizeof(b)))
      strncpy(s->freq_khz,b,sizeof(s->freq_khz)-1);
  }

  /* nice */
  {
    errno=0;
    int pr=getpriority(PRIO_PROCESS,0);
    if(errno==0) s->nicev=pr;
  }

  /* affinity snapshot (best-effort; compile-safe) */
  {
#if defined(CPU_SETSIZE) && defined(CPU_SET)
    cpu_set_t set;
    CPU_ZERO(&set);
    if(sched_getaffinity(0,sizeof(set),&set)==0){
      int cnt=0,last=-1;
      for(int i=0;i<CPU_SETSIZE;i++){
        if(CPU_ISSET(i,&set)){ cnt++; last=i; }
      }
      if(cnt==1) s->aff_one=last;
    }
#endif
  }
#else
  strncpy(s->cpu_model,"unknown",sizeof(s->cpu_model)-1);
#endif
}

static void sysctx_print(const sysctx *s){
  printf("[sys] cpu   : %s\n", s->cpu_model[0]?s->cpu_model:"unknown");
  if(s->freq_khz[0]) printf("[sys] freq  : %s kHz (best-effort)\n", s->freq_khz);
  if(s->mem_total[0]) printf("[sys] mem   : total=%s | avail=%s\n", s->mem_total, s->mem_avail[0]?s->mem_avail:"?");
  if(s->load1>=0) printf("[sys] load  : %.2f %.2f %.2f\n", s->load1, s->load5, s->load15);
  printf("[sys] nice  : %d\n", s->nicev);
  if(s->aff_one>=0) printf("[sys] aff   : pinned cpu=%d\n", s->aff_one);
}

/* =========================== /proc/stat busy% =========================== */
typedef struct { uint64_t u,n,s,i,w,iq,si,st; } cstat;

static int read_proc_stat(cstat *x){
#if defined(__linux__) || defined(__ANDROID__)
  FILE *f=fopen("/proc/stat","rb");
  if(!f) return 0;
  char line[512];
  if(!fgets(line,sizeof(line),f)){ fclose(f); return 0; }
  fclose(f);
  memset(x,0,sizeof(*x));
  char cpu[8]={0};
  int n=sscanf(line,"%7s %llu %llu %llu %llu %llu %llu %llu %llu",
    cpu,
    (unsigned long long*)&x->u,(unsigned long long*)&x->n,(unsigned long long*)&x->s,
    (unsigned long long*)&x->i,(unsigned long long*)&x->w,(unsigned long long*)&x->iq,
    (unsigned long long*)&x->si,(unsigned long long*)&x->st
  );
  return (n>=5);
#else
  (void)x; return 0;
#endif
}

static double busy_pct(const cstat *a, const cstat *b){
  uint64_t idle_a=a->i + a->w;
  uint64_t idle_b=b->i + b->w;
  uint64_t non_a=a->u+a->n+a->s+a->iq+a->si+a->st;
  uint64_t non_b=b->u+b->n+b->s+b->iq+b->si+b->st;
  uint64_t idle_d=(idle_b>=idle_a)?(idle_b-idle_a):0;
  uint64_t non_d =(non_b >=non_a )?(non_b -non_a ):0;
  uint64_t tot=idle_d+non_d;
  if(tot==0) return -1.0;
  return (double)non_d*100.0/(double)tot;
}

/* =========================== results as indexed vector =========================== */
enum { R_INT=0, R_FP, R_BW, R_RAND, R_HASH, R_PCPU, R_SBUSY, R_SCORE, R_N };
static const char *RNAME[R_N]={
  "cpu_int_mops","cpu_fp_mflops","mem_bw_gbs","mem_rand_ns","hash_crc_gbs","proc_cpu_pct","sys_busy_pct","score"
};

/* =========================== kernels =========================== */
static double bench_int(double sec){
  uint64_t seed=0xC0FFEE123456789ull;
  volatile uint64_t sink=0;
  uint64_t t0=ns_now(), end=t0+(uint64_t)(sec*1e9);
  uint64_t it=0;
  while(ns_now()<end){
    uint64_t x=xs64(&seed);
    x^=(x<<9)+0x9E3779B97F4A7C15ull;
    x*=0xD6E8FEB86659FD93ull;
    x^=x>>32;
    sink^=x;
    it++;
  }
  (void)sink;
  double s=ns_to_s(ns_now()-t0);
  double ops=(double)it*6.0;
  return (ops/1e6)/s;
}

static double bench_fp(double sec){
  volatile double sink=0.0;
  double a=1.0000001,b=1.0000002,c=0.9999997;
  uint64_t t0=ns_now(), end=t0+(uint64_t)(sec*1e9);
  uint64_t it=0;
  while(ns_now()<end){
    a=a*b+c; b=b*c+a; c=c*a+b;
    sink += a+b+c;
    it++;
  }
  (void)sink;
  double s=ns_to_s(ns_now()-t0);
  double fl=(double)it*12.0;
  return (fl/1e6)/s;
}

static double bench_bw(uint8_t *src, uint8_t *dst, size_t bytes, double sec){
  memset(src,0xA5,bytes);
  memset(dst,0x5A,bytes);
  uint64_t t0=ns_now(), end=t0+(uint64_t)(sec*1e9);
  uint64_t it=0;
  while(ns_now()<end){
    memcpy(dst,src,bytes);
    dst[(it*1315423911u) & (bytes-1)] ^= (uint8_t)it;
    it++;
  }
  double s=ns_to_s(ns_now()-t0);
  double gb=((double)bytes*(double)it)/(1024.0*1024.0*1024.0);
  return gb/s;
}

static void shuffle_u32(uint32_t *a, size_t n, uint64_t *seed){
  for(size_t i=n-1;i>0;i--){
    size_t j=(size_t)(xs64(seed) % (uint64_t)(i+1));
    uint32_t t=a[i]; a[i]=a[j]; a[j]=t;
  }
}

static double bench_rand(uint32_t *idx, size_t n, double sec){
  (void)n;
  volatile uint32_t sink=0;
  uint32_t cur=0;
  uint64_t t0=ns_now(), end=t0+(uint64_t)(sec*1e9);
  uint64_t steps=0;
  while(ns_now()<end){
    cur=idx[cur];
    sink^=cur;
    steps++;
  }
  (void)sink;
  if(steps==0) return 0.0;
  return (double)(ns_now()-t0)/(double)steps;
}

static double bench_hash(uint8_t *buf, size_t bytes, double sec){
  volatile uint64_t hs=0;
  volatile uint32_t cs=0;
  uint64_t t0=ns_now(), end=t0+(uint64_t)(sec*1e9);
  uint64_t it=0;
  while(ns_now()<end){
    hs ^= djb2_64(buf,bytes);
    cs ^= crc32_ieee(buf,bytes);
    buf[(it*2654435761u)&(bytes-1)] ^= (uint8_t)it;
    it++;
  }
  (void)hs; (void)cs;
  double s=ns_to_s(ns_now()-t0);
  double gb=((double)bytes*(double)it)/(1024.0*1024.0*1024.0);
  return gb/s;
}

/* =========================== score =========================== */
static double score_comp(const double r[R_N]){
  /* não pune demais RAM fraca: BW e RAND entram com peso moderado */
  double a=r[R_INT]*0.35;
  double b=r[R_FP ]*0.25;
  double c=r[R_BW ]*30.0;
  double d=(r[R_RAND]>0)? (1200.0/r[R_RAND]) : 0.0;
  double e=r[R_HASH]*35.0;
  return a+b+c+d+e;
}

/* =========================== JSON/CSV =========================== */
static void write_json(const char *path, const sysctx *s, const double r[R_N], size_t block_bytes, double seconds, int repeat){
  FILE *f=fopen(path,"wb"); if(!f){ fprintf(stderr,"json open failed: %s\n",path); return; }
  fprintf(f,"{\n");
  fprintf(f,"  \"cpu_model\": \"%s\",\n", s->cpu_model);
  if(s->freq_khz[0]) fprintf(f,"  \"cpu_freq_khz\": %s,\n", s->freq_khz);
  if(s->mem_total[0]) fprintf(f,"  \"mem_total\": \"%s\",\n", s->mem_total);
  if(s->mem_avail[0]) fprintf(f,"  \"mem_avail\": \"%s\",\n", s->mem_avail);
  if(s->load1>=0) fprintf(f,"  \"loadavg\": [%.4f, %.4f, %.4f],\n", s->load1,s->load5,s->load15);
  fprintf(f,"  \"nice\": %d,\n", s->nicev);
  if(s->aff_one>=0) fprintf(f,"  \"pinned_cpu\": %d,\n", s->aff_one);
  fprintf(f,"  \"block_bytes\": %zu,\n", block_bytes);
  fprintf(f,"  \"seconds\": %.4f,\n", seconds);
  fprintf(f,"  \"repeat\": %d,\n", repeat);
  fprintf(f,"  \"results\": {\n");
  for(int i=0;i<R_N;i++){
    fprintf(f,"    \"%s\": %.6f%s\n", RNAME[i], r[i], (i==R_N-1)?"":",");
  }
  fprintf(f,"  }\n");
  fprintf(f,"}\n");
  fclose(f);
}

static void write_csv(const char *path, const sysctx *s, const double r[R_N], size_t block_bytes, double seconds, int repeat){
  FILE *f=fopen(path,"wb"); if(!f){ fprintf(stderr,"csv open failed: %s\n",path); return; }
  fprintf(f,"cpu_model,block_bytes,seconds,repeat");
  for(int i=0;i<R_N;i++) fprintf(f,",%s", RNAME[i]);
  fprintf(f,"\n");
  fprintf(f,"\"%s\",%zu,%.4f,%d", s->cpu_model, block_bytes, seconds, repeat);
  for(int i=0;i<R_N;i++) fprintf(f,",%.6f", r[i]);
  fprintf(f,"\n");
  fclose(f);
}

/* =========================== CLI =========================== */
typedef struct {
  size_t mb;
  double sec;
  int rep;
  int quick;
  int no_clear;
  int no_color;
  int do_aff;
  int pin;
  int do_nice;
  int nicev;
  uint64_t seed;
  const char *json;
  const char *csv;
} cfg;

static void cfg_def(cfg *c){
  memset(c,0,sizeof(*c));
  c->mb=128;
  c->sec=1.0;
  c->rep=1;
  c->seed=0xA11CE5EED1234ull;
  c->pin=-1;
}

static int streq(const char *a, const char *b){ return strcmp(a,b)==0; }

static void usage(const char *a0){
  printf("Usage: %s [flags]\n",a0);
  puts("Flags:");
  puts("  --size-mb N     (pow2 rounded; default 128)");
  puts("  --seconds S     (per test; default 1.0)");
  puts("  --repeat N      (default 1)");
  puts("  --quick         (same as defaults; kept for CatOS wrapper)");
  puts("  --seed HEX      (default A11CE5EED1234)");
  puts("  --no-clear      (no clear)");
  puts("  --no-color      (no ANSI)");
  puts("  --pin-cpu N     (best-effort; if supported)");
  puts("  --nice N        (best-effort)");
  puts("  --json path     (write json)");
  puts("  --csv  path     (write csv)");
  puts("  --help");
}

static void parse(cfg *c, int ac, char **av){
  for(int i=1;i<ac;i++){
    const char *a=av[i];
    if(streq(a,"--help")){ usage(av[0]); exit(0); }
    else if(streq(a,"--size-mb") && i+1<ac) c->mb=(size_t)strtoull(av[++i],0,10);
    else if(streq(a,"--seconds") && i+1<ac) c->sec=strtod(av[++i],0);
    else if(streq(a,"--repeat") && i+1<ac) c->rep=atoi(av[++i]);
    else if(streq(a,"--quick")) c->quick=1;
    else if(streq(a,"--seed") && i+1<ac) c->seed=strtoull(av[++i],0,16);
    else if(streq(a,"--no-clear")) c->no_clear=1;
    else if(streq(a,"--no-color")) c->no_color=1;
    else if(streq(a,"--pin-cpu") && i+1<ac){ c->do_aff=1; c->pin=atoi(av[++i]); }
    else if(streq(a,"--nice") && i+1<ac){ c->do_nice=1; c->nicev=atoi(av[++i]); }
    else if(streq(a,"--json") && i+1<ac) c->json=av[++i];
    else if(streq(a,"--csv")  && i+1<ac) c->csv =av[++i];
    else { fprintf(stderr,"Unknown: %s\n",a); usage(av[0]); exit(1); }
  }
  if(c->mb<8) c->mb=8;
  if(c->sec<0.2) c->sec=0.2;
  if(c->rep<1) c->rep=1;
}

static size_t next_pow2(size_t x){
  size_t p=1; while(p<x) p<<=1; return p;
}

static void maybe_aff(const cfg *c){
#if defined(__linux__) || defined(__ANDROID__)
#if defined(CPU_SETSIZE) && defined(CPU_SET)
  if(!c->do_aff) return;
  if(c->pin<0 || c->pin>=CPU_SETSIZE){ fprintf(stderr,"[warn] invalid cpu: %d\n",c->pin); return; }
  cpu_set_t set; CPU_ZERO(&set); CPU_SET(c->pin,&set);
  if(sched_setaffinity(0,sizeof(set),&set)!=0)
    fprintf(stderr,"[warn] sched_setaffinity: %s\n",strerror(errno));
  else
    printf("[cfg] aff   : pinned cpu=%d\n",c->pin);
#else
  (void)c;
#endif
#else
  (void)c;
#endif
}

static void maybe_nice(const cfg *c){
#if defined(__linux__) || defined(__ANDROID__)
  if(!c->do_nice) return;
  if(setpriority(PRIO_PROCESS,0,c->nicev)!=0)
    fprintf(stderr,"[warn] setpriority(%d): %s\n",c->nicev,strerror(errno));
  else
    printf("[cfg] nice  : %d\n",c->nicev);
#else
  (void)c;
#endif
}

int main(int ac, char **av){
  cfg c; cfg_def(&c); parse(&c,ac,av);

  g_clear = c.no_clear ? 0 : 1;
  g_color = c.no_color ? 0 : 1;

  term_clear();
  bbs_banner();

  sysctx sx; sysctx_init(&sx);
  sysctx_collect(&sx);
  sysctx_print(&sx);

  maybe_nice(&c);
  maybe_aff(&c);

  crc32_init();

  size_t bytes = next_pow2(c.mb * (size_t)1024 * (size_t)1024);

  /* allocate with graceful fallback */
  uint8_t *src=NULL, *dst=NULL;
  uint32_t *idx=NULL;
  size_t tryb=bytes;
  for(;;){
    src=(uint8_t*)xmalloc_aligned(64, tryb);
    dst=(uint8_t*)xmalloc_aligned(64, tryb);
    idx=(uint32_t*)xmalloc_aligned(64, (tryb/4));
    if(src && dst && idx){ bytes=tryb; break; }
    tryb >>= 1;
    if(tryb < (size_t)8*1024*1024){ fprintf(stderr,"alloc too small\n"); return 2; }
  }

  /* init idx as permutation for pointer-chase */
  size_t n = bytes/4;
  for(size_t i=0;i<n;i++) idx[i]=(uint32_t)i;
  uint64_t seed=c.seed;
  shuffle_u32(idx,n,&seed);
  for(size_t i=0;i<n-1;i++) idx[idx[i]] = idx[i+1];
  idx[idx[n-1]] = idx[0];

  double r[R_N]={0};

  /* telemetry snapshot */
  cstat A,B;
  int have_stat = read_proc_stat(&A);

  uint64_t w0=ns_now(), p0=ns_proc_cpu();

  /* repeat loop */
  for(int k=0;k<c.rep;k++){
    r[R_INT]  += bench_int(c.sec);
    r[R_FP ]  += bench_fp (c.sec);
    r[R_BW ]  += bench_bw(src,dst,bytes,c.sec);
    r[R_RAND] += bench_rand(idx,n,c.sec);
    r[R_HASH] += bench_hash(src,bytes,c.sec);
  }
  for(int i=0;i<R_N;i++) r[i] /= (double)c.rep;

  uint64_t w1=ns_now(), p1=ns_proc_cpu();
  double wall=ns_to_s(w1-w0);
  double pcpu=ns_to_s(p1-p0);
  r[R_PCPU] = (wall>0)? (pcpu*100.0/wall) : 0.0;

  if(have_stat && read_proc_stat(&B)) r[R_SBUSY]=busy_pct(&A,&B);
  else r[R_SBUSY]=-1.0;

  r[R_SCORE]=score_comp(r);

  /* report */
  const char *g = g_color ? "\033[1;32m" : "";
  const char *y = g_color ? "\033[1;33m" : "";
  const char *rr= g_color ? "\033[1;31m" : "";
  const char *z = g_color ? "\033[0m" : "";

  puts("------------------------------------------------------------");
  printf("%s[bench]%s CPU_INT  : %8.2f Mops/s\n", g, z, r[R_INT]);
  printf("%s[bench]%s CPU_FP   : %8.2f Mflop/s\n", g, z, r[R_FP]);
  printf("%s[bench]%s MEM_BW   : %8.2f GB/s   (seq)\n", g, z, r[R_BW]);
  printf("%s[bench]%s MEM_RAND : %8.2f ns/step (rand)\n", g, z, r[R_RAND]);
  printf("%s[bench]%s HASH+CRC : %8.2f GB/s\n", g, z, r[R_HASH]);
  puts("------------------------------------------------------------");
  printf("%s[tele]%s proc_cpu%%: %6.2f%% | sys_busy%%: %s%.2f%%%s\n",
         y, z, r[R_PCPU],
         (r[R_SBUSY]<0? rr:y), (r[R_SBUSY]<0? 0.0:r[R_SBUSY]), z);
  printf("%s[score]%s %.2f  (composite; not absolute truth)\n", y, z, r[R_SCORE]);
  puts("------------------------------------------------------------");
  puts("[note] determinismo: seed fixa + bloco pow2 => repetível; variação vem do mundo real (load/thermal).");
  puts("[note] geometria: 'vetor' aqui = fluxo seq + bússola de acesso rand.");

  if(c.json){ write_json(c.json,&sx,r,bytes,c.sec,c.rep); printf("[out] json : %s\n",c.json); }
  if(c.csv ){ write_csv (c.csv ,&sx,r,bytes,c.sec,c.rep); printf("[out] csv  : %s\n",c.csv ); }

  free(src); free(dst); free(idx);
  return 0;
}
C

# ────────────────────────────────────────────────────────────────
# bin/catos wrapper (build/run/quick/json/csv)
# ────────────────────────────────────────────────────────────────
cat > "$ROOT/bin/catos" <<SH
#!/data/data/com.termux/files/usr/bin/sh
set -eu
ROOT="\${ROOT:-$ROOT}"
CC="\${CC:-$CC}"
CFLAGS="\${CFLAGS:-$CFLAGS}"
LDFLAGS="\${LDFLAGS:-$LDFLAGS}"

SRC="\$ROOT/src/raf_bench_bbs.c"
OUT="\$ROOT/out/raf_bench"

cmd="\${1:-help}"
shift 2>/dev/null || true

build(){
  echo "[build] \$CC \$CFLAGS \"\$SRC\" -o \"\$OUT\" \$LDFLAGS"
  "\$CC" \$CFLAGS "\$SRC" -o "\$OUT" \$LDFLAGS
}

run(){
  [ -x "\$OUT" ] || build
  "\$OUT" "\$@"
}

case "\$cmd" in
  build) build ;;
  quick) run --size-mb 128 --seconds 1 --repeat 1 ;;
  run)   run "\$@" ;;
  json)  run --json "\$ROOT/out/bench.json" --size-mb 128 --seconds 1 --repeat 1 ;;
  csv)   run --csv  "\$ROOT/out/bench.csv"  --size-mb 128 --seconds 1 --repeat 1 ;;
  *)     echo "CatOS commands: build | quick | run ... | json | csv"
         echo "Examples:"
         echo "  \$ROOT/bin/catos quick"
         echo "  \$ROOT/bin/catos run --size-mb 256 --seconds 2 --repeat 3 --json \$ROOT/out/bench.json --csv \$ROOT/out/bench.csv"
         ;;
esac
SH
chmod +x "$ROOT/bin/catos"

# ────────────────────────────────────────────────────────────────
# ativo scripts (atalhos)
# ────────────────────────────────────────────────────────────────
cat > "$ROOT/ativo/run_quick.sh" <<'SH'
#!/data/data/com.termux/files/usr/bin/sh
set -eu
ROOT="${ROOT:-CatOS_RAFAELIA}"
"$ROOT/bin/catos" quick
SH
chmod +x "$ROOT/ativo/run_quick.sh"

cat > "$ROOT/ativo/run_full.sh" <<'SH'
#!/data/data/com.termux/files/usr/bin/sh
set -eu
ROOT="${ROOT:-CatOS_RAFAELIA}"
"$ROOT/bin/catos" run --size-mb 256 --seconds 2 --repeat 3 --json "$ROOT/out/bench.json" --csv "$ROOT/out/bench.csv"
SH
chmod +x "$ROOT/ativo/run_full.sh"

echo "✅ CatOS pronto em: $ROOT"
echo "▶ build: $ROOT/bin/catos build"
echo "▶ quick: $ROOT/bin/catos quick"
echo "▶ full : $ROOT/ativo/run_full.sh"
echo "▶ MID  : $ROOT/tools/mid_replace.sh  (ver docs/MID_MAP.md)"
echo "▶ JSONF : $ROOT/tools/json_find.sh shared_conversations.json \"seu padrão\""
