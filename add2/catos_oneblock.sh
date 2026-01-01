#!/data/data/com.termux/files/usr/bin/sh
set -eu

ROOT="${ROOT:-CatOS_RAFAELIA}"
mkdir -p "$ROOT/bin" "$ROOT/src" "$ROOT/out" "$ROOT/docs" "$ROOT/patches" "$ROOT/ativo"

# ───────────────────────────────────────────────────────────────
# src/raf_bench_bbs.c  (no-deps, Termux-friendly)
# ───────────────────────────────────────────────────────────────
cat > "$ROOT/src/raf_bench_bbs.c" <<'C_EOF'
/* raf_bench_bbs.c — RAFAELIA CatOS Bench (BBS/CLI) 🧮⚡🦉
   - Single-file C (no external deps)
   - Termux/Android friendly (NO getloadavg; uses /proc/loadavg)
   - BBS output + JSON/CSV
   - Telemetry: /proc/cpuinfo, meminfo, loadavg, /proc/stat busy%, process cpu%
   Build: clang -O3 -DNDEBUG raf_bench_bbs.c -o raf_bench
*/

#define _GNU_SOURCE 1
#define _POSIX_C_SOURCE 200809L

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
  #include <sched.h>
#endif

/* =========================== UI =========================== */
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
  printf("%s║  RAFAELIA :: CatOS Bench (C puro / auditável / no-deps)      ║%s\n", c2, c0);
  printf("%s║  ψ→χ→ρ→Δ→Σ→Ω  |  INT/FP  MEM(BW+LAT)  HASH+CRC  TELEMETRIA    ║%s\n", c2, c0);
  printf("%s╚══════════════════════════════════════════════════════════════╝%s\n", c1, c0);
}

/* =========================== time =========================== */
static inline uint64_t ns_now(void){
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec*1000000000ull + (uint64_t)ts.tv_nsec;
}
static inline uint64_t ns_proc_cpu(void){
  struct timespec ts;
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
  return (uint64_t)ts.tv_sec*1000000000ull + (uint64_t)ts.tv_nsec;
}
static inline double ns_to_s(uint64_t ns){ return (double)ns / 1e9; }

/* =========================== tiny helpers =========================== */
static void* xmalloc(size_t n){
  void *p = malloc(n);
  if(!p){ fprintf(stderr, "alloc fail: %zu\n", n); exit(1); }
  return p;
}
static void* xmalloc_aligned(size_t align, size_t n){
  void *p = NULL;
#if (defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112L) || defined(__ANDROID__)
  if(posix_memalign(&p, align, n)!=0) p=NULL;
#else
  (void)align;
  p = malloc(n);
#endif
  if(!p){ fprintf(stderr, "alloc fail: %zu\n", n); exit(1); }
  return p;
}
static size_t next_pow2(size_t x){
  size_t p=1;
  while(p<x) p<<=1;
  return p;
}

/* =========================== RNG =========================== */
static inline uint64_t xorshift64(uint64_t *s){
  uint64_t x=*s;
  x^=x<<13; x^=x>>7; x^=x<<17;
  *s=x;
  return x;
}

/* =========================== DJB2 64 =========================== */
static inline uint64_t djb2_64(const uint8_t *p, size_t n){
  uint64_t h=5381u;
  for(size_t i=0;i<n;i++) h = ((h<<5)+h) + (uint64_t)p[i];
  return h;
}

/* =========================== CRC32 (IEEE) =========================== */
static uint32_t crcT[256];
static void crc32_init(void){
  for(uint32_t i=0;i<256;i++){
    uint32_t c=i;
    for(int k=0;k<8;k++)
      c = (c & 1u) ? (0xEDB88320u ^ (c>>1)) : (c>>1);
    crcT[i]=c;
  }
}
static inline uint32_t crc32_ieee(const uint8_t *p, size_t n){
  uint32_t c=0xFFFFFFFFu;
  for(size_t i=0;i<n;i++)
    c = crcT[(c ^ p[i]) & 0xFFu] ^ (c>>8);
  return c ^ 0xFFFFFFFFu;
}

