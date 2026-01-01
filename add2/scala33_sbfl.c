/*
 * SCALA 33 — KERNEL SBFL (METAL FRIO / ANSI C)
 * ==========================================================
 * Objetivo (CIÊNCIA APLICADA):
 * - Baixar METAR real (SBFL) e calcular:
 *   1) Pressão de vapor real via ponto de orvalho (Magnus)
 *   2) Densidade do ar úmido (mistura de gases ideais)
 *   3) Densidade-altitude (aproximação operacional)
 *   4) Vstall e margem (V / Vstall)
 * - Exibir em dashboard ASCII (BBS style)
 *
 * Notas:
 * - Depende de: gcc, wget
 * - Fonte METAR (pública): NOAA TGFTP
 * - Não usa "variáveis mágicas" (radon/aurora/etc) — só dados meteo e física básica.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

/* ---------- Constantes físicas ---------- */
#define R_DRY      287.058   /* J/(kg·K) */
#define R_VAPOR    461.495   /* J/(kg·K) */
#define KELVIN     273.15
#define P0_PA      101325.0  /* Pa */
#define T0_K       288.15    /* K (15°C) */
#define LAPSE_KPM  0.0065    /* K/m */

/* ---------- Aeronave (exemplo) ---------- */
#define AC_WEIGHT_N  12000.0 /* N */
#define AC_WING_S    16.2    /* m^2 */
#define AC_CL_MAX    1.55
#define AC_SPEED_MS  65.0    /* m/s */

/* ---------- Config ---------- */
#define UPDATE_SECONDS 60
#define METAR_URL "https://tgftp.nws.noaa.gov/data/observations/metar/stations/SBFL.TXT"
#define METAR_FILE "sbfl.txt"

