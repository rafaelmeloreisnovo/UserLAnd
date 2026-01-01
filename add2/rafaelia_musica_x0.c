/*
 * RAFAELIA_MUSICA_X0 (C11) — música matemática viva (grafo⊕espiral⊕toro⊕ethica)
 *
 * build:
 *   cc -std=c11 -O2 rafaelia_musica_x0.c -lm -o rafaelia_musica_x0
 *
 * run:
 *   ./rafaelia_musica_x0 200 4242 A
 *   ./rafaelia_musica_x0 200 4242 B
 *
 * opcional (stdin = "verbo" externo; 1 linha por step):
 *   printf "0.2\n-0.1\n0.05\n" | ./rafaelia_musica_x0 120 4242 A
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const double SQRT3_DIV_2 = 0.86602540378443864676; /* √3/2 */
static const double PHI         = 1.61803398874989484820; /* φ */
static const double EPS         = 1e-12;

typedef struct {
  double v[6];     /* ψ χ ρ Δ Σ Ω */
  double angle;    /* fase */
  double mem_u;    /* toro u */
  double mem_w;    /* toro w */
  uint64_t step;
  uint64_t rng;
  char mode;       /* 'A' ciclo estável | 'B' caos controlado */
} Rafaelia;

/* xorshift64* */
static uint64_t xs64(uint64_t *s){
  uint64_t x = *s;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  *s = x;
  return x * 2685821657736338717ULL;
}
static double urand01(uint64_t *s){
  return (xs64(s) >> 11) * (1.0/9007199254740992.0);
}

/* CRC32 */
static uint32_t crc32_u8(const uint8_t *data, size_t n){
  uint32_t crc = 0xFFFFFFFFu;
  for(size_t i=0;i<n;i++){
    crc ^= (uint32_t)data[i];
    for(int k=0;k<8;k++){
      uint32_t m = -(crc & 1u);
      crc = (crc >> 1) ^ (0xEDB88320u & m);
    }
  }
  return ~crc;
}

static void probs_from_vec(const double v[6], double p[6]){
  double s = 0.0;
  for(int i=0;i<6;i++){ p[i] = fabs(v[i]); s += p[i]; }
  if(s < EPS){
    for(int i=0;i<6;i++) p[i] = 1.0/6.0;
  }else{
    for(int i=0;i<6;i++) p[i] /= s;
  }
}

static double shannon_entropy(const double p[6]){
  double H = 0.0;
  for(int i=0;i<6;i++) if(p[i] > EPS) H -= p[i]*log(p[i]);
  return H;
}

static double coherence_from_entropy(double H){
  const double Hmax = log(6.0);
  double c = 1.0 - (H/(Hmax+EPS));
  if(c < 0.0) c = 0.0;
  if(c > 1.0) c = 1.0;
  return c;
}

static double l2_norm6(const double v[6]){
  double s=0.0;
  for(int i=0;i<6;i++) s += v[i]*v[i];
  return sqrt(s);
}

/* Ethica[8] compacta: 8 sinais -> fator 0..1 */
static double ethica8_factor(const Rafaelia *R){
  double p[6]; probs_from_vec(R->v, p);
  double H  = shannon_entropy(p);
  double C  = coherence_from_entropy(H);

  double e1 = C;                                     /* coerência */
  double e2 = 1.0 - fmin(1.0, fabs(R->v[2]));        /* ρ baixo */
  double e3 = 1.0 - fmin(1.0, fabs(R->v[3]));        /* Δ não explode */
  double e4 = fmin(1.0, fabs(R->v[4]));              /* Σ presente */
  double e5 = fmin(1.0, fabs(R->v[5]));              /* Ω presente */
  double e6 = 1.0 - fmin(1.0, fabs(R->v[0]-R->v[1]));/* ψ≈χ */
  double e7 = 1.0 - fmin(1.0, fabs(R->v[1]-R->v[4]));/* χ≈Σ */
  double e8 = 1.0 - fmin(1.0, fabs(R->v[0]-R->v[5]));/* ψ≈Ω */

  double prod = 1.0;
  double e[8] = {e1,e2,e3,e4,e5,e6,e7,e8};
  for(int i=0;i<8;i++){
    double z = e[i];
    if(z < 0.0) z = 0.0;
    if(z > 1.0) z = 1.0;
    prod *= (z + 1e-9);
  }
  return pow(prod, 1.0/8.0);
}

