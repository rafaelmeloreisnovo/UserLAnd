// Optimized low-level memory info parser - minimal dependencies
// Removed: stdlib.h (replaced strtoull with inline parser)
// Benefits: Reduced binary size, faster parsing, fewer function calls
#define _POSIX_C_SOURCE 199309L

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAX_MEM_FIELDS 64
#define MAX_KEY_LEN 32
#define MAX_LINE 256

typedef struct {
  char key[MAX_KEY_LEN];
  uint64_t value;
} mem_field;

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

static int parse_meminfo_line(const char *line, mem_field *out) {
  const char *colon = strchr(line, ':');
  if (colon == NULL) {
    return 0;
  }
  size_t key_len = (size_t)(colon - line);
  while (key_len > 0 && (line[key_len - 1] == ' ' || line[key_len - 1] == '\t')) {
    key_len--;
  }
  if (key_len == 0 || key_len >= MAX_KEY_LEN) {
    return 0;
  }
  memcpy(out->key, line, key_len);
  out->key[key_len] = '\0';

  const char *cursor = colon + 1;
  while (*cursor == ' ' || *cursor == '\t') {
    cursor++;
  }
  // Parse number inline without strtoull for better performance
  uint64_t value = 0;
  const char *start = cursor;
  while (*cursor >= '0' && *cursor <= '9') {
    value = value * 10 + (*cursor - '0');
    cursor++;
  }
  if (cursor == start) {
    return 0;
  }

  out->value = value;
  return 1;
}

static void print_meminfo_json(const mem_field *fields, size_t count) {
  fputs("{\"timestamp_ms\":", stdout);
  printf("%llu,\"meminfo_unit\":\"kB\",\"meminfo\":{", (unsigned long long)now_ms());

  for (size_t i = 0; i < count; ++i) {
    if (i > 0) {
      fputc(',', stdout);
    }
    fputc('\"', stdout);
    fputs(fields[i].key, stdout);
    fputs("\":", stdout);
    printf("%llu", (unsigned long long)fields[i].value);
  }

  fputs("}}\n", stdout);
}

int main(void) {
  FILE *fp = fopen("/proc/meminfo", "r");
  if (fp == NULL) {
    return 1;
  }

  mem_field fields[MAX_MEM_FIELDS];
  size_t count = 0;
  char line[MAX_LINE];

  while (fgets(line, sizeof(line), fp) != NULL && count < MAX_MEM_FIELDS) {
    if (drain_long_line(fp, line)) {
      continue;
    }
    mem_field field;
    if (parse_meminfo_line(line, &field)) {
      fields[count++] = field;
    }
  }

  fclose(fp);

  if (count == 0) {
    return 1;
  }

  print_meminfo_json(fields, count);
  return 0;
}
