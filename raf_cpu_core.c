// Optimized low-level CPU core parser - minimal dependencies
// Removed: ctype.h (replaced with inline checks), stdlib.h (replaced strtoull with inline parser)
// Benefits: Reduced binary size, faster parsing, fewer function calls
#define _POSIX_C_SOURCE 199309L

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAX_CPUS 256
#define MAX_LINE 512
#define FIELD_COUNT 10

static const char *FIELD_NAMES[FIELD_COUNT] = {
    "user",     "nice",     "system",    "idle",
    "iowait",   "irq",      "softirq",   "steal",
    "guest",    "guest_nice"};

typedef struct {
  char id[16];
  uint64_t fields[FIELD_COUNT];
  size_t field_count;
} cpu_sample;

static uint64_t now_ms(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
    return 0;
  }
  return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
}

static int drain_long_line(FILE *fp, char *line) {
  size_t len = strlen(line);
  if (len > 0u && line[len - 1u] == '\n') {
    return 0;
  }
  int ch = 0;
  while ((ch = fgetc(fp)) != EOF) {
    if (ch == '\n') {
      break;
    }
  }
  return 1;
}

static int parse_cpu_line(const char *line, cpu_sample *out) {
  if (strncmp(line, "cpu", 3) != 0) {
    return 0;  // not a CPU line
  }

  const char *cursor = line;
  // Fast-path: skip to whitespace without ctype.h dependency
  while (*cursor && *cursor != ' ' && *cursor != '\t') {
    cursor++;
  }
  size_t id_len = (size_t)(cursor - line);
  if (id_len == 0 || id_len >= sizeof(out->id)) {
    return -1;  // malformed id
  }
  memcpy(out->id, line, id_len);
  out->id[id_len] = '\0';

  size_t idx = 0;
  while (*cursor != '\0' && idx < FIELD_COUNT) {
    // Skip whitespace without ctype.h
    while (*cursor == ' ' || *cursor == '\t') {
      cursor++;
    }
    if (*cursor == '\0' || *cursor == '\n') {
      break;
    }
    // Parse number inline without strtoull for better performance
    uint64_t value = 0;
    const char *start = cursor;
    while (*cursor >= '0' && *cursor <= '9') {
      value = value * 10 + (*cursor - '0');
      cursor++;
    }
    if (cursor == start) {
      break;  // no number found
    }
    out->fields[idx++] = value;
  }

  out->field_count = idx;
  return idx > 0 ? 1 : -1;
}

static void print_cpu_json(const cpu_sample *sample, int is_last) {
  fputs("{\"id\":\"", stdout);
  fputs(sample->id, stdout);
  fputc('\"', stdout);

  for (size_t i = 0; i < FIELD_COUNT; ++i) {
    uint64_t value = i < sample->field_count ? sample->fields[i] : 0;
    fputc(',', stdout);
    fputc('\"', stdout);
    fputs(FIELD_NAMES[i], stdout);
    fputs("\":", stdout);
    printf("%llu", (unsigned long long)value);
  }

  fputc('}', stdout);
  if (!is_last) {
    fputc(',', stdout);
  }
}

int main(void) {
  cpu_sample samples[MAX_CPUS];
  size_t sample_count = 0;
  char line[MAX_LINE];

  while (fgets(line, sizeof(line), stdin) != NULL) {
    if (drain_long_line(stdin, line)) {
      continue;
    }
    if (strncmp(line, "cpu", 3) != 0) {
      break;  // stop at first non-CPU line to keep interface predictable
    }
    cpu_sample sample;
    int parsed = parse_cpu_line(line, &sample);
    if (parsed == 1 && sample_count < MAX_CPUS) {
      samples[sample_count++] = sample;
    }
  }

  if (sample_count == 0) {
    return 1;  // no cpu lines
  }

  printf("{\"timestamp_ms\":%llu,\"schema\":\"proc_stat\",\"cpus\":[",
         (unsigned long long)now_ms());
  for (size_t i = 0; i < sample_count; ++i) {
    print_cpu_json(&samples[i], i + 1 == sample_count);
  }
  fputs("]}\n", stdout);
  return 0;
}