/* =========================== /proc readers =========================== */
static int read_file_snip(const char *path, char *out, size_t out_sz, size_t max_bytes){
  FILE *f=fopen(path,"rb");
  if(!f) return 0;
  size_t n=fread(out,1,(max_bytes<out_sz?max_bytes:out_sz-1),f);
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
  while(n && (out[n-1]=='\n'||out[n-1]=='\r')) out[--n]=0;
  return 1;
}
static int read_loadavg(double *a,double *b,double *c){
#if defined(__linux__) || defined(__ANDROID__)
  char buf[128];
  if(!read_first_line("/proc/loadavg", buf, sizeof(buf))) return 0;
  /* "0.12 0.34 0.56 1/123 4567" */
  if(sscanf(buf, "%lf %lf %lf", a,b,c)==3) return 1;
#endif
  return 0;
}

/* =========================== sys ctx =========================== */
typedef struct {
  char cpu[256];
  char freq_khz[64];
  char mem_total[64];
  char mem_avail[64];
  double la1,la5,la15;
  int nicev;
  int pinned; /* -1 unknown, else cpu id */
} sys_ctx;

static void sysctx_collect(sys_ctx *s){
  memset(s,0,sizeof(*s));
  s->la1=s->la5=s->la15=-1.0;
  s->pinned=-1;

#if defined(__linux__) || defined(__ANDROID__)
  /* cpu model (best effort) */
  {
    char buf[4096];
    if(read_file_snip("/proc/cpuinfo",buf,sizeof(buf),4095)){
      const char *p=strstr(buf,"model name");
      if(!p) p=strstr(buf,"Hardware");
      if(p){
        const char *c=strchr(p,':');
        if(c){
          while(*c==':'||*c==' '||*c=='\t') c++;
          strncpy(s->cpu,c,sizeof(s->cpu)-1);
          char *nl=strchr(s->cpu,'\n'); if(nl) *nl=0;
        }
      }
    }
    if(!s->cpu[0]) strncpy(s->cpu,"unknown",sizeof(s->cpu)-1);
  }

  /* freq */
  {
    char b[128];
    if(read_first_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq",b,sizeof(b)) ||
       read_first_line("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq",b,sizeof(b))){
      strncpy(s->freq_khz,b,sizeof(s->freq_khz)-1);
    }
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

  /* loadavg via /proc/loadavg (Termux ok) */
  {
    double a,b,c;
    if(read_loadavg(&a,&b,&c)){ s->la1=a; s->la5=b; s->la15=c; }
  }

  /* nice */
  {
    errno=0;
    int pr=getpriority(PRIO_PROCESS,0);
    if(errno==0) s->nicev=pr;
  }

  /* affinity (best effort) */
  {
    cpu_set_t set;
    CPU_ZERO(&set);
    if(sched_getaffinity(0,sizeof(set),&set)==0){
      int count=0,last=-1;
      for(int i=0;i<CPU_SETSIZE;i++){
        if(CPU_ISSET(i,&set)){ count++; last=i; }
      }
      if(count==1) s->pinned=last;
      else s->pinned=-1;
    }
  }
#else
  strncpy(s->cpu,"unknown",sizeof(s->cpu)-1);
#endif
}

typedef struct { uint64_t user,nice,sys,idle,iowait,irq,soft,steal; } cpu_stat;

static int read_proc_stat(cpu_stat *st){
#if defined(__linux__) || defined(__ANDROID__)
  FILE *f=fopen("/proc/stat","rb");
  if(!f) return 0;
  char line[512];
  if(!fgets(line,sizeof(line),f)){ fclose(f); return 0; }
  fclose(f);
  memset(st,0,sizeof(*st));
  char cpu[8]={0};
  int n=sscanf(line,"%7s %llu %llu %llu %llu %llu %llu %llu %llu",
               cpu,
               (unsigned long long*)&st->user,
               (unsigned long long*)&st->nice,
               (unsigned long long*)&st->sys,
               (unsigned long long*)&st->idle,
               (unsigned long long*)&st->iowait,
               (unsigned long long*)&st->irq,
               (unsigned long long*)&st->soft,
               (unsigned long long*)&st->steal);
  return (n>=5);
#else
  (void)st; return 0;
#endif
}
static double cpu_busy_pct(const cpu_stat *a,const cpu_stat *b){
  uint64_t idle_a=a->idle+a->iowait, idle_b=b->idle+b->iowait;
  uint64_t non_a =a->user+a->nice+a->sys+a->irq+a->soft+a->steal;
  uint64_t non_b =b->user+b->nice+b->sys+b->irq+b->soft+b->steal;
  uint64_t idle_d=(idle_b>=idle_a)?(idle_b-idle_a):0;
  uint64_t non_d =(non_b>=non_a)?(non_b-non_a):0;
  uint64_t tot=idle_d+non_d;
  if(!tot) return -1.0;
  return (double)non_d*100.0/(double)tot;
}

/* =========================== bench kernels =========================== */
static double bench_int(double sec){
  if(sec<0.25) sec=0.25;
  uint64_t seed=0xC0FFEE123456789ull;
  volatile uint64_t sink=0;
  uint64_t t0=ns_now(), end=t0+(uint64_t)(sec*1e9);
  uint64_t it=0;
  while(ns_now()<end){
    uint64_t x=xorshift64(&seed);
    x ^= (x<<9) + 0x9E3779B97F4A7C15ull;
    x *= 0xD6E8FEB86659FD93ull;
    x ^= x>>32;
    sink ^= x;
    it++;
  }
  (void)sink;
  double s=ns_to_s(ns_now()-t0);
  if(s<=0) return 0.0;
  return ((double)it*6.0/1e6)/s;
}

static double bench_fp(double sec){
  if(sec<0.25) sec=0.25;
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
  if(s<=0) return 0.0;
  return ((double)it*12.0/1e6)/s;
}

/* sequential memcpy BW */
static double bench_mem_bw(uint8_t *a,uint8_t *b,size_t bytes,double sec){
  if(sec<0.25) sec=0.25;
  memset(a,0xA5,bytes);
  memset(b,0x5A,bytes);

  uint64_t t0=ns_now(), end=t0+(uint64_t)(sec*1e9);
  uint64_t it=0;
  while(ns_now()<end){
    memcpy(b,a,bytes);
    b[(it*1315423911u) & (bytes-1)] ^= (uint8_t)it;
    it++;
  }
  double s=ns_to_s(ns_now()-t0);
  if(s<=0) return 0.0;
  double gb=((double)bytes*(double)it)/(1024.0*1024.0*1024.0);
  return gb/s;
}

/* pointer-chase latency (ns/step) using ONE cycle permutation */
static void make_cycle(uint32_t *idx,size_t n,uint64_t *seed){
  /* perm */
  for(size_t i=0;i<n;i++) idx[i]=(uint32_t)i;
  for(size_t i=n-1;i>0;i--){
    size_t j=(size_t)(xorshift64(seed)%(uint64_t)(i+1));
    uint32_t t=idx[i]; idx[i]=idx[j]; idx[j]=t;
  }
  /* link into one cycle: idx[perm[i]] = perm[i+1] */
  uint32_t first=idx[0];
  for(size_t i=0;i<n-1;i++){
    uint32_t from=idx[i], to=idx[i+1];
    idx[from]=to;
  }
  idx[idx[n-1]]=first;
}

static double bench_mem_rand(uint32_t *idx,size_t n,double sec){
  if(sec<0.25) sec=0.25;
  volatile uint32_t sink=0;
  uint32_t cur=0;
  uint64_t t0=ns_now(), end=t0+(uint64_t)(sec*1e9);
  uint64_t steps=0;
  while(ns_now()<end){
    cur = idx[cur];
    sink ^= cur;
    steps++;
  }
  (void)sink;
  uint64_t dt=ns_now()-t0;
  if(steps==0) return 0.0;
  return (double)dt/(double)steps;
}

static double bench_hash_crc(uint8_t *buf,size_t bytes,double sec){
  if(sec<0.25) sec=0.25;
  volatile uint64_t hs=0;
  volatile uint32_t cs=0;
  uint64_t t0=ns_now(), end=t0+(uint64_t)(sec*1e9);
  uint64_t it=0;
  while(ns_now()<end){
    hs ^= djb2_64(buf,bytes);
    cs ^= crc32_ieee(buf,bytes);
    buf[(it*2654435761u) & (bytes-1)] ^= (uint8_t)it;
    it++;
  }
  (void)hs; (void)cs;
  double s=ns_to_s(ns_now()-t0);
  if(s<=0) return 0.0;
  double gb=((double)bytes*(double)it)/(1024.0*1024.0*1024.0);
  return gb/s;
}

/* =========================== result + score =========================== */
typedef struct {
  double i_mops, f_mflops, bw_gbs, lat_ns, hash_gbs;
  double proc_cpu_pct, sys_busy_pct;
  double score;
} R;

static double score_comp(const R *r){
  /* não “pune” celular por RAM: peso maior em INT/FP/HASH, menor em BW */
  double a=r->i_mops*0.40;
  double b=r->f_mflops*0.25;
  double c=r->hash_gbs*55.0;
  double d=r->bw_gbs*20.0;
  double e=(r->lat_ns>0)? (1800.0/r->lat_ns) : 0.0;
  return a+b+c+d+e;
}

/* =========================== JSON/CSV =========================== */
static void write_json(const char *path,const sys_ctx *s,const R *r,size_t bytes,double sec,int rep){
  FILE *f=fopen(path,"wb");
  if(!f){ fprintf(stderr,"json open fail: %s\n",path); return; }
  fprintf(f,"{\n");
  fprintf(f,"  \"cpu_model\": \"%s\",\n", s->cpu);
  if(s->freq_khz[0]) fprintf(f,"  \"cpu_freq_khz\": %s,\n", s->freq_khz);
  if(s->mem_total[0]) fprintf(f,"  \"mem_total\": \"%s\",\n", s->mem_total);
  if(s->mem_avail[0]) fprintf(f,"  \"mem_avail\": \"%s\",\n", s->mem_avail);
  if(s->la1>=0) fprintf(f,"  \"loadavg\": [%.4f, %.4f, %.4f],\n", s->la1,s->la5,s->la15);
  fprintf(f,"  \"nice\": %d,\n", s->nicev);
  if(s->pinned>=0) fprintf(f,"  \"pinned_cpu\": %d,\n", s->pinned);
  fprintf(f,"  \"block_bytes\": %zu,\n", bytes);
  fprintf(f,"  \"seconds\": %.4f,\n", sec);
  fprintf(f,"  \"repeat\": %d,\n", rep);
  fprintf(f,"  \"results\": {\n");
  fprintf(f,"    \"cpu_int_mops\": %.6f,\n", r->i_mops);
  fprintf(f,"    \"cpu_fp_mflops\": %.6f,\n", r->f_mflops);
  fprintf(f,"    \"mem_bw_gbs\": %.6f,\n", r->bw_gbs);
  fprintf(f,"    \"mem_rand_ns\": %.6f,\n", r->lat_ns);
  fprintf(f,"    \"hash_crc_gbs\": %.6f,\n", r->hash_gbs);
  fprintf(f,"    \"proc_cpu_pct\": %.6f,\n", r->proc_cpu_pct);
  fprintf(f,"    \"sys_busy_pct\": %.6f,\n", r->sys_busy_pct);
  fprintf(f,"    \"score\": %.6f\n", r->score);
  fprintf(f,"  }\n}\n");
  fclose(f);
}

static void write_csv(const char *path,const sys_ctx *s,const R *r,size_t bytes,double sec,int rep){
  FILE *f=fopen(path,"wb");
  if(!f){ fprintf(stderr,"csv open fail: %s\n",path); return; }
  fprintf(f,"cpu_model,block_bytes,seconds,repeat,cpu_int_mops,cpu_fp_mflops,mem_bw_gbs,mem_rand_ns,hash_crc_gbs,proc_cpu_pct,sys_busy_pct,score\n");
  fprintf(f,"\"%s\",%zu,%.4f,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
          s->cpu,bytes,sec,rep,r->i_mops,r->f_mflops,r->bw_gbs,r->lat_ns,r->hash_gbs,r->proc_cpu_pct,r->sys_busy_pct,r->score);
  fclose(f);
}

/* =========================== CLI =========================== */
typedef struct {
  size_t size_mb;
  double sec;
  int rep;
  int quick;
  int no_clear;
  int no_color;
  int pin_cpu; /* -1 none */
  int nice_set;
  int nicev;
  const char *json;
  const char *csv;
} Cfg;

static void usage(const char *a0){
  printf("Usage: %s [flags]\n", a0);
  puts("Flags:");
  puts("  --quick              preset celular (64MB, 1s, rep=1)");
  puts("  --size-mb N          bloco (MiB). default 64 (celular-friendly)");
  puts("  --seconds S          por teste. default 1.5");
  puts("  --repeat N           repetições. default 3");
  puts("  --no-clear           não limpar tela");
  puts("  --no-color           sem ANSI");
  puts("  --pin-cpu N          fixa em core N (best effort)");
  puts("  --nice N             set nice (prioridade) (best effort)");
  puts("  --json path          exporta JSON");
  puts("  --csv  path          exporta CSV");
  puts("  --help               ajuda");
}

static int streq(const char *a,const char *b){ return strcmp(a,b)==0; }

static void parse(Cfg *c,int ac,char **av){
  c->size_mb=64;
  c->sec=1.5;
  c->rep=3;
  c->quick=0;
  c->no_clear=0;
  c->no_color=0;
  c->pin_cpu=-1;
  c->nice_set=0;
  c->nicev=0;
  c->json=NULL;
  c->csv=NULL;

  for(int i=1;i<ac;i++){
    const char *x=av[i];
    if(streq(x,"--help")){ usage(av[0]); exit(0); }
    else if(streq(x,"--quick")) c->quick=1;
    else if(streq(x,"--size-mb") && i+1<ac) c->size_mb=(size_t)strtoull(av[++i],NULL,10);
    else if(streq(x,"--seconds") && i+1<ac) c->sec=strtod(av[++i],NULL);
    else if(streq(x,"--repeat") && i+1<ac) c->rep=atoi(av[++i]);
    else if(streq(x,"--no-clear")) c->no_clear=1;
    else if(streq(x,"--no-color")) c->no_color=1;
    else if(streq(x,"--pin-cpu") && i+1<ac) c->pin_cpu=atoi(av[++i]);
    else if(streq(x,"--nice") && i+1<ac){ c->nice_set=1; c->nicev=atoi(av[++i]); }
    else if(streq(x,"--json") && i+1<ac) c->json=av[++i];
    else if(streq(x,"--csv")  && i+1<ac) c->csv=av[++i];
    else { fprintf(stderr,"Unknown: %s\n",x); usage(av[0]); exit(1); }
  }

  if(c->quick){
    c->size_mb=64;
    c->sec=1.0;
    c->rep=1;
  }
  if(c->size_mb<8) c->size_mb=8;
  if(c->sec<0.25) c->sec=0.25;
  if(c->rep<1) c->rep=1;
}

static void maybe_aff(int cpu){
#if defined(__linux__) || defined(__ANDROID__)
  if(cpu<0) return;
  cpu_set_t set;
  CPU_ZERO(&set);
  if(cpu>=CPU_SETSIZE){ fprintf(stderr,"[warn] invalid cpu=%d\n",cpu); return; }
  CPU_SET(cpu,&set);
  if(sched_setaffinity(0,sizeof(set),&set)!=0)
    fprintf(stderr,"[warn] sched_setaffinity: %s\n",strerror(errno));
#endif
}

static void maybe_nice(int on,int val){
#if defined(__linux__) || defined(__ANDROID__)
  if(!on) return;
  if(setpriority(PRIO_PROCESS,0,val)!=0)
    fprintf(stderr,"[warn] setpriority(nice=%d): %s\n",val,strerror(errno));
#endif
}

int main(int ac,char **av){
  Cfg c; parse(&c,ac,av);
  g_clear = !c.no_clear;
  g_color = !c.no_color;

  maybe_aff(c.pin_cpu);
  maybe_nice(c.nice_set,c.nicev);

  term_clear();
  bbs_banner();

  crc32_init();

  sys_ctx sx; sysctx_collect(&sx);
  printf("[sys] cpu   : %s\n", sx.cpu);
  if(sx.freq_khz[0]) printf("[sys] freq  : %s kHz (best-effort)\n", sx.freq_khz);
  if(sx.mem_total[0]) printf("[sys] mem   : total=%s | avail=%s\n", sx.mem_total, sx.mem_avail[0]?sx.mem_avail:"?");
  if(sx.la1>=0) printf("[sys] load  : %.2f %.2f %.2f\n", sx.la1,sx.la5,sx.la15);
  printf("[sys] nice  : %d\n", sx.nicev);
  if(sx.pinned>=0) printf("[sys] aff   : pinned cpu=%d\n", sx.pinned);

  size_t bytes = (size_t)c.size_mb * 1024u * 1024u;
  bytes = next_pow2(bytes);          /* geometria do cache: pow2 */
  if(bytes < (8u*1024u*1024u)) bytes = 8u*1024u*1024u;

  uint8_t  *a = (uint8_t*)xmalloc_aligned(64, bytes);
  uint8_t  *b = (uint8_t*)xmalloc_aligned(64, bytes);

  size_t n_u32 = bytes / 4u;
  if(n_u32 < 1024) n_u32 = 1024;
  uint32_t *idx = (uint32_t*)xmalloc_aligned(64, n_u32 * sizeof(uint32_t));

  uint64_t seed = 0xA11CE5EED1234ull;
  make_cycle(idx, n_u32, &seed);

  cpu_stat s0,s1;
  (void)read_proc_stat(&s0);

  uint64_t w0 = ns_now();
  uint64_t p0 = ns_proc_cpu();

  R r;
  memset(&r,0,sizeof(r));

  /* repeat: média simples (você pode trocar por mediana depois) */
  for(int k=0;k<c.rep;k++){
    r.i_mops   += bench_int(c.sec);
    r.f_mflops += bench_fp(c.sec);
    r.bw_gbs   += bench_mem_bw(a,b,bytes,c.sec);
    r.lat_ns   += bench_mem_rand(idx,n_u32,c.sec);
    r.hash_gbs += bench_hash_crc(a,bytes,c.sec);
  }
  r.i_mops   /= (double)c.rep;
  r.f_mflops /= (double)c.rep;
  r.bw_gbs   /= (double)c.rep;
  r.lat_ns   /= (double)c.rep;
  r.hash_gbs /= (double)c.rep;

  uint64_t p1 = ns_proc_cpu();
  uint64_t w1 = ns_now();

  double wall = ns_to_s(w1-w0);
  double proc = ns_to_s(p1-p0);
  r.proc_cpu_pct = (wall>0)? (proc*100.0/wall) : -1.0;

  (void)read_proc_stat(&s1);
  r.sys_busy_pct = cpu_busy_pct(&s0,&s1);

  r.score = score_comp(&r);

  const char *g = g_color ? "\033[1;32m" : "";
  const char *y = g_color ? "\033[1;33m" : "";
  const char *rC= g_color ? "\033[1;31m" : "";
  const char *z = g_color ? "\033[0m" : "";

  puts("------------------------------------------------------------");
  printf("%s[bench]%s CPU_INT   : %8.2f Mops/s\n", g,z,r.i_mops);
  printf("%s[bench]%s CPU_FP    : %8.2f Mflop/s\n", g,z,r.f_mflops);
  printf("%s[bench]%s MEM_BW    : %8.2f GB/s   (seq memcpy)\n", g,z,r.bw_gbs);
  printf("%s[bench]%s MEM_RAND  : %8.2f ns/step (pointer-chase)\n", g,z,r.lat_ns);
  printf("%s[bench]%s HASH+CRC  : %8.2f GB/s\n", g,z,r.hash_gbs);
  puts("------------------------------------------------------------");
  printf("%s[tele]%s proc_cpu%% : %6.2f%% | sys_busy%% : %s%6.2f%%%s\n",
         y,z,r.proc_cpu_pct,(r.sys_busy_pct<0? rC:y),(r.sys_busy_pct<0? 0.0:r.sys_busy_pct),z);
  printf("%s[score]%s  %.2f   (composto; não “verdade absoluta”)\n", y,z,r.score);
  puts("------------------------------------------------------------");
  printf("[note] bytes(pow2)=%zu | seconds=%.2f | repeat=%d\n", bytes,c.sec,c.rep);
  puts("[note] determinismo: seed fixa + ciclo fechado; variação vem do mundo real (load/thermal).");
  puts("[note] geometria: 'vetor' aqui é fluxo seq + bússola rand (latência).");

  if(c.json){ write_json(c.json,&sx,&r,bytes,c.sec,c.rep); printf("[out] json : %s\n", c.json); }
  if(c.csv ){ write_csv (c.csv ,&sx,&r,bytes,c.sec,c.rep); printf("[out] csv  : %s\n", c.csv ); }

  free(a); free(b); free(idx);
  return 0;
}
C_EOF

# ───────────────────────────────────────────────────────────────
# bin/catos  (ROOT auto by path; no env required)
# ───────────────────────────────────────────────────────────────
cat > "$ROOT/bin/catos" <<'SH_EOF'
#!/data/data/com.termux/files/usr/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
SRC="$ROOT/src/raf_bench_bbs.c"
OUT="$ROOT/out/raf_bench"

cmd="${1:-quick}"
shift || true

build(){
  clang -O3 -DNDEBUG "$SRC" -o "$OUT"
}

case "$cmd" in
  build) build ;;
  quick)
    build
    exec "$OUT" --quick "$@"
    ;;
  run)
    build
    exec "$OUT" "$@"
    ;;
  json)
    build
    exec "$OUT" --quick --json "$ROOT/out/bench.json" "$@"
    ;;
  csv)
    build
    exec "$OUT" --quick --csv "$ROOT/out/bench.csv" "$@"
    ;;
  *)
    build
    exec "$OUT" "$cmd" "$@"
    ;;
esac
SH_EOF
chmod +x "$ROOT/bin/catos"

# ───────────────────────────────────────────────────────────────
# docs/README
# ───────────────────────────────────────────────────────────────
cat > "$ROOT/docs/README.txt" <<'R_EOF'
CatOS_RAFAELIA — Bench BBS/CLI (Termux)

Run:
  ./bin/catos quick
  ./bin/catos run --size-mb 128 --seconds 1.5 --repeat 3
  ./bin/catos json
  ./bin/catos csv

Flags:
  --no-clear  (não limpa tela)
  --no-color  (sem ANSI)
  --pin-cpu N (fixa core)  [best effort]
  --nice N    (prioridade) [best effort]

Patch/Replace (sem “caça no celular”):
  # exemplo: trocar default size 64 -> 128
  sed -i 's/c->size_mb=64;/c->size_mb=128;/' src/raf_bench_bbs.c
R_EOF

echo "✅ CatOS pronto em: $ROOT"
echo "▶ rodar (quick): $ROOT/bin/catos quick"
echo "▶ rodar (full) : $ROOT/bin/catos run --size-mb 128 --seconds 1.5 --repeat 3 --json $ROOT/out/bench.json --csv $ROOT/out/bench.csv"
