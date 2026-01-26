#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>

/* Deterministic matrix-style aggregator for debug metrics. */
struct metric_row {
    double cpu;
    double mem;
    double io;
    double thr;
    int valid;
};

/* Aggregate accumulators keep finite values only. */
struct accum_state {
    double cpu_sum;
    double mem_sum;
    double io_sum;
    double thr_sum;
    double cpu_min;
    double mem_min;
    double io_min;
    double thr_min;
    double cpu_max;
    double mem_max;
    double io_max;
    double thr_max;
    int rows;
    int anomalies;
};

static const char *sample_matrix[] = {
    "# cpu,mem,io,thr",
    "15.0, 512, 40, 120",
    "78.0, 2048, 900, 350",
    "99.0, 3900, 1200, 600",
};

static int parse_double_field(const char *cursor, double *out, const char **next) {
    if (cursor == NULL || out == NULL || next == NULL) {
        return 0;
    }
    errno = 0;
    char *endptr = NULL;
    double value = strtod(cursor, &endptr);
    if (errno != 0 || endptr == cursor) {
        return 0;
    }
    if (!isfinite(value)) {
        return 0;
    }
    *out = value;
    *next = endptr;
    return 1;
}

/* Reset accumulator with neutral bounds. */
static void accum_init(struct accum_state *s) {
    s->cpu_sum = s->mem_sum = s->io_sum = s->thr_sum = 0.0;
    s->cpu_min = s->mem_min = s->io_min = s->thr_min = 1e18;
    s->cpu_max = s->mem_max = s->io_max = s->thr_max = -1e18;
    s->rows = 0;
    s->anomalies = 0;
}

/* Parse a CSV line with four scalar fields. */
static struct metric_row parse_line(const char *line) {
    struct metric_row row;
    row.cpu = row.mem = row.io = row.thr = 0.0;
    row.valid = 0;

    if (!line) {
        return row;
    }

    const char *cursor = line;
    const char *next = NULL;

    while (*cursor == ' ' || *cursor == '\t' || *cursor == ',') {
        cursor++;
    }

    if (!parse_double_field(cursor, &row.cpu, &next)) {
        return row;
    }
    cursor = next;

    while (*cursor == ' ' || *cursor == '\t' || *cursor == ',') {
        cursor++;
    }

    if (!parse_double_field(cursor, &row.mem, &next)) {
        return row;
    }
    cursor = next;

    while (*cursor == ' ' || *cursor == '\t' || *cursor == ',') {
        cursor++;
    }

    if (!parse_double_field(cursor, &row.io, &next)) {
        return row;
    }
    cursor = next;

    while (*cursor == ' ' || *cursor == '\t' || *cursor == ',') {
        cursor++;
    }

    if (!parse_double_field(cursor, &row.thr, &next)) {
        return row;
    }

    row.valid = 1;
    return row;
}

/* Clamp extreme values to keep stability. */
static double finite_guard(double v) {
    if (!isfinite(v)) {
        return 0.0;
    }
    if (v > 1e9) {
        return 1e9;
    }
    if (v < -1e9) {
        return -1e9;
    }
    return v;
}

/* Integrate a parsed row into accumulators. */
static void accum_push(struct accum_state *s, const struct metric_row *r) {
    if (!r || !s || !r->valid) {
        return;
    }

    double cpu = finite_guard(r->cpu);
    double mem = finite_guard(r->mem);
    double io = finite_guard(r->io);
    double thr = finite_guard(r->thr);

    s->cpu_sum += cpu;
    s->mem_sum += mem;
    s->io_sum += io;
    s->thr_sum += thr;

    if (cpu < s->cpu_min) s->cpu_min = cpu;
    if (mem < s->mem_min) s->mem_min = mem;
    if (io < s->io_min) s->io_min = io;
    if (thr < s->thr_min) s->thr_min = thr;

    if (cpu > s->cpu_max) s->cpu_max = cpu;
    if (mem > s->mem_max) s->mem_max = mem;
    if (io > s->io_max) s->io_max = io;
    if (thr > s->thr_max) s->thr_max = thr;

    s->rows += 1;

    if (cpu >= 95.0 || mem >= 8192.0 || io >= 5000.0 || thr >= 1024.0) {
        s->anomalies += 1;
    }
}

/* Print a compact JSON-like summary for debug consoles. */
static void emit_summary(const struct accum_state *s) {
    if (!s || s->rows == 0) {
        printf("{\"rows\":0}\n");
        return;
    }

    double rows = (double)s->rows;
    double cpu_avg = s->cpu_sum / rows;
    double mem_avg = s->mem_sum / rows;
    double io_avg = s->io_sum / rows;
    double thr_avg = s->thr_sum / rows;

    printf("{\"rows\":%d,", s->rows);
    printf("\"cpu\":{\"avg\":%.3f,\"min\":%.3f,\"max\":%.3f},", cpu_avg, s->cpu_min, s->cpu_max);
    printf("\"mem\":{\"avg\":%.3f,\"min\":%.3f,\"max\":%.3f},", mem_avg, s->mem_min, s->mem_max);
    printf("\"io\":{\"avg\":%.3f,\"min\":%.3f,\"max\":%.3f},", io_avg, s->io_min, s->io_max);
    printf("\"thr\":{\"avg\":%.3f,\"min\":%.3f,\"max\":%.3f},", thr_avg, s->thr_min, s->thr_max);
    printf("\"anomalies\":%d}\n", s->anomalies);
}

/* Ingest stdin first; if empty, fall back to internal sample matrix. */
static int process_stream(void) {
    struct accum_state acc;
    accum_init(&acc);

    char line[256];
    int saw_input = 0;
    int tty = isatty(fileno(stdin));

    if (!tty) {
        while (fgets(line, sizeof(line), stdin)) {
            struct metric_row row = parse_line(line);
            if (!row.valid) {
                continue;
            }
            saw_input = 1;
            accum_push(&acc, &row);
        }
    }

    if (!saw_input) {
        size_t i;
        for (i = 0; i < sizeof(sample_matrix)/sizeof(sample_matrix[0]); ++i) {
            struct metric_row row = parse_line(sample_matrix[i]);
            if (!row.valid) {
                continue;
            }
            accum_push(&acc, &row);
        }
    }

    emit_summary(&acc);
    return (acc.anomalies > 0) ? 1 : 0;
}

int main(void) {
    return process_stream();
}