/* grafo: Laplaciano discreto (menos dissipativo) */
static void graph_step(const Rafaelia *R, double out[6]){
  static const double W[6][6] = {
    {0, .30, 0,  0, .15, .25}, /* ψ */
    {.30,0, .30, 0,  0, .15},  /* χ */
    {0, .30,0, .30, 0,  0},    /* ρ */
    {0,  0, .30,0, .30, 0},    /* Δ */
    {.15,0,  0, .30,0, .30},   /* Σ */
    {.25,.15,0,  0, .30,0}     /* Ω */
  };

  double x[6]; memcpy(x, R->v, sizeof(x));
  double Wx[6]={0}, Dx[6]={0};
  for(int i=0;i<6;i++){
    double di=0.0;
    for(int j=0;j<6;j++){ Wx[i] += W[i][j]*x[j]; di += W[i][j]; }
    Dx[i] = di * x[i];
  }

  const double alpha = 0.28;
  for(int i=0;i<6;i++){
    out[i] = x[i] + alpha*(Wx[i] - Dx[i]);
  }
}

/* filtro ético: contrai só o que precisa, sem matar amplitude */
static double ethics_filter(Rafaelia *R){
  double p[6]; probs_from_vec(R->v, p);
  double H  = shannon_entropy(p);
  double C  = coherence_from_entropy(H);
  double E  = ethica8_factor(R);
  double lambda = SQRT3_DIV_2 * C * E;

  /* contrai ruído e transdução quando λ baixo */
  double k = 0.60 + 0.40*lambda;      /* 0.60..1.00 */
  R->v[2] *= k;                       /* ρ */
  R->v[3] *= (0.75 + 0.25*lambda);    /* Δ */

  /* preserva memória/fecho */
  R->v[4] *= (0.92 + 0.08*lambda);    /* Σ */
  R->v[5] *= (0.92 + 0.08*lambda);    /* Ω */

  /* saturação suave só nos canais “perigosos” */
  R->v[2] = tanh(R->v[2]);
  R->v[3] = tanh(R->v[3]);
  return lambda;
}

/* rotação contínua (espiral) */
static void spiral_step(Rafaelia *R, double lambda){
  R->angle += (0.10 + 0.22*lambda) * PHI;

  double a = R->angle;
  double c = cos(a), s = sin(a);
  double psi = R->v[0], chi = R->v[1];
  R->v[0] =  c*psi - s*chi;
  R->v[1] =  s*psi + c*chi;

  double b = -a * SQRT3_DIV_2;
  double cb = cos(b), sb = sin(b);
  double Sig = R->v[4], Om = R->v[5];
  R->v[4] = cb*Sig - sb*Om;
  R->v[5] = sb*Sig + cb*Om;
}

/* toro: memória como ciclo fechado (ganho adaptativo pra não "travar") */
static void torus_embed(Rafaelia *R, double *X, double *Y, double *Z){
  double s1 = R->v[0] + R->v[3] + 0.5*R->v[5];
  double s2 = R->v[1] + R->v[2] + 0.5*R->v[4];

  double norm = l2_norm6(R->v);
  double g = 0.6 / (1e-3 + norm); /* ganho adaptativo */

  R->mem_u += g * 0.20 * tanh(s1);
  R->mem_w += g * 0.17 * tanh(s2);

  /* wrap */
  if(R->mem_u >= 2*M_PI) R->mem_u -= 2*M_PI;
  if(R->mem_u <  0)      R->mem_u += 2*M_PI;
  if(R->mem_w >= 2*M_PI) R->mem_w -= 2*M_PI;
  if(R->mem_w <  0)      R->mem_w += 2*M_PI;

  double Rmaj = 2.0, rmin = 0.65;
  *X = (Rmaj + rmin*cos(R->mem_w)) * cos(R->mem_u);
  *Y = (Rmaj + rmin*cos(R->mem_w)) * sin(R->mem_u);
  *Z = rmin * sin(R->mem_w);
}

/* verbo externo */
static void apply_verbo(Rafaelia *R, double verbo){
  R->v[0] += verbo;         /* ψ */
  R->v[3] += 0.33*verbo;    /* Δ */
  R->v[4] += 0.21*verbo;    /* Σ */
}

/* micro-injeção interna: mantém música viva */
static void micro_drive(Rafaelia *R){
  if(R->mode == 'A'){
    /* A: limit-cycle (suave, musical) */
    R->v[0] += 0.0020 * sin(R->angle + 0.37);
    R->v[1] += 0.0014 * sin(R->angle * SQRT3_DIV_2);
    R->v[3] += 0.0012 * cos(R->angle * PHI);
  }else{
    /* B: caos controlado (kick não linear, mas pequeno) */
    double u = urand01(&R->rng);              /* [0,1) */
    double x = (R->v[0] + 1.0) * 0.5;         /* [-1,1] -> [0,1] */
    if(x < 0.0) x = 0.0;
    if(x > 1.0) x = 1.0;
    x = 3.78 * x * (1.0 - x);                 /* logistic */
    double kick = (x - 0.5) * 0.006 + (u - 0.5) * 0.002;
    R->v[0] += kick;
    R->v[2] += 0.5*kick;   /* ρ acompanha */
    R->v[3] += 0.3*kick;
  }
}

