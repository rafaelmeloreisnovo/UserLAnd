/*
 * tools/cli/ziprafa_cli.c
 *
 * Simple CLI that links to core, fec and crypto modules.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../core/include/libziprafa_core.h"
#include "../../crypto/include/libziprafa_crypto.h"

static const char *arg_val(int argc, char **argv, const char *k) {
    for (int i=1;i<argc;i++) if (strcmp(argv[i], k)==0 && i+1<argc) return argv[i+1];
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("ziprafa_cli usage: selftest | gen-map | shard | recover | bench\n");
        return 1;
    }
    const char *cmd = argv[1];
    if (strcmp(cmd, "selftest")==0) {
        int r = zcore_selftest();
        printf("selftest: %s\n", r==0 ? "OK":"FAIL");
        return r==0 ? 0 : 2;
    }
    if (strcmp(cmd, "gen-map")==0) {
        const char *pref = arg_val(argc, argv, "--prefix"); if (!pref) pref = "map";
        layout_cfg_t cfg = { 1024, 1000, 4096, 6, 1, 0 };
        zcore_build_map(&cfg, (uint64_t)time(NULL), "maps/example.map.json");
        printf("map generated: maps/example.map.json\n");
        return 0;
    }
    if (strcmp(cmd, "shard")==0) {
        const char *in = arg_val(argc, argv, "--in"); const char *k_s = arg_val(argc, argv, "--k");
        if (!in || !k_s) { printf("shard requires --in and --k\n"); return 2; }
        int k = atoi(k_s);
        zcore_shard_file(in, k, "out/demo_payload");
        printf("shard done\n");
        return 0;
    }
    if (strcmp(cmd, "recover")==0) {
        const char *pref = arg_val(argc, argv, "--prefix"); if (!pref) pref="out/demo_payload";
        const char *k_s = arg_val(argc, argv, "--k"); const char *miss_s = arg_val(argc, argv, "--missing");
        if (!k_s || !miss_s) { printf("recover needs --k and --missing\n"); return 2; }
        int k = atoi(k_s), missing = atoi(miss_s);
        zcore_recover_xor(pref, k, missing, "out/recovered.bin");
        printf("recover done\n");
        return 0;
    }
    printf("unknown command\n");
    return 1;
}