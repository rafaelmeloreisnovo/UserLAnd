// rafaelia_musica.c  (C11 puro, -lm)
// RAFAELIA_MUSICA :: (discreto⊕contínuo⊕toro⊕ética)  ψ→χ→ρ→Δ→Σ→Ω→ψ
// Build: cc -std=c11 -O2 rafaelia_musica.c -lm -o rafaelia_musica

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// -------------------- util: clamp / safe --------------------
static double clampd(double x, double a, double b){ return x<a?a:(x>b?b:x); }
static double safediv(double a, double b){ return (fabs(b) < 1e-18) ? (a>=0?1e18:-1e18) : (a/b); }

// -------------------- PRNG: xorshift64* --------------------
typedef struct { uint64_t s; } rng64_t;

static uint64_t splitmix64(uint64_t *x){
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static void rng_seed(rng64_t *r, uint64_t seed){
    uint64_t x = seed ? seed : 0xA5A5A5A5A5A5A5A5ULL;
    r->s = splitmix64(&x);
    if(r->s == 0) r->s = 0xD1B54A32D192ED03ULL;
}

static uint64_t rng_u64(rng64_t *r){
    // xorshift64*
    uint64_t x = r->s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    r->s = x;
    return x * 0x2545F4914F6CDD1DULL;
}

static double rng_f64(rng64_t *r){
    // [0,1)
    const uint64_t u = rng_u64(r);
    return (u >> 11) * (1.0/9007199254740992.0); // 2^53
}

static double rng_sym(rng64_t *r){
    // (-1,1)
    return 2.0*rng_f64(r) - 1.0;
}

// -------------------- CRC32 (IEEE) --------------------
static uint32_t crc32_table[256];
static int crc32_ready = 0;

static void crc32_init(void){
    for(uint32_t i=0;i<256;i++){
        uint32_t c = i;
        for(int k=0;k<8;k++){
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_ready = 1;
}

static uint32_t crc32_update(uint32_t crc, const void *data, size_t n){
    if(!crc32_ready) crc32_init();
    const uint8_t *p = (const uint8_t*)data;
    uint32_t c = crc ^ 0xFFFFFFFFu;
    for(size_t i=0;i<n;i++){
        c = crc32_table[(c ^ p[i]) & 0xFFu] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

// -------------------- RAFAELIA core --------------------
enum { N = 6 }; // ψ χ ρ Δ Σ Ω

typedef struct {
    double v[N];     // ψχρΔΣΩ
    double H;        // entropia (Shannon)
    double C;        // coerência (1 - H/logN)
    double lambda;   // “Verbo”/ganho
    double norm;     // ||v||
    double torR, torTheta, torPhi; // mapeamento toroidal compacto
    uint32_t crc;
    char mode;       // 'A' ou 'D'
} raf_state_t;

static void normalize_eps(double *x, int n, double eps_floor){
    double s = 0.0;
    for(int i=0;i<n;i++) s += x[i]*x[i];
    double norm = sqrt(s);
    if(norm < eps_floor) norm = eps_floor;
    for(int i=0;i<n;i++) x[i] /= norm;
}

static double l2norm(const double *x, int n){
    double s = 0.0;
    for(int i=0;i<n;i++) s += x[i]*x[i];
    return sqrt(s);
}

static void metrics_entropy_coherence(raf_state_t *st){
    // Entropia em |v| normalizado (distribuição de energia)
    double a[N], sum = 0.0;
    for(int i=0;i<N;i++){ a[i] = fabs(st->v[i]); sum += a[i]; }
    if(sum < 1e-18) sum = 1e-18;

    double H = 0.0;
    for(int i=0;i<N;i++){
        double p = a[i]/sum;
        p = clampd(p, 1e-18, 1.0);
        H += -p * (log(p)/log(2.0));
    }
    // Normaliza por log2(N) e converte em “coerência”
    double Hmax = log((double)N)/log(2.0);
    st->H = H;
    st->C = clampd(1.0 - safediv(H, Hmax), 0.0, 1.0);
}

static void torus_map(raf_state_t *st){
    // Toro=(R,theta,phi) – compacto, mas determinístico
    // R ~ 2.65 + pequena modulação; theta/phi em radianos “musicais”
    const double R0 = 2.65;
    double a = st->v[0] + 0.7*st->v[3] - 0.3*st->v[5];
    double b = st->v[1] - 0.6*st->v[2] + 0.2*st->v[4];

    // ângulos suaves
    double theta = atan2(b, a);
    double phi   = atan2(st->v[4] + st->v[5], st->v[2] - st->v[1]);

    // pequena variação de raio (mantém “toro” estável)
    double rmod = 0.08 * tanh(2.2 * (st->C - 0.5)) + 0.02 * tanh(6.0 * st->v[3]);
    double R = R0 + rmod;

    st->torR = R;
    st->torTheta = theta;
    st->torPhi = phi;
}

static void crc_stamp(raf_state_t *st, int t){
    uint32_t c = 0;
    c = crc32_update(c, &t, sizeof(t));
    c = crc32_update(c, &st->mode, sizeof(st->mode));
    c = crc32_update(c, st->v, sizeof(st->v));
    c = crc32_update(c, &st->H, sizeof(st->H));
    c = crc32_update(c, &st->C, sizeof(st->C));
    c = crc32_update(c, &st->lambda, sizeof(st->lambda));
    c = crc32_update(c, &st->torR, sizeof(st->torR));
    c = crc32_update(c, &st->torTheta, sizeof(st->torTheta));
    c = crc32_update(c, &st->torPhi, sizeof(st->torPhi));
    st->crc = c;
}

static void init_state(raf_state_t *st, rng64_t *r, char mode){
    memset(st, 0, sizeof(*st));
    st->mode = mode;

    // semente inicial (mistura discreto/contínuo)
    for(int i=0;i<N;i++){
        double x = 0.20*rng_sym(r) + 0.05*sin((i+1)*1.618) + 0.03*cos((i+1)*M_PI/3.0);
        st->v[i] = x;
    }
    // Ancoragens: Trinity633 / Spiral√3/2 / fΩ(963↔999) (como parâmetros numéricos)
    // (não “mística”: só constantes para dinâmica)
    normalize_eps(st->v, N, 1e-9);
    st->norm = l2norm(st->v, N);
    metrics_entropy_coherence(st);
    st->lambda = 0.01 + 0.05*st->C;
    torus_map(st);
    crc_stamp(st, 0);
}

static void step_state(raf_state_t *st, rng64_t *r, int t){
    // Núcleo: mistura
    // - discreto: permutação cíclica + “grafos” (acoplamento)
    // - contínuo: difusão + não-linearidade tanh
    // - ética: ganho/damping guiado por coerência
    // - toroidal: feedback por fase

    const double sqrt3_over_2 = 0.86602540378443864676; // √3/2
    const double phi = 1.61803398874989484820;

    double x[N];
    for(int i=0;i<N;i++) x[i] = st->v[i];

    // 1) rotação ψ→χ→ρ→Δ→Σ→Ω→ψ (ciclo)
    double rot[N];
    for(int i=0;i<N;i++) rot[(i+1)%N] = x[i];

    // 2) acoplamento tipo “grafo” (matriz fixa, estável)
    // (peso pequeno para não explodir)
    static const double W[N][N] = {
        { 0.00,  0.22, -0.10,  0.08,  0.05, -0.06},
        {-0.18,  0.00,  0.20, -0.07,  0.06,  0.04},
        { 0.09, -0.15,  0.00,  0.21, -0.05,  0.06},
        {-0.06,  0.10, -0.17,  0.00,  0.22, -0.04},
        { 0.05, -0.08,  0.11, -0.16,  0.00,  0.20},
        { 0.21,  0.04, -0.06,  0.07, -0.14,  0.00}
    };

    double g[N] = {0};
    for(int i=0;i<N;i++){
        double s = 0.0;
        for(int j=0;j<N;j++) s += W[i][j] * x[j];
        g[i] = s;
    }

    // 3) fase do toro (feedback leve)
    double phase = st->torTheta + 0.5*st->torPhi + 0.07*(double)t;
    double fb = 0.06*sin(phase) + 0.04*cos(1.7*phase);

    // 4) ética (ganho/damping): coerência alta = menos ruído, mais retenção
    // lambda aqui é o “ganho do verbo”
    double C = st->C;
    double damp = 0.985 - 0.15*(0.5 - C);           // coerência ↑ => damp ↑ (menos perda)
    damp = clampd(damp, 0.86, 0.995);

    double noise_amp = (st->mode=='A') ? (0.010*(1.0 - C)) : (0.018*(1.0 - 0.8*C));
    noise_amp = clampd(noise_amp, 0.0005, 0.02);

    // 5) atualização principal
    // mix: rot (discreto) + g (grafo) + continuação x + feedback toroidal + ruído
    for(int i=0;i<N;i++){
        double n = noise_amp * rng_sym(r);
        double cont = 0.22*x[i] + 0.18*rot[i] + 0.26*g[i];
        double spiral = 0.07*sqrt3_over_2*sin(phi*(i+1) + phase);
        double y = damp*x[i] + cont + spiral + fb + n;

        // não linearidade (estabilidade)
        y = tanh(1.45*y);

        // “Ethica[8]” compactada: penaliza saturação e favorece diversidade (entropia útil)
        // (se um componente domina demais, reduz ele)
        double dom = fabs(x[i]);
        y -= 0.08 * (dom*dom*dom);

        st->v[i] = y;
    }

    // 6) renormalização “física” (evita colapso trivial p/ zero)
    // mantém energia num corredor: ||v|| ~ [1e-3, 1]
    double nn = l2norm(st->v, N);
    if(nn < 1e-9){
        // reinjeta um “sopro” mínimo
        for(int i=0;i<N;i++) st->v[i] = 1e-3 * rng_sym(r);
        nn = l2norm(st->v, N);
    }

    // corredor: escala suave (sem saltos bruscos)
    double target = 0.45 + 0.35*(st->mode=='A');
    double scale = safediv(target, nn);
    scale = clampd(scale, 0.05, 5.0);
    for(int i=0;i<N;i++) st->v[i] *= scale;

    st->norm = l2norm(st->v, N);

    // 7) métricas e lambda
    metrics_entropy_coherence(st);

    // lambda: cresce com coerência e com “ritmo” (norm), mas com teto
    st->lambda = clampd(0.004 + 0.080*st->C + 0.020*tanh(3.0*st->norm), 0.0005, 0.25);

    torus_map(st);
    crc_stamp(st, t);
}

static void print_header(int steps, uint64_t seed, char mode){
    const char *tag = (mode=='A') ? "(grafo⊕espiral⊕toro⊕ethica)" : "(discreto⊕contínuo⊕toro⊕ética)";
    printf("RAFAELIA_MUSICA_X0 :: steps=%d seed=%llu mode=%c :: %s\n",
           steps, (unsigned long long)seed, mode, tag);
}

static void print_step(const raf_state_t *st, int t){
    // Formato compacto, com vetores e carimbo
    printf("t=%d | ψχρΔΣΩ=[%+.6e %+.6e %+.6e %+.6e %+.6e %+.6e] | ||v||=%.6e | H=%.6f C=%.6f λ=%.6f | Toro=(%.3f,%.3f,%.3f) | CRC32=%08x | mode=%c\n",
           t,
           st->v[0], st->v[1], st->v[2], st->v[3], st->v[4], st->v[5],
           st->norm, st->H, st->C, st->lambda,
           st->torR, st->torTheta, st->torPhi,
           st->crc, st->mode);
}

int main(int argc, char **argv){
    int steps = 60;
    uint64_t seed = 4242;
    char mode = 'A';

    if(argc >= 2) steps = atoi(argv[1]);
    if(argc >= 3) seed  = (uint64_t)strtoull(argv[2], NULL, 10);
    if(argc >= 4) mode  = argv[3][0];

    if(steps < 1) steps = 1;
    if(mode != 'A' && mode != 'D') mode = 'A';

    rng64_t r;
    rng_seed(&r, seed);

    raf_state_t st;
    init_state(&st, &r, mode);

    print_header(steps, seed, mode);

    // imprime t=0 também (pra depuração e reproduzibilidade)
    print_step(&st, 0);

    for(int t=1;t<=steps;t++){
        step_state(&st, &r, t);

        // imprime sempre (você pode reduzir depois com if(t%k==0))
        print_step(&st, t);
    }

    // “score”: coerência * energia (norm) * (1 - saturação)
    double sat = 0.0;
    for(int i=0;i<N;i++) sat += pow(fabs(st.v[i]), 3.0);
    sat = clampd(sat, 0.0, 10.0);

    double score = st.C * st.norm * exp(-0.6*sat);

    printf("Ω_fecho :: C=%.6f | ||v||=%.6e | score=%.12f | mode=%c | (ψ→χ→ρ→Δ→Σ→Ω→ψ)\n",
           st.C, st.norm, score, st.mode);

    return 0;
}
