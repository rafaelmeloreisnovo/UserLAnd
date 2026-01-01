/*
 * signer_tool.c
 *
 * Small CLI to sign a file with ed25519_tweet implementation (demo).
 *
 * Usage:
 *   gcc -O3 signer_tool.c ed25519_tweet.c blake3_fallback.c -o signer_tool -lm
 *   ./signer_tool sign --seed-key demo_keypair.txt payload.bin signature.sig
 *
 * demo_keypair.txt format:
 *   seed_hex (64 hex chars for 32 bytes)
 *
 * NOTE: demo only. Replace with real key management in production.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ed25519_tweet.h"

static void hex2bin(const char *hex, unsigned char *out, size_t outlen) {
    for (size_t i=0;i<outlen;i++) {
        unsigned int v;
        sscanf(hex + 2*i, "%2x", &v);
        out[i] = (unsigned char)v;
    }
}

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s sign <seedfile> <payload> <sigout>\n", argv[0]);
        return 2;
    }
    const char *cmd = argv[1];
    if (strcmp(cmd, "sign") != 0) { fprintf(stderr, "Only 'sign' supported\n"); return 2; }
    const char *seedfile = argv[2];
    const char *payload = argv[3];
    const char *sigout = argv[4];

    FILE *sf = fopen(seedfile, "r"); if (!sf) { perror("seedfile"); return 3; }
    char hex[200]; if (!fgets(hex, sizeof(hex), sf)) { fclose(sf); return 4; } fclose(sf);
    while (hex[strlen(hex)-1] == '\n') hex[strlen(hex)-1] = '\0';
    unsigned char seed[32]; hex2bin(hex, seed, 32);
    unsigned char pk[32], sk[64];
    ed25519_keypair_from_seed(pk, sk, seed);

    FILE *pf = fopen(payload, "rb"); if (!pf) { perror("payload"); return 5; }
    fseek(pf, 0, SEEK_END); long psz = ftell(pf); rewind(pf);
    unsigned char *buf = malloc((size_t)psz); fread(buf, 1, psz, pf); fclose(pf);

    unsigned char sig[64];
    ed25519_sign(sig, buf, (size_t)psz, sk);

    FILE *sfout = fopen(sigout, "wb"); if (!sfout) { perror("sigout"); free(buf); return 6; }
    fwrite(sig, 1, 64, sfout); fclose(sfout);
    free(buf);
    printf("Signed %s -> %s (demo signature)\n", payload, sigout);
    return 0;
}