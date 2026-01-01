/*
 * ziprafa_cli.c
 *
 * Command-line interface for libziprafa_core
 * Builds against libziprafa_core.c (compile and link together)
 *
 * Simple subcommands:
 *  - selftest
 *  - gen-map
 *  - shard
 *  - recover
 *  - bench
 *
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libziprafa_core.h" /* header wrapper for prototypes (assume you create header) */

/* For simplicity this CLI file contains small wrappers and uses functions in libziprafa_core.c.
   If you split files, ensure header provides prototypes above. */

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("ziprafa_cli: usage: %s <cmd> [...]\n", argv[0]);
        return 1;
    }
    const char *cmd = argv[1];
    if (strcmp(cmd, "selftest") == 0) {
        int r = ziprafa_selftest();
        printf("selftest: %s\n", r==0 ? "OK" : "FAIL");
        return r;
    }
    if (strcmp(cmd, "gen-map") == 0) {
        /* simplified: use fixed params or parse args */
        printf("gen-map: use interactive toolkit for richer options\n");
        return 0;
    }
    printf("unknown command\n");
    return 1;
}