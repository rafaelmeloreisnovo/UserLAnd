// raf_cat_hz.c
// Low-level: read/write/poll. Dois modos:
// 1) cat puro: copia stdin->stdout até EOF
// 2) --tone: gera PCM 16-bit mono em stdout, e lê comandos "HZ <valor>\n" no stdin até EOF
//
// Build: clang -O2 -Wall -Wextra raf_cat_hz.c -lm -o raf_cat_hz
// Uso:
//   Cat:  ./raf_cat_hz < in.bin > out.bin
//   Tone: ./raf_cat_hz --tone --hz 144 --rate 48000 > out.pcm
//   Controle (em outro terminal):  echo "HZ 288" > /proc/<pid>/fd/0  (ou rode interativo e digite HZ 288)
//   Player (Linux): aplay -f S16_LE -c 1 -r 48000 out.pcm

#define _GNU_SOURCE
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void die(const char *msg) {
    write(2, msg, (int)strlen(msg));
    write(2, "\n", 1);
    _exit(1);
}

static int parse_i(const char *s, int *out) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s) return 0;
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') end++;
    if (*end != '\0') return 0;
    if (v < 1 || v > 2000000) return 0;
    *out = (int)v;
    return 1;
}

static int try_parse_hz_line(const char *line, int *hz_out) {
    // Aceita: "HZ 144", "hz 288", "Hz\t777"
    while (*line == ' ' || *line == '\t') line++;
    if (!(line[0]=='H' || line[0]=='h') || !(line[1]=='Z' || line[1]=='z')) return 0;
    line += 2;
    while (*line == ' ' || *line == '\t') line++;
    return parse_i(line, hz_out);
}

static void cat_until_eof(void) {
    uint8_t buf[64 * 1024];
    for (;;) {
        ssize_t n = read(0, buf, sizeof(buf));
        if (n == 0) break;               // EOF
        if (n < 0) {
            if (errno == EINTR) continue;
            die("read() falhou");
        }
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(1, buf + off, (size_t)(n - off));
            if (w < 0) {
                if (errno == EINTR) continue;
                die("write() falhou");
            }
            off += w;
        }
    }
}

static void tone_until_eof(int hz_init, int rate) {
    // Gera PCM S16_LE mono em stdout.
    // stdin recebe linhas: "HZ <n>\n" (qualquer hora). EOF encerra.

    int hz = hz_init;
    double phase = 0.0;
    const double two_pi = 2.0 * M_PI;

    // buffers
    int16_t audio[1024];        // ~21ms @ 48k
    char    linebuf[512];
    size_t  linelen = 0;

    struct pollfd pfd;
    pfd.fd = 0;
    pfd.events = POLLIN | POLLHUP;

    for (;;) {
        // 1) Poll curto: se tiver comando, lê; senão, gera bloco de áudio.
        int pr = poll(&pfd, 1, 0); // 0ms: não bloqueia
        if (pr < 0) {
            if (errno == EINTR) continue;
            die("poll() falhou");
        }

        // Se stdin fechou, encerra
        if (pfd.revents & POLLHUP) break;

        // 2) Se tem entrada, lê e processa linhas (pode ter várias)
        if (pfd.revents & POLLIN) {
            char in[256];
            ssize_t n = read(0, in, sizeof(in));
            if (n == 0) break; // EOF
            if (n < 0) {
                if (errno == EINTR) continue;
                die("read(stdin) falhou");
            }

            for (ssize_t i = 0; i < n; i++) {
                char c = in[i];
                if (linelen < sizeof(linebuf) - 1) {
                    linebuf[linelen++] = c;
                }
                if (c == '\n') {
                    linebuf[linelen] = '\0';
                    int newhz = 0;
                    if (try_parse_hz_line(linebuf, &newhz)) {
                        hz = newhz;
                        // feedback no stderr (opcional)
                        dprintf(2, "[HZ] => %d\n", hz);
                    }
                    linelen = 0;
                }
            }
        }

        // 3) Gera um bloco de áudio (seno) na frequência atual
        double step = two_pi * (double)hz / (double)rate;
        for (int i = 0; i < (int)(sizeof(audio)/sizeof(audio[0])); i++) {
            double s = sin(phase);
            // amplitude moderada pra evitar clipping
            audio[i] = (int16_t)(s * 12000.0);
            phase += step;
            if (phase >= two_pi) phase -= two_pi;
        }

        // 4) Escreve no stdout
        uint8_t *p = (uint8_t*)audio;
        size_t bytes = sizeof(audio);
        size_t off = 0;
        while (off < bytes) {
            ssize_t w = write(1, p + off, bytes - off);
            if (w < 0) {
                if (errno == EINTR) continue;
                die("write(audio) falhou");
            }
            off += (size_t)w;
        }

        // 5) Pequeno yield (evita 100% CPU). Ajuste fino: 1ms.
        usleep(1000);
    }
}

int main(int argc, char **argv) {
    int tone = 0;
    int hz = 144;
    int rate = 48000;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--tone")) {
            tone = 1;
        } else if (!strcmp(argv[i], "--hz") && i + 1 < argc) {
            hz = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--rate") && i + 1 < argc) {
            rate = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--help")) {
            const char *help =
                "Uso:\n"
                "  ./raf_cat_hz < in > out                (cat até EOF)\n"
                "  ./raf_cat_hz --tone --hz 144 --rate 48000 > out.pcm\n"
                "Comandos no stdin (modo --tone):\n"
                "  HZ 288\\n   (muda frequência em tempo real)\n";
            write(1, help, (int)strlen(help));
            return 0;
        }
    }

    if (!tone) {
        cat_until_eof();
        return 0;
    }

    if (hz < 1) hz = 1;
    if (rate < 8000) rate = 8000;

    tone_until_eof(hz, rate);
    return 0;
}
