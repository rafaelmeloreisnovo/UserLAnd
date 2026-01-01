#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine_v3.h"
#include "engine_accel.h"
#include "ethica_policy.h"
#include "gpcu.h"
#include "bitstack.h"

/* high-level CLI that can run engines, gpcu programs, and write micro-log JSON */

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s --mode [v3|accel|gpcu] [options]\n", prog);
    fprintf(stderr, "Examples:\n  %s --mode v3 --steps 100 --size 32 --p 4 --out micro.json\n", prog);
}

int main(int argc, char **argv) {
    const char *mode = NULL;
    int steps = 100;
    int size = 32;
    int p = 4;
    const char *out = NULL;
    int verbose = 1;
    /* parse minimal args */
    for(int i=1;i<argc;i++) {
        if(strcmp(argv[i],"--mode")==0 && i+1<argc) mode = argv[++i];
        else if(strcmp(argv[i],"--steps")==0 && i+1<argc) steps = atoi(argv[++i]);
        else if(strcmp(argv[i],"--size")==0 && i+1<argc) size = atoi(argv[++i]);
        else if(strcmp(argv[i],"--p")==0 && i+1<argc) p = atoi(argv[++i]);
        else if(strcmp(argv[i],"--out")==0 && i+1<argc) out = argv[++i];
        else if(strcmp(argv[i],"--quiet")==0) verbose = 0;
    }
    if(!mode) { usage(argv[0]); return 1; }

    if(strcmp(mode,"v3")==0) {
        EngineV3 *e = engine_v3_create(size, size, p);
        int rc = engine_v3_run(e, steps, 1.0f, 0.01f, 0.963999f, 0.01, 0.05f, out, verbose);
        engine_v3_free(e);
        return rc;
    } else if(strcmp(mode,"accel")==0) {
        int nd = 3;
        int dims[3] = {size, size, 8};
        EngineAccel *ea = engine_accel_create(nd, dims, p);
        int rc = engine_accel_run(ea, steps, 1.0f, 0.01f, 0.963999f, 0.01, 0.05f, 0, 1, out, verbose);
        engine_accel_free(ea);
        return rc;
    } else if(strcmp(mode,"gpcu")==0) {
        /* demo program */
        const char *prog[] = {
            "MOV r0 5",
            "MOV r1 3",
            "ADD r0 r1",
            "HASH r0",
            "HALT"
        };
        GPCU *g = gpcu_create(4096);
        PCB *pcb = gpcu_run_program(g, prog, 5, 100);
        if(pcb) {
            printf("GPCU PID %d regs[0]=%d\n", pcb->pid, pcb->regs[0]);
        }
        gpcu_free(g);
        return 0;
    } else {
        usage(argv[0]);
        return 1;
    }
}