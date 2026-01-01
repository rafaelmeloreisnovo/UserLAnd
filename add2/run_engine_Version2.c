#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "engine_v3_3d.h"

int main(int argc, char **argv) {
    int X=32, Y=32, D=8, P=4;
    int steps = 100;
    float threshold = 1.0f;
    float dissipation = 0.01f;
    float phi_ethica = 0.96f;
    float inject_rate = 0.005f;
    float inject_strength = 0.1f;
    double watchdog_E_max = 1e12;
    unsigned long watchdog_K_max = 1000000;
    unsigned int seed = (unsigned int)time(NULL);

    // simple arg parse
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"--steps")==0 && i+1<argc) steps = atoi(argv[++i]);
        else if(strcmp(argv[i],"--size")==0 && i+1<argc) { X = Y = atoi(argv[++i]); }
        else if(strcmp(argv[i],"--d")==0 && i+1<argc) D = atoi(argv[++i]);
        else if(strcmp(argv[i],"--p")==0 && i+1<argc) P = atoi(argv[++i]);
        else if(strcmp(argv[i],"--seed")==0 && i+1<argc) seed = (unsigned int)atoi(argv[++i]);
        else if(strcmp(argv[i],"--inject_rate")==0 && i+1<argc) inject_rate = atof(argv[++i]);
    }

    printf("Engine run: X=%d Y=%d D=%d P=%d steps=%d seed=%u\n", X,Y,D,P,steps,seed);
    engine_srand(seed);
    EngineState *s = engine_create(X,Y,D,P);
    if(!s) { fprintf(stderr,"alloc fail\n"); return 1; }

    int rc = engine_run_loop(s, steps, threshold, dissipation, phi_ethica, inject_rate, inject_strength, watchdog_E_max, watchdog_K_max);
    if(rc != 0) {
        fprintf(stderr,"Engine aborted with code %d\n", rc);
    } else {
        printf("Run complete. steps=%lu final_energy=%.6f hash=%016llx\n", s->total_steps, s->total_energy, (unsigned long long)engine_hash64(s));
    }

    engine_free(s);
    return rc;
}