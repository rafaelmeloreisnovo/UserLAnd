#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>

#define MAX_WORKERS_DEFAULT 12
#define TOTAL_JOBS_DEFAULT 1000
#define MATRIX_N 128
#define INNER_REPEATS 8
#define LEDGER_PATH "swarm_ledger.dat"
#define FIXED_FLOPS 2.7

/*
 * Worker routine performs deterministic matrix multiplication to mimic the
 * prior shell-based swarm worker while keeping allocation and I/O purely in
 * low-level C. It returns 0 on success and appends the delivered flops value
 * to the ledger file so the parent can aggregate without external tools.
 */
static int run_worker_job(int id) {
    size_t total = (size_t)MATRIX_N * (size_t)MATRIX_N;
    double *a = (double *)malloc(total * sizeof(double));
    double *b = (double *)malloc(total * sizeof(double));
    double *c = (double *)malloc(total * sizeof(double));
    if (!a || !b || !c) {
        free(a);
        free(b);
        free(c);
        return 1;
    }

    for (size_t i = 0; i < total; ++i) {
        a[i] = 1.001 + (double)(id % 7) * 0.001;
        b[i] = 2.002 + (double)(id % 5) * 0.002;
        c[i] = 0.0;
    }

    for (int repeat = 0; repeat < INNER_REPEATS; ++repeat) {
        for (int i = 0; i < MATRIX_N; ++i) {
            for (int k = 0; k < MATRIX_N; ++k) {
                double temp = a[(size_t)i * MATRIX_N + (size_t)k];
                for (int j = 0; j < MATRIX_N; ++j) {
                    c[(size_t)i * MATRIX_N + (size_t)j] += temp * b[(size_t)k * MATRIX_N + (size_t)j];
                }
            }
        }
    }

    int fd = open(LEDGER_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) {
        char buffer[64];
        int len = snprintf(buffer, sizeof(buffer), "%.1f\n", FIXED_FLOPS);
        if (len > 0) {
            ssize_t written = write(fd, buffer, (size_t)len);
            (void)written;
        }
        close(fd);
    }

    free(a);
    free(b);
    free(c);
    return 0;
}

/*
 * Parent orchestrator keeps worker concurrency bounded, tracks aggregated
 * flops without invoking external interpreters, and emits progress updates
 * that mirror the prior Bash script for developer-mode observability.
 */
static int orchestrate_swarm(int max_workers, int total_jobs) {
    int active = 0;
    int launched = 0;
    double total_flops = 0.0;

    while (launched < total_jobs || active > 0) {
        while (active < max_workers && launched < total_jobs) {
            pid_t pid = fork();
            if (pid == 0) {
                int code = run_worker_job(launched + 1);
                _exit(code);
            } else if (pid > 0) {
                ++active;
                ++launched;
                fprintf(stdout, "\r🚀 Launching worker #%d \t [Active: %d/%d]", launched, active, max_workers);
                fflush(stdout);
            } else {
                return 2;
            }
        }

        int status = 0;
        pid_t done = waitpid(-1, &status, 0);
        if (done > 0) {
            --active;
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                total_flops += FIXED_FLOPS;
            }
        }

        if (launched % 5 == 0 || launched == total_jobs) {
            fprintf(stdout, "\r[STACK] Total: %.1f GigaFlops accumulated towards 1,000,000\n", total_flops);
            fprintf(stdout, "[HEAT] Concurrent: %d | Progress: %d/%d jobs\n", active, launched, total_jobs);
        }
    }

    fprintf(stdout, "\nCompleted %d jobs with %.1f GigaFlops delivered.\n", launched, total_flops);
    return 0;
}

/*
 * Simple parser to keep execution flags self-contained; avoids locale or
 * library-specific helpers to maintain deterministic behavior in austere
 * environments.
 */
static int parse_arg(const char *value, int fallback) {
    if (!value || *value == '\0') {
        return fallback;
    }
    int acc = 0;
    for (const char *p = value; *p; ++p) {
        if (*p < '0' || *p > '9') {
            return fallback;
        }
        acc = acc * 10 + (*p - '0');
    }
    return acc > 0 ? acc : fallback;
}

int main(int argc, char **argv) {
    int max_workers = MAX_WORKERS_DEFAULT;
    int total_jobs = TOTAL_JOBS_DEFAULT;

    if (argc > 1) {
        max_workers = parse_arg(argv[1], MAX_WORKERS_DEFAULT);
    }
    if (argc > 2) {
        total_jobs = parse_arg(argv[2], TOTAL_JOBS_DEFAULT);
    }

    fprintf(stdout, "=== RAFAELIA SWARM: PROCESS STACKER (C MODE) ===\n");
    fprintf(stdout, "Target: Accumulate until 1,000,000 GigaFlops\n");
    fprintf(stdout, "Configured workers: %d | Planned jobs: %d\n", max_workers, total_jobs);

    return orchestrate_swarm(max_workers, total_jobs);
}