/* ---------- Util ---------- */
static double clamp(double x, double lo, double hi) {
    if (!isfinite(x)) return lo;
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/* Magnus-Tetens: pressão de vapor de saturação sobre água líquida (hPa) */
static double es_hPa_magnus(double t_c) {
    /* Faixa operacional boa para meteo de aviação */
    return 6.112 * exp((17.62 * t_c) / (243.12 + t_c));
}

/* Pressão de vapor REAL pelo ponto de orvalho (Pa) */
static double vapor_pressure_pa_from_dewpoint(double td_c) {
    return es_hPa_magnus(td_c) * 100.0; /* hPa -> Pa */
}

/* Pressão de saturação na temperatura do ar (Pa) */
static double saturation_vapor_pressure_pa(double t_c) {
    return es_hPa_magnus(t_c) * 100.0;
}

/* Umidade relativa (0..1) a partir de T e Td (boa aproximação) */
static double relative_humidity(double t_c, double td_c) {
    double e = vapor_pressure_pa_from_dewpoint(td_c);
    double es = saturation_vapor_pressure_pa(t_c);
    if (es <= 0) return 0.0;
    return clamp(e / es, 0.0, 1.0);
}

/* Densidade do ar úmido (kg/m³): rho = Pd/(Rd*T) + Pv/(Rv*T) */
static double moist_air_density(double p_pa, double t_k, double pv_pa) {
    double pd = p_pa - pv_pa;
    if (pd <= 0) pd = 1.0; /* evita estado físico inválido por parse ruim */
    return (pd / (R_DRY * t_k)) + (pv_pa / (R_VAPOR * t_k));
}

/* Aproximação operacional de densidade altitude (m)
 * DA ≈ PA + 120*(OAT - ISA), com PA em ft; aqui fazemos versão simples em metros.
 * 1) Pressão altitude (PA) por ISA: z = (T0/L) * (1 - (P/P0)^(R*L/g))  (aprox)
 * 2) Corrige por temperatura (OAT vs ISA)
 */
static double pressure_altitude_m(double p_pa) {
    /* Fórmula barométrica ISA (troposfera) */
    const double g = 9.80665;
    const double R = 287.05287;
    double ratio = clamp(p_pa / P0_PA, 0.01, 2.0);
    double expn = (R * LAPSE_KPM) / g;
    /* z = (T0/L) * (1 - (P/P0)^expn) */
    return (T0_K / LAPSE_KPM) * (1.0 - pow(ratio, expn));
}

static double isa_temp_at_alt_k(double alt_m) {
    /* troposfera simples */
    return T0_K - LAPSE_KPM * alt_m;
}

static double density_altitude_m(double p_pa, double oat_k) {
    double pa_m = pressure_altitude_m(p_pa);
    double isa_k = isa_temp_at_alt_k(pa_m);
    /* regra prática: ~118.8 m por 1°C (aprox 120 ft/°C) */
    double delta_c = (oat_k - isa_k);
    return pa_m + (118.8 * delta_c);
}

static double stall_speed(double weight_n, double rho, double S, double clmax) {
    rho = fmax(rho, 1e-6);
    clmax = fmax(clmax, 1e-6);
    S = fmax(S, 1e-6);
    return sqrt((2.0 * weight_n) / (rho * S * clmax));
}

/* ---------- Dados ---------- */
typedef struct {
    double temp_c;
    double dew_c;
    double press_hpa;
    int wind_dir_deg;     /* -1 = VRB/unknown */
    double wind_kt;
    double gust_kt;
    char raw_metar[512];
    int valid;
} SensorData;

typedef struct {
    /* Atmosfera calculada */
    double temp_k;
    double press_pa;
    double pv_pa;
    double rh;            /* 0..1 */
    double rho_real;      /* kg/m3 */
    double pressure_alt_m;
    double density_alt_m;

    /* Aeronave */
    double v_stall;
    double margin;

    /* Risco */
    int flag_ts;
    int flag_cb;
    int flag_shra;
    int flag_gust;
} CoreState;

/* ---------- Parsing METAR (robusto o suficiente para SBFL) ---------- */

/* Parse temp/dew token: "24/20" "M02/M05" */
static int parse_tempdew(const char *tok, double *t_c, double *td_c) {
    int signT = 1, signD = 1;
    int a = 0, b = 0;
    const char *p = tok;

    if (p[0] == 'M') { signT = -1; p++; }
    if (!isdigit((unsigned char)p[0])) return 0;
    a = atoi(p);

    const char *slash = strchr(tok, '/');
    if (!slash) return 0;

    p = slash + 1;
    if (p[0] == 'M') { signD = -1; p++; }
    if (!isdigit((unsigned char)p[0])) return 0;
    b = atoi(p);

    *t_c = signT * (double)a;
    *td_c = signD * (double)b;
    return 1;
}

/* Parse pressure token: "Q1012" (hPa) */
static int parse_qnh(const char *tok, double *qnh_hpa) {
    if (tok[0] != 'Q') return 0;
    if (!isdigit((unsigned char)tok[1])) return 0;
    int v = atoi(tok + 1);
    if (v < 800 || v > 1100) return 0;
    *qnh_hpa = (double)v;
    return 1;
}

/* Parse wind token: "12005KT", "VRB03KT", "12005G15KT" */
static int parse_wind(const char *tok, int *dir_deg, double *spd_kt, double *gst_kt) {
    size_t n = strlen(tok);
    if (n < 5) return 0;
    if (strstr(tok, "KT") == NULL) return 0;

    *gst_kt = 0.0;

    if (!strncmp(tok, "VRB", 3)) {
        *dir_deg = -1;
    } else {
        if (!isdigit((unsigned char)tok[0]) || !isdigit((unsigned char)tok[1]) || !isdigit((unsigned char)tok[2]))
            return 0;
        *dir_deg = (tok[0]-'0')*100 + (tok[1]-'0')*10 + (tok[2]-'0');
    }

    /* speed starts at pos 3 for dir, pos 3 for VRB too */
    const char *p = tok + 3;
    if (!isdigit((unsigned char)p[0])) return 0;
    *spd_kt = atof(p);

    const char *g = strchr(tok, 'G');
    if (g && isdigit((unsigned char)g[1])) {
        *gst_kt = atof(g + 1);
    }
    return 1;
}

static int token_contains(const char *tok, const char *needle) {
    return strstr(tok, needle) != NULL;
}

static SensorData fetch_sbfl(void) {
    SensorData d;
    memset(&d, 0, sizeof(d));
    d.valid = 0;
    d.wind_dir_deg = -1;

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "wget -q -O %s \"%s\"", METAR_FILE, METAR_URL);
    int rc = system(cmd);
    (void)rc;

    FILE *f = fopen(METAR_FILE, "r");
    if (!f) return d;

    /* Arquivo tem 2 linhas: timestamp e METAR */
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "SBFL")) {
            strncpy(d.raw_metar, line, sizeof(d.raw_metar)-1);
            d.raw_metar[strcspn(d.raw_metar, "\n")] = 0;
            d.valid = 1;
            break;
        }
    }
    fclose(f);

    if (!d.valid) return d;

    /* Tokeniza por espaço e varre */
    char buf[512];
    strncpy(buf, d.raw_metar, sizeof(buf)-1);
    buf[sizeof(buf)-1] = 0;

    char *saveptr = NULL;
    char *tok = strtok_r(buf, " ", &saveptr);

    int got_td = 0, got_qnh = 0, got_wind = 0;

    while (tok) {
        /* Flags de risco (METAR tokens) */
        if (!d.valid) break;
        if (!strcmp(tok, "TS") || token_contains(tok, "TSRA") || token_contains(tok, "TS")) {
            /* cuidado: "TS" pode vir colado */
            /* simplificação: se contém TS, marcamos */
            /* (não confunde com "T" isolado) */
        }

        /* Temp/Dew */
        if (!got_td) {
            double t = 0, td = 0;
            if (parse_tempdew(tok, &t, &td)) {
                d.temp_c = t;
                d.dew_c = td;
                got_td = 1;
            }
        }

        /* QNH */
        if (!got_qnh) {
            double q = 0;
            if (parse_qnh(tok, &q)) {
                d.press_hpa = q;
                got_qnh = 1;
            }
        }

        /* Wind */
        if (!got_wind) {
            int dir = -1; double spd = 0, gst = 0;
            if (parse_wind(tok, &dir, &spd, &gst)) {
                d.wind_dir_deg = dir;
                d.wind_kt = spd;
                d.gust_kt = gst;
                got_wind = 1;
            }
        }

        tok = strtok_r(NULL, " ", &saveptr);
    }

    /* valida mínimo: temp/dew e pressão */
    if (!got_td || !got_qnh) d.valid = 0;
    return d;
}