/* um ciclo */
static void step_cycle(Rafaelia *R){
  /* verbo externo (opcional): 1 linha por step */
  double verbo = 0.0;
  int got = 0;
  char buf[128];
  if(fgets(buf, sizeof(buf), stdin)){
    char *end = NULL;
    double x = strtod(buf, &end);
    if(end != buf){ verbo = x; got = 1; }
  }
  if(got) apply_verbo(R, verbo);

  /* drive interno (não deixa morrer) */
  micro_drive(R);

  /* 1) discreto (grafo) */
  double nxt[6];
  graph_step(R, nxt);
  memcpy(R->v, nxt, sizeof(nxt));

  /* 2) ética (retorna λ) */
  double lambda = ethics_filter(R);

  /* 3) contínuo (espiral) */
  spiral_step(R, lambda);

  /* 4) fecho Ω (circuito) */
  R->v[5] += 0.07 * tanh(R->v[4]); /* Ω <- Σ */
  R->v[4] += 0.05 * tanh(R->v[0]); /* Σ <- ψ */
  R->v[0] += 0.03 * tanh(R->v[5]); /* ψ <- Ω */

  /* saturação final mínima (proteção contra overflow) */
  for(int i=0;i<6;i++){
    if(fabs(R->v[i]) > 3.0) R->v[i] = tanh(R->v[i]);
  }

  R->step++;
}

static void print_state(const Rafaelia *R){
  double p[6]; probs_from_vec(R->v, p);
  double H  = shannon_entropy(p);
  double C  = coherence_from_entropy(H);
  double E  = ethica8_factor(R);
  double lambda = SQRT3_DIV_2 * C * E;
  double norm = l2_norm6(R->v);

  /* calcula toro usando o estado real (não “tmp congelado”) */
  Rafaelia tmp = *R;
  double X,Y,Z;
  torus_embed(&tmp, &X, &Y, &Z);

  /* pack para CRC */
  struct {
    double v[6];
    double angle, mem_u, mem_w;
    uint64_t step;
    char mode;
  } pack;
  memcpy(pack.v, R->v, sizeof(pack.v));
  pack.angle = R->angle;
  pack.mem_u = R->mem_u;
  pack.mem_w = R->mem_w;
  pack.step  = R->step;
  pack.mode  = R->mode;

  uint32_t crc = crc32_u8((const uint8_t*)&pack, sizeof(pack));

  printf("t=%llu | ψχρΔΣΩ=[%+.6e %+.6e %+.6e %+.6e %+.6e %+.6e] | ||v||=%.6e | H=%.6f C=%.6f λ=%.6f | Toro=(%+.6e,%+.6e,%+.6e) | CRC32=%08x | mode=%c\n",
    (unsigned long long)R->step,
    R->v[0],R->v[1],R->v[2],R->v[3],R->v[4],R->v[5],
    norm,
    H, C, lambda,
    X,Y,Z,
    crc,
    R->mode
  );
}

int main(int argc, char **argv){
  int steps = 200;
  uint64_t seed = 1337;
  char mode = 'A';

  if(argc >= 2) steps = atoi(argv[1]);
  if(argc >= 3) seed  = (uint64_t)strtoull(argv[2], NULL, 10);
  if(argc >= 4) mode  = argv[3][0];

  if(mode != 'A' && mode != 'B') mode = 'A';
  if(steps < 1) steps = 1;
  if(steps > 200000) steps = 200000;

  Rafaelia R;
  memset(&R, 0, sizeof(R));
  R.mode = mode;
  R.rng  = seed ^ 0x9E3779B97F4A7C15ULL;

  /* init pequeno, mas não trivial */
  uint64_t s = R.rng;
  for(int i=0;i<6;i++){
    double r = urand01(&s)*2.0 - 1.0;
    R.v[i] = 0.08 * r;
  }
  R.v[0] += 0.10; /* ψ */
  R.v[4] += 0.07; /* Σ */
  R.v[5] += 0.05; /* Ω */

  printf("RAFAELIA_MUSICA_X0 :: steps=%d seed=%llu mode=%c :: (grafo⊕espiral⊕toro⊕ethica)\n",
         steps, (unsigned long long)seed, mode);

  for(int k=0;k<steps;k++){
    step_cycle(&R);
    print_state(&R);
  }

  /* fecho */
  double p[6]; probs_from_vec(R.v, p);
  double H  = shannon_entropy(p);
  double C  = coherence_from_entropy(H);
  double norm = l2_norm6(R.v);

  double prod = 1.0;
  for(int i=0;i<6;i++) prod *= (fabs(R.v[i]) + 1e-9);
  double score = C * log(1.0 + prod) * (1.0 + 0.25*norm);

  printf("Ω_fecho :: C=%.6f | ||v||=%.6e | score=%.12f | mode=%c | (ψ→χ→ρ→Δ→Σ→Ω→ψ)\n", C, norm, score, mode);
  return 0;
}
