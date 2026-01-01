#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*
  SCALA33_FEEDER (C puro, Termux)
  - Fetch METAR XML (AviationWeather) -> temp, dewpoint, pressure
  - Compute RH from T & Td
  - Compute moist air density rho
  - Fetch USGS ATOM -> pega magnitude do 1º evento do feed (proxy sísmico global)
  - Print numbers (rho, Vstall, margin) + proxy seismicIndex
*/

#define CLAMP01(x) ((x) < 0.0 ? 0.0 : ((x) > 1.0 ? 1.0 : (x)))

static char* slurp_cmd(const char* cmd){
  FILE* fp = popen(cmd, "r");
  if(!fp) return NULL;

  size_t cap = 1<<16;
  size_t len = 0;
  char* buf = (char*)malloc(cap);
  if(!buf){ pclose(fp); return NULL; }

  while(1){
    if(len + 4096 + 1 > cap){
      cap *= 2;
      char* nb = (char*)realloc(buf, cap);
      if(!nb){ free(buf); pclose(fp); return NULL; }
      buf = nb;
    }
    size_t n = fread(buf + len, 1, 4096, fp);
    len += n;
    if(n == 0) break;
  }
  buf[len] = 0;
  pclose(fp);
  return buf;
}

static int tag_double(const char* xml, const char* tag, double* out){
  char open[64], close[64];
  snprintf(open, sizeof(open), "<%s>", tag);
  snprintf(close, sizeof(close), "</%s>", tag);
  const char* p = strstr(xml, open);
  if(!p) return 0;
  p += strlen(open);
  const char* q = strstr(p, close);
  if(!q) return 0;
  char tmp[64];
  size_t n = (size_t)(q - p);
  if(n > 63) n = 63;
  memcpy(tmp, p, n);
  tmp[n] = 0;
  *out = strtod(tmp, NULL);
  return 1;
}

int main(int argc, char** argv){
  const char* ICAO = (argc >= 2) ? argv[1] : "SBGR";

  // AviationWeather METAR XML (mostRecent within last 2 hours)
  char metar_url[1024];
  snprintf(metar_url, sizeof(metar_url),
    "https://aviationweather.gov/api/data/metar?format=xml&hours=2&mostRecent=true&ids=%s",
    ICAO
  );

  // USGS ATOM (global M2.5 last day)
  const char* eq_url =
    "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/2.5_day.atom";

  char cmd[1400];

  // ── Fetch METAR XML
  snprintf(cmd, sizeof(cmd), "wget -qO- \"%s\"", metar_url);
  char* metar = slurp_cmd(cmd);
  if(!metar){
    fprintf(stderr, "ERR: wget METAR failed\n");
    return 1;
  }

  // Parse METAR XML fields
  double tempC = NAN, dewC = NAN, altimInHg = NAN, seaLevelMb = NAN;
  // AviationWeather XML varies; try a few common tags:
  tag_double(metar, "temp_c", &tempC);
  tag_double(metar, "dewpoint_c", &dewC);
  tag_double(metar, "altim_in_hg", &altimInHg);
  tag_double(metar, "sea_level_pressure_mb", &seaLevelMb);

  // Pressure estimate (Pa)
  double pressurePa = NAN;
  if(!isnan(seaLevelMb)){
    pressurePa = seaLevelMb * 100.0;
  } else if(!isnan(altimInHg)){
    double hPa = altimInHg * 33.8639;
    pressurePa = hPa * 100.0;
  } else {
    pressurePa = 1013.25 * 100.0; // fallback
  }

  // RH from T & Td (Magnus)
  double rh01 = 0.5;
  if(!isnan(tempC) && !isnan(dewC)){
    const double a = 17.625, b = 243.04;
    double gammaTd = (a * dewC) / (b + dewC);
    double gammaT  = (a * tempC) / (b + tempC);
    double rh = 100.0 * exp(gammaTd - gammaT);
    rh01 = CLAMP01(rh / 100.0);
  }

  // Moist air density rho = pd/(Rd*T) + pv/(Rv*T)
  double T = (!isnan(tempC) ? (tempC + 273.15) : (15.0 + 273.15));
  // Saturation vapor pressure (Pa) Magnus-Tetens
  double es_hPa = 6.112 * exp((17.67 * tempC) / (tempC + 243.5));
  double es_Pa = es_hPa * 100.0;
  double pv = rh01 * es_Pa;
  double pd = pressurePa - pv;
  const double Rd = 287.05, Rv = 461.495;
  double rho = 0.0;
  if(pd > 0) rho = (pd/(Rd*T)) + (pv/(Rv*T));

  // ── Fetch EQ ATOM
  snprintf(cmd, sizeof(cmd), "wget -qO- \"%s\"", eq_url);
  char* eq = slurp_cmd(cmd);
  if(!eq){
    fprintf(stderr, "ERR: wget EQ ATOM failed\n");
    free(metar);
    return 1;
  }

  // Parse first magnitude from ATOM title: "<title>M 3.2 - ..."
  double mag = 0.0;
  char* entry = strstr(eq, "<entry>");
  if(entry){
    char* title = strstr(entry, "<title>");
    if(title){
      title += (int)strlen("<title>");
      char* mpos = strstr(title, "M ");
      if(mpos) mag = strtod(mpos + 2, NULL);
    }
  }
  double seismicIndex = CLAMP01(mag / 9.0);

  // Example aircraft constants (troca quando quiser)
  double massKg = 1200.0;
  double weightN = massKg * 9.80665;
  double wingArea = 16.2;
  double clMax = 1.55;
  double airspeedMS = 61.0;

  // Vstall and margin
  double vstall = sqrt((2.0 * weightN) / (rho * wingArea * clMax));
  double margin = airspeedMS / vstall;

  // Simple regime (só pra imprimir “estado”)
  const char* regime = "SAFE";
  if(margin <= 1.0) regime = "ABORT";
  else if(seismicIndex > 0.55) regime = "CRITICAL";
  else if(seismicIndex > 0.30) regime = "CAUTION";

  // Output
  printf("\n🦉 SCALA33 FEEDER (XML live)\n");
  printf("──────────────────────────────────────────────\n");
  printf("ICAO: %s\n", ICAO);
  printf("METAR: T=%.2f°C  Td=%.2f°C  RH=%.1f%%  P=%.1f hPa\n",
         tempC, dewC, rh01*100.0, pressurePa/100.0);
  printf("PHYS: rho(moist)=%.4f kg/m³\n", rho);
  printf("AERO: Vstall=%.3f m/s  Margin=%.3f\n", vstall, margin);
  printf("SEISMIC: mag≈%.2f  seismicIndex=%.3f\n", mag, seismicIndex);
  printf("Ω REGIME (simple): %s\n", regime);
  printf("──────────────────────────────────────────────\n\n");

  free(metar);
  free(eq);
  return 0;
}