/* ---------- Processamento físico + risco ---------- */
static void process_kernel(const SensorData *s, CoreState *st) {
    memset(st, 0, sizeof(*st));

    st->temp_k = s->temp_c + KELVIN;
    st->press_pa = s->press_hpa * 100.0; /* hPa -> Pa */

    st->pv_pa = vapor_pressure_pa_from_dewpoint(s->dew_c);
    st->rh = relative_humidity(s->temp_c, s->dew_c);

    st->rho_real = moist_air_density(st->press_pa, st->temp_k, st->pv_pa);

    st->pressure_alt_m = pressure_altitude_m(st->press_pa);
    st->density_alt_m  = density_altitude_m(st->press_pa, st->temp_k);

    st->v_stall = stall_speed(AC_WEIGHT_N, st->rho_real, AC_WING_S, AC_CL_MAX);
    st->margin  = AC_SPEED_MS / fmax(st->v_stall, 1e-6);

    /* Riscos: inferência simples via string bruta (sem inventar) */
    st->flag_ts = (strstr(s->raw_metar, "TS") != NULL);
    st->flag_cb = (strstr(s->raw_metar, "CB") != NULL);
    st->flag_shra = (strstr(s->raw_metar, "SHRA") != NULL || strstr(s->raw_metar, "+RA") != NULL);
    st->flag_gust = (s->gust_kt > 0.1);
}

/* ---------- UI ASCII ---------- */
static void draw_gauge(const char *label, double value, double max, int width, int invert_critical) {
    double v = value;
    double ratio = (max <= 0) ? 0 : (v / max);
    ratio = clamp(ratio, 0, 1);

    int filled = (int)round(ratio * width);
    if (filled < 0) filled = 0;
    if (filled > width) filled = width;

    printf(" %-10s [", label);
    for (int i = 0; i < width; i++) {
        if (i < filled) {
            /* critical for margin: if invert_critical and value is low => show ! */
            if (invert_critical && (value < 1.3)) putchar('!');
            else putchar('=');
        } else {
            putchar('.');
        }
    }
    printf("] %.3f\n", value);
}

