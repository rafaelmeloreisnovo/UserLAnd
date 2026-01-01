/*
 * tools/agent/ziprafa_agent.c
 *
 * Minimal daemon skeleton for scheduled tasks, metrics and running proofs.
 * For demo only; extend with systemd unit or containerization for production.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

int main(void) {
    printf("ziprafa_agent: demo agent running. Press Ctrl-C to exit.\n");
    while (1) {
        time_t t = time(NULL);
        printf("[%lld] agent heartbeat\n", (long long)t);
        sleep(60);
    }
    return 0;
}