static void render(const SensorData *s, const CoreState *st) {
    printf("\033[2J\033[H");

    printf("   _____  _______  _        _______      _____   ____  \n");
    printf("  / ____|/ ____| || |      |___  \\ \\    / /__ \\ /___ \\ \n");
    printf(" | (___ | |    | || |_  ___  __) |\\ \\  / /   ) |  __) |\n");
    printf("  \\___ \\| |    |__   _|/ _ \\|__ <  \\ \\/ /   / /  |__ < \n");
    printf("  ____) | |____   | | | (_) |__) |  \\  /   / /_  ___) |\n");
    printf(" |_____/ \\_____|  |_|  \\___/____/    \\/   |____||____/ \n");
    printf(" TARGET: SBFL (Florianopolis) | MODE: METAL_FRIO | SCIENCE: PURE\n");
    printf(" ================================================================\n");

    if (!s->valid) {
        printf("\n [!] Sem METAR válido ainda. Verifique: internet/wget/URL.\n");
        printf("     Fonte: %s\n", METAR_URL);
        return;
    }

    printf("\n [ METAR RAW ]\n %s\n", s->raw_metar);
    printf(" ---------------------------------------------------------------\n");
    printf(" TEMP: %6.2f C | DEW: %6.2f C | RH: %5.1f %%\n", s->temp_c, s->dew_c, st->rh * 100.0);
    printf(" QNH : %6.0f hPa | WIND: ", s->press_hpa);

    if (s->wind_dir_deg < 0) printf("VRB ");
    else printf("%03d ", s->wind_dir_deg);

    printf("%4.0f kt", s->wind_kt);
    if (s->gust_kt > 0.1) printf(" G%0.0f", s->gust_kt);
    printf("\n");

    printf("\n [ ATMOSPHERE (REAL) ]\n");
    printf(" Pv (dew) : %8.1f Pa\n", st->pv_pa);
    printf(" Rho moist: %8.4f kg/m^3\n", st->rho_real);
    printf(" PressAlt : %8.0f m  | DensAlt: %8.0f m\n", st->pressure_alt_m, st->density_alt_m);

    printf("\n [ FLIGHT ENVELOPE (EXAMPLE AIRFRAME) ]\n");
    printf(" Vstall   : %8.2f m/s\n", st->v_stall);
    printf(" Vactual  : %8.2f m/s\n", (double)AC_SPEED_MS);
    printf(" Margin   : %8.3f  (>= 1.30 recomendado p/ folga)\n", st->margin);

    printf("\n [ GAUGES ]\n");
    draw_gauge("MARGIN", st->margin, 2.0, 34, 1);
    draw_gauge("RH", st->rh, 1.0, 34, 0);

    printf("\n [ HAZARD FLAGS (from METAR tokens) ]\n");
    printf(" TS:%s  CB:%s  HeavyRain:%s  Gust:%s\n",
           st->flag_ts ? "YES" : "no",
           st->flag_cb ? "YES" : "no",
           st->flag_shra ? "YES" : "no",
           st->flag_gust ? "YES" : "no");

    printf("\n [ DECISION (simple, deterministic) ]\n");
    if (st->flag_cb || st->flag_ts) {
        printf("  \033[0;31mNO_GO\033[0m  -> Thunderstorm/CB indicated in METAR. Avoid/hold/reroute.\n");
    } else if (st->margin < 1.10) {
        printf("  \033[0;31mCRITICAL\033[0m -> Margin too low. Increase speed/reduce weight/abort.\n");
    } else if (st->margin < 1.30) {
        printf("  \033[0;33mCAUTION\033[0m  -> Tight margin. Treat as limit regime.\n");
    } else {
        printf("  \033[0;32mGO\033[0m      -> Within basic envelope (still monitor weather).\n");
    }

    printf("\n [>] Update in %ds | CTRL+C to halt.\n", UPDATE_SECONDS);
}

/* ---------- Main loop ---------- */
int main(void) {
    /* seed rand once (if you later add any non-deterministic sim hooks) */
    srand((unsigned)time(NULL));

    while (1) {
        SensorData s = fetch_sbfl();
        CoreState st;

        if (s.valid) {
            process_kernel(&s, &st);
            render(&s, &st);
        } else {
            printf("\033[2J\033[H");
            printf("SCALA33 SBFL — aguardando METAR válido...\n");
            printf("Verifique: wget, internet, URL NOAA.\n");
            printf("Fonte: %s\n", METAR_URL);
        }

        sleep(UPDATE_SECONDS);
    }
    return 0;
}
