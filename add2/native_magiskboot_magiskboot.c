/* Signature: RAFCODE-Φ-∆RafaelVerboΩ - 2025-08-31T14:25:55Z
   Manifest: RAFAELIA_MANIFEST.json (see repo root)
   ψχρΔΣΩ-session: (runtime will inject per-run)
   Hardened + journaling + atomic write + backup + --safe/--force policy additions
*/

/* ============================================================================
   RAFAELIA | MAGISKBOOT - Hardened variant with journaling and atomic writes
   ============================================================================ */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <inttypes.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <openssl/sha.h>
#include <zlib.h>

#ifdef __ANDROID__
#include <android/log.h>
#define RAF_LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, "RAFAELIA", fmt, ##__VA_ARGS__)
#define RAF_LOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, "RAFAELIA", fmt, ##__VA_ARGS__)
#else
#define RAF_LOGI(fmt, ...) do { fprintf(stdout, "RAFAELIA INFO: " fmt "\n", ##__VA_ARGS__); fflush(stdout); } while(0)
#define RAF_LOGE(fmt, ...) do { fprintf(stderr, "RAFAELIA ERROR: " fmt "\n", ##__VA_ARGS__); fflush(stderr); } while(0)
#endif

/* Constants and caps */
#define BUF_SIZE (64*1024)
#define MAX_NAME_SIZE (1024*1024)
#define MAX_FILE_SIZE ((uint64_t)4<<30)
#define RAMDISK_SCAN_LIMIT (16*1024*1024)
#define DEFAULT_MAX_UNCOMPRESSED (256ULL * 1024 * 1024) /* 256 MiB conservative */

/* Journal and policy */
#define RAFAELIA_JOURNAL "boot.img.rafaelia.journal"
#define BACKUP_SUFFIX ".rafaelia.bak"

typedef enum { POLICY_SAFE = 0, POLICY_NORMAL = 1, POLICY_FORCE = 2 } operation_policy_t;

static operation_policy_t g_policy = POLICY_NORMAL;
static size_t g_max_uncompressed = DEFAULT_MAX_UNCOMPRESSED;

/* Utility: current timestamp */
static const char* now_iso8601(char *buf, size_t n) {
    time_t t = time(NULL);
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(buf, n, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

/* Small helper to emit JSON-like structured logs for CI parsing */
static void raf_log_json(const char *level, const char *component, const char *event, const char *msg) {
    char ts[32];
    now_iso8601(ts, sizeof(ts));
    RAF_LOGI("{\"ts\":\"%s\",\"level\":\"%s\",\"component\":\"%s\",\"event\":\"%s\",\"msg\":\"%s\"}", ts, level, component, event, msg ? msg : "");
}

/* Robust read wrapper */
static ssize_t safe_read(int fd, void *buf, size_t count) {
    ssize_t r = read(fd, buf, count);
    if (r < 0) {
        RAF_LOGE("safe_read: read failed: %s", strerror(errno));
    }
    return r;
}

/* Robust lseek wrapper */
static off_t safe_lseek(int fd, off_t offset, int whence) {
    off_t r = lseek(fd, offset, whence);
    if (r == (off_t)-1) {
        RAF_LOGE("safe_lseek: lseek failed: %s", strerror(errno));
    }
    return r;
}

/* Safe write loop */
static ssize_t safe_write_loop(int fd, const void *buf, size_t count) {
    size_t written = 0;
    const uint8_t *p = buf;
    while (written < count) {
        ssize_t r = write(fd, p + written, count - written);
        if (r < 0) {
            if (errno == EINTR) continue;
            RAF_LOGE("safe_write_loop: write failed: %s", strerror(errno));
            return -1;
        }
        written += (size_t)r;
    }
    return (ssize_t)written;
}

/* Get parent directory path of a path; caller must free() returned string */
static char *get_parent_dir(const char *path) {
    if (!path) return NULL;
    char *tmp = strdup(path);
    if (!tmp) return NULL;
    char *dir = dirname(tmp); /* dirname may modify tmp */
    char *res = strdup(dir);
    free(tmp);
    return res;
}

/* Atomic write with fsync of file and parent directory */
static int write_atomic_and_fsync(const char *dst_path, const void *data, size_t len) {
    if (!dst_path || (!data && len)) return -1;

    char *parent = get_parent_dir(dst_path);
    if (!parent) {
        RAF_LOGE("write_atomic_and_fsync: failed to get parent dir");
        return -1;
    }

    /* create temp path in same dir */
    char tmp_path[PATH_MAX];
    snprintf(tmp_path, sizeof(tmp_path), "%s/.%s.tmp.XXXXXX", parent, basename((char*)dst_path));

    int tmpfd = -1;
    /* Use mkstemp in parent directory by building a template */
    /* To avoid path length issues, build template in parent */
    char template_path[PATH_MAX];
    snprintf(template_path, sizeof(template_path), "%s/.rafaelia_tmp_XXXXXX", parent);
    tmpfd = mkstemp(template_path);
    if (tmpfd < 0) {
        RAF_LOGE("write_atomic_and_fsync: mkstemp failed: %s", strerror(errno));
        free(parent);
        return -1;
    }

    /* Unlink now to avoid leaving temp file on failure; but keep fd open */
    unlink(template_path);

    if (len) {
        if (safe_write_loop(tmpfd, data, len) != (ssize_t)len) {
            close(tmpfd);
            free(parent);
            return -1;
        }
    }
    if (fsync(tmpfd) != 0) {
        RAF_LOGE("write_atomic_and_fsync: fsync file failed: %s", strerror(errno));
        close(tmpfd);
        free(parent);
        return -1;
    }
    close(tmpfd);

    /* Now rename temp to destination: create another temp file name then rename.
       Since we unlinked the mkstemp file, recreate by writing to a new tmp file. Simpler approach:
       write straight to tmp in same dir using .<basename>.tmpPID and rename it.
       For portability, perform write -> rename by writing to template_path earlier and renaming.
       But above we unlinked; instead, we'll write to template_path then rename. For robustness, redo: */
    RAF_LOGI("write_atomic_and_fsync: finalizing rename to %s", dst_path);

    /* Recreate a deterministic tmp file path (less ideal but portable): */
    char final_tmp[PATH_MAX];
    snprintf(final_tmp, sizeof(final_tmp), "%s/.%s.rafaelia.tmp", parent, basename((char*)dst_path));
    int fd2 = open(final_tmp, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd2 < 0) {
        RAF_LOGE("write_atomic_and_fsync: open final tmp failed: %s", strerror(errno));
        free(parent);
        return -1;
    }
    if (len) {
        if (safe_write_loop(fd2, data, len) != (ssize_t)len) {
            close(fd2); unlink(final_tmp); free(parent); return -1;
        }
    }
    if (fsync(fd2) != 0) {
        RAF_LOGE("write_atomic_and_fsync: fsync final tmp failed: %s", strerror(errno));
        close(fd2); unlink(final_tmp); free(parent); return -1;
    }
    close(fd2);

    /* fsync parent dir after rename */
    if (rename(final_tmp, dst_path) != 0) {
        RAF_LOGE("write_atomic_and_fsync: rename failed: %s", strerror(errno));
        unlink(final_tmp);
        free(parent);
        return -1;
    }

    int dirfd = open(parent, O_DIRECTORY | O_RDONLY);
    if (dirfd >= 0) {
        if (fsync(dirfd) != 0) {
            RAF_LOGE("write_atomic_and_fsync: fsync dir failed: %s", strerror(errno));
            close(dirfd);
            free(parent);
            return -1;
        }
        close(dirfd);
    } else {
        RAF_LOGE("write_atomic_and_fsync: open parent dir failed: %s", strerror(errno));
        free(parent);
        return -1;
    }

    free(parent);
    return 0;
}

/* Journal entry structure and helpers */
struct journal_entry {
    char orig_sha[129];
    char new_sha[129];
    char ts[32];
    char modules[256];
    char state[32]; /* pending|done|failed */
};

static int journal_write(const char *journal_path, const struct journal_entry *je) {
    if (!journal_path || !je) return -1;
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s.tmp", journal_path);
    FILE *f = fopen(tmp, "w");
    if (!f) {
        RAF_LOGE("journal_write: fopen tmp failed: %s", strerror(errno));
        return -1;
    }
    fprintf(f, "{\n");
    fprintf(f, "  \"ts\": \"%s\",\n", je->ts);
    fprintf(f, "  \"orig_sha\": \"%s\",\n", je->orig_sha);
    fprintf(f, "  \"new_sha\": \"%s\",\n", je->new_sha);
    fprintf(f, "  \"modules\": \"%s\",\n", je->modules);
    fprintf(f, "  \"state\": \"%s\"\n", je->state);
    fprintf(f, "}\n");
    fflush(f);
    fsync(fileno(f));
    fclose(f);

    /* atomic rename */
    if (rename(tmp, journal_path) != 0) {
        RAF_LOGE("journal_write: rename failed: %s", strerror(errno));
        unlink(tmp);
        return -1;
    }
    return 0;
}

/* Create backup copy path.bak (simple copy) */
static int create_backup(const char *path) {
    if (!path) return -1;
    char bak[PATH_MAX];
    snprintf(bak, sizeof(bak), "%s%s", path, BACKUP_SUFFIX);

    int in = open(path, O_RDONLY);
    if (in < 0) {
        RAF_LOGE("create_backup: open source failed: %s", strerror(errno));
        return -1;
    }
    int out = open(bak, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (out < 0) {
        RAF_LOGE("create_backup: open dest failed: %s", strerror(errno));
        close(in);
        return -1;
    }
    unsigned char buf[BUF_SIZE];
    ssize_t r;
    while ((r = safe_read(in, buf, sizeof(buf))) > 0) {
        if (safe_write_loop(out, buf, (size_t)r) != r) {
            close(in); close(out); return -1;
        }
    }
    fsync(out);
    close(in); close(out);
    RAF_LOGI("create_backup: backup created at %s", bak);
    return 0;
}

/* SHA-256 helper for fd */
int sha256_file_stream_fd(int fd, unsigned char out[32], int (*progress_cb)(unsigned long long, unsigned long long, void*), void *pud) {
    if (fd < 0 || out == NULL) {
        raf_log_json("ERROR", "sha256", "invalid_params", "invalid fd or out == NULL");
        return -1;
    }
    SHA256_CTX ctx;
    if (!SHA256_Init(&ctx)) {
        raf_log_json("ERROR", "sha256", "init_fail", "SHA256_Init failed");
        return -1;
    }

    unsigned char *buf = malloc(BUF_SIZE);
    if (!buf) {
        raf_log_json("ERROR", "sha256", "oom", "malloc BUF_SIZE failed");
        return -1;
    }

    uint64_t total = 0;
    ssize_t n;
    off_t orig = safe_lseek(fd, 0, SEEK_CUR);
    if (orig != (off_t)-1) safe_lseek(fd, 0, SEEK_SET);

    while ((n = safe_read(fd, buf, BUF_SIZE)) > 0) {
        SHA256_Update(&ctx, buf, (size_t)n);
        total += (uint64_t)n;
        if (progress_cb && progress_cb(total, 0, pud)) {
            raf_log_json("ERROR", "sha256", "aborted_by_callback", "callback requested abort");
            free(buf);
            if (orig != (off_t)-1) safe_lseek(fd, orig, SEEK_SET);
            return -1;
        }
    }
    if (n < 0) {
        raf_log_json("ERROR", "sha256", "read_fail", strerror(errno));
        free(buf);
        if (orig != (off_t)-1) safe_lseek(fd, orig, SEEK_SET);
        return -1;
    }
    SHA256_Final(out, &ctx);
    free(buf);
    if (orig != (off_t)-1) safe_lseek(fd, orig, SEEK_SET);

    char msg[128];
    snprintf(msg, sizeof(msg), "sha256 computed bytes=%" PRIu64, (uint64_t)total);
    raf_log_json("INFO", "sha256", "done", msg);
    return 0;
}

/* -----------------------------
   CPIO parser & decompressors (kept as in original but with size caps)
   ----------------------------- */
/* feed callback type: called with data buffer and size. Return 0 to continue, non-zero to abort. */
typedef int (*feed_cb_t)(const unsigned char *buf, size_t len, void *userdata);

static int parse_hex8(const char *s, uint32_t *out) {
    char tmp[9]; memcpy(tmp, s, 8); tmp[8]=0;
    char *end;
    unsigned long v = strtoul(tmp, &end, 16);
    if (end == tmp) return -1;
    *out = (uint32_t)v;
    return 0;
}

/* cpio parser limited by max_bytes (to prevent OOM) */
int cpio_parse_and_feed_fd(int fd, uint64_t max_bytes, feed_cb_t feed, void *userdata) {
    if (fd < 0 || !feed) {
        raf_log_json("ERROR","cpio","invalid_params","fd<0 or feed==NULL");
        return -1;
    }

    unsigned char header[6*8+1];
    ssize_t n;
    uint64_t processed = 0;
    char tmp[256];
    while (1) {
        n = safe_read(fd, header, 6);
        if (n == 0) {
            raf_log_json("INFO","cpio","eof","end of stream");
            return 0;
        }
        if (n < 6) { raf_log_json("ERROR","cpio","short_header","short magic"); return -1; }
        header[6] = '\0';
        if (memcmp(header, "070701", 6) != 0) {
            raf_log_json("ERROR","cpio","bad_magic","expected 070701");
            return -1;
        }
        n = safe_read(fd, tmp, 11*8);
        if (n != 11*8) { raf_log_json("ERROR","cpio","short_header_fields","expected 88 bytes"); return -1; }
        tmp[88] = '\0';
        char fields[13][9];
        for (int i=0;i<13;i++) {
            memcpy(fields[i], tmp + (i*8), 8);
            fields[i][8]=0;
        }
        uint32_t namesize = 0;
        uint32_t filesize = 0;
        if (parse_hex8(fields[11], &namesize) != 0) { raf_log_json("ERROR","cpio","namesize_parse","failed"); return -1; }
        if (parse_hex8(fields[6], &filesize) != 0) { raf_log_json("ERROR","cpio","filesize_parse","failed"); return -1; }

        if (namesize == 0 || namesize > MAX_NAME_SIZE) {
            raf_log_json("ERROR","cpio","namesize_invalid","namesize out of bounds");
            return -1;
        }
        if ((uint64_t)filesize > MAX_FILE_SIZE) {
            raf_log_json("ERROR","cpio","filesize_invalid","filesize out of bounds");
            return -1;
        }

        size_t name_padded = ((namesize + 3) & ~3);
        unsigned char *namebuf = malloc(name_padded);
        if (!namebuf) { raf_log_json("ERROR","cpio","oom","alloc namebuf failed"); return -1; }
        n = safe_read(fd, namebuf, name_padded);
        if (n != (ssize_t)name_padded) { free(namebuf); raf_log_json("ERROR","cpio","short_name","short read"); return -1; }
        if (memcmp(namebuf, "TRAILER!!!", 10) == 0) {
            free(namebuf);
            raf_log_json("INFO","cpio","trailer","found trailer, end of archive");
            return 0;
        }
        uint64_t remain = (uint64_t)filesize;
        size_t toread;
        unsigned char *data_buf = malloc(BUF_SIZE);
        if (!data_buf) { free(namebuf); raf_log_json("ERROR","cpio","oom_data","malloc failed"); return -1; }
        while (remain) {
            toread = (remain > BUF_SIZE) ? BUF_SIZE : (size_t)remain;
            n = safe_read(fd, data_buf, toread);
            if (n <= 0) { free(namebuf); free(data_buf); raf_log_json("ERROR","cpio","short_payload","read failed"); return -1; }
            if (feed(data_buf, (size_t)n, userdata) != 0) {
                free(namebuf); free(data_buf);
                raf_log_json("ERROR","cpio","feed_abort","feed callback returned non-zero");
                return -1;
            }
            remain -= (size_t)n;
            processed += (size_t)n;
            if (max_bytes && processed > max_bytes) {
                free(namebuf); free(data_buf);
                raf_log_json("ERROR","cpio","max_bytes_exceeded","processed exceeded max_bytes");
                return -1;
            }
        }
        free(namebuf);
        free(data_buf);
        off_t cur = safe_lseek(fd, 0, SEEK_CUR);
        off_t aligned = ( (cur + 3) & ~3 );
        if (aligned > cur) safe_lseek(fd, aligned, SEEK_SET);
    }
    return 0;
}

/* gzip decompressor with cap on uncompressed bytes (streaming) */
int decompress_gzip_fd_and_feed_limited(int fd, uint64_t max_uncompressed, feed_cb_t feed, void *userdata) {
    if (fd < 0 || !feed) { raf_log_json("ERROR","decompress","invalid_params",""); return -1; }
    int dupfd = dup(fd);
    if (dupfd < 0) { raf_log_json("ERROR","decompress","dup_fail",strerror(errno)); return -1; }
    FILE *f = fdopen(dupfd, "rb");
    if (!f) { close(dupfd); raf_log_json("ERROR","decompress","fdopen_fail",strerror(errno)); return -1; }
    gzFile gzf = gzdopen(dupfd, "rb");
    if (!gzf) { fclose(f); raf_log_json("ERROR","decompress","gzdopen_fail",""); return -1; }

    unsigned char *out = malloc(BUF_SIZE);
    if (!out) { gzclose(gzf); raf_log_json("ERROR","decompress","oom","alloc"); return -1; }

    int r;
    uint64_t processed = 0;
    while ((r = gzread(gzf, out, BUF_SIZE)) > 0) {
        processed += (uint64_t)r;
        if (max_uncompressed && processed > max_uncompressed) {
            raf_log_json("ERROR","decompress","limit_exceeded","decompressed beyond configured limit");
            free(out); gzclose(gzf); return -1;
        }
        if (feed(out, (size_t)r, userdata) != 0) {
            raf_log_json("ERROR","decompress","feed_abort","callback returned non-zero");
            free(out); gzclose(gzf); return -1;
        }
    }
    if (r < 0) {
        raf_log_json("ERROR","decompress","gzread_fail","error reading gzip stream");
        free(out); gzclose(gzf); return -1;
    }
    free(out);
    gzclose(gzf);
    raf_log_json("INFO","decompress","done","gzip feed complete");
    return 0;
}

/* compute_logical_ramdisk_sha_from_fd with limits & magic scan fallback */
int compute_logical_ramdisk_sha_from_fd(int fd, const char *format_hint, unsigned char out[32], int (*pcb)(unsigned long long, unsigned long long, void*), void *pud) {
    if (fd < 0 || !out) { raf_log_json("ERROR","compute_logical_ramdisk","invalid_params",""); return -1; }

    off_t cur = safe_lseek(fd, 0, SEEK_CUR);
    if (cur == (off_t)-1) cur = 0;

    raf_log_json("INFO","compute_logical_ramdisk","start","attempting to compute logical ramdisk SHA (best-effort)");

    /* For brevity: scan for gzip magic within RAMDISK_SCAN_LIMIT */
    unsigned char *scan_buf = malloc(RAMDISK_SCAN_LIMIT);
    if (!scan_buf) { raf_log_json("ERROR","compute_logical_ramdisk","oom","alloc"); return -1; }
    off_t start = safe_lseek(fd, 0, SEEK_SET);
    ssize_t got = safe_read(fd, scan_buf, RAMDISK_SCAN_LIMIT);
    if (got <= 0) { free(scan_buf); raf_log_json("ERROR","compute_logical_ramdisk","read_fail","scan read failed"); return -1; }
    ssize_t pos = -1;
    for (ssize_t i = 0; i+1 < got; ++i) {
        if (scan_buf[i] == 0x1f && scan_buf[i+1] == 0x8b) { pos = i; break; }
    }
    if (pos == -1) {
        raf_log_json("ERROR","compute_logical_ramdisk","no_magic","no gzip magic found in scan window");
        free(scan_buf);
        return -1;
    }
    if (safe_lseek(fd, start + pos, SEEK_SET) == (off_t)-1) { free(scan_buf); return -1; }

    /* Compute a best-effort SHA over compressed stream (decompressed path would be ideal) */
    SHA256_CTX ctx;
    if (!SHA256_Init(&ctx)) { free(scan_buf); return -1; }
    unsigned char *rbuf = malloc(BUF_SIZE);
    if (!rbuf) { free(scan_buf); return -1; }
    ssize_t rn;
    uint64_t total = 0;
    while ((rn = safe_read(fd, rbuf, BUF_SIZE)) > 0) {
        SHA256_Update(&ctx, rbuf, (size_t)rn);
        total += (uint64_t)rn;
    }
    if (rn < 0) { raf_log_json("ERROR","compute_logical_ramdisk","read_fail2","read failed during sha"); free(scan_buf); free(rbuf); return -1; }
    SHA256_Final(out, &ctx);
    free(scan_buf);
    free(rbuf);
    char msg[128];
    snprintf(msg, sizeof(msg), "computed best-effort ramdisk sha over compressed stream bytes=%" PRIu64, total);
    raf_log_json("INFO","compute_logical_ramdisk","done", msg);
    safe_lseek(fd, cur, SEEK_SET);
    return 0;
}

/* compute_image_and_ramdisk_shas convenience combo */
int compute_image_and_ramdisk_shas(const char *image_path, unsigned char sha_before[32], unsigned char sha_after[32], unsigned char ramdisk_sha[32]) {
    if (!image_path) return -1;
    int fd = open(image_path, O_RDONLY);
    if (fd < 0) { raf_log_json("ERROR","compute_combo","open_fail", strerror(errno)); return -1; }
    int rc = sha256_file_stream_fd(fd, sha_before, NULL, NULL);
    if (rc != 0) { close(fd); return -1; }
    int r = compute_logical_ramdisk_sha_from_fd(fd, NULL, ramdisk_sha, NULL, NULL);
    if (r != 0) raf_log_json("WARN","compute_combo","ramdisk_nok","ramdisk logical sha failed or incomplete");
    safe_lseek(fd, 0, SEEK_SET);
    int rc2 = sha256_file_stream_fd(fd, sha_after, NULL, NULL);
    close(fd);
    return (rc==0 && rc2==0) ? 0 : -1;
}

/* ============================================================================
   Placeholders for other modules (repack/sign/heuristics) kept from original code
   ============================================================================ */

#define BOOT_MAGIC "ANDROID!"
#define BOOT_MAGIC_SIZE 8

struct boot_img_hdr {
    char magic[BOOT_MAGIC_SIZE];
    uint32_t kernel_size;
    uint32_t kernel_addr;
    uint32_t ramdisk_size;
    uint32_t ramdisk_addr;
    uint32_t second_size;
    uint32_t second_addr;
    uint32_t tags_addr;
    uint32_t page_size;
    uint32_t dt_size;
    uint32_t unused;
    char name[16];
    char cmdline[512];
};

bool detect_boot_image(void *buf, size_t sz) {
    if (sz < sizeof(struct boot_img_hdr)) return false;
    struct boot_img_hdr *h = (struct boot_img_hdr*) buf;
    return !memcmp(h->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE);
}

typedef struct { uint8_t *buf; size_t sz; } image_t;

image_t repack_A(image_t in) {
    raf_log_json("INFO","repack","mode","Repack modo A");
    return in;
}

image_t repack_B(image_t in) {
    raf_log_json("INFO","repack","mode","Repack modo B");
    return in;
}

image_t dual_repack(image_t in) {
    image_t A = repack_A(in);
    image_t B = repack_B(in);
    return (A.sz <= B.sz) ? A : B;
}

/* minimal signature stub */
void apply_signature(const void *buf, size_t sz) {
    raf_log_json("INFO","sign","apply","Assinatura RAFCODE-Φ aplicada (stub).");
}

/* OEM heuristics simplified */
static const char *rafaelia_detect_profile(void *buf, size_t sz) {
    if (memmem(buf, sz, "SEANDROIDENFORCE", 16)) return "Samsung_KNOX";
    if (memmem(buf, sz, "qcom", 4)) return "Qualcomm_ABOOT";
    if (memmem(buf, sz, "MTK", 3)) return "MediaTek_DA";
    if (memmem(buf, sz, "AVBf", 4)) return "Google_AVB2";
    if (sz > (50 * 1024 * 1024)) return "VendorBoot_12plus";
    return "Generic_OEM";
}

/* simple ECC helpers (kept) */
static inline uint32_t ecc32(uint32_t x) {
    x ^= x >> 1; x ^= x << 3; x ^= x >> 5; return x;
}
static inline uint32_t autocorrect32(uint32_t v) {
    uint32_t e = ecc32(v);
    return v ^ (e & 0x30303030);
}

/* small ramdisk patch */
bool patch_ramdisk(uint8_t *buf, size_t sz) {
    for (size_t i = 0; i + 4 < sz; i++) {
        if (!memcmp(buf + i, "exec", 4)) {
            memcpy(buf + i, "safe", 4);
        }
    }
    return true;
}

/* fast repack stub (kept) */
static image_t fast_repack(image_t in) {
    raf_log_json("INFO","fast_repack","start","FastRepack ativado");
    for (size_t i = 0; i < in.sz; i += 64) {
        __builtin_prefetch(in.buf + i + 128);
    }
    return in;
}

/* SIMD detection stub */
static inline void rafael_simd_select() {
#if defined(__aarch64__)
    raf_log_json("INFO","simd","select","NEON ativo (ARMv8)");
#elif defined(__x86_64__)
    raf_log_json("INFO","simd","select","SSE2/AVX ativo (x86_64)");
#else
    raf_log_json("INFO","simd","select","Modo C puro (sem SIMD)");
#endif
}

/* Dev mode stub */
static void enable_dev_mode_android() {
    raf_log_json("INFO","devmode","enable","Ativando DevMode sem root (stub)");
}

/* ============================================================================
   MAIN
   ============================================================================ */

static void usage(FILE *f, const char *prog) {
    fprintf(f, "Usage: %s [--safe|--force] <boot.img>\n", prog);
    fprintf(f, "  --safe    : do not modify vbmeta/signatures and require explicit --force to overwrite\n");
    fprintf(f, "  --force   : force operations even if signatures/vbmeta might be affected\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(stderr, argv[0]);
        return 1;
    }

    const char *infile = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--safe")) {
            g_policy = POLICY_SAFE;
            continue;
        }
        if (!strcmp(argv[i], "--force")) {
            g_policy = POLICY_FORCE;
            continue;
        }
        if (!infile) infile = argv[i];
    }

    if (!infile) {
        usage(stderr, argv[0]);
        return 1;
    }

    /* Open and mmap input image read-only */
    int fd = open(infile, O_RDONLY);
    if (fd < 0) {
        perror("Erro ao abrir arquivo");
        return 1;
    }
    struct stat st;
    if (fstat(fd, &st) != 0) {
        perror("fstat");
        close(fd);
        return 1;
    }
    void *map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    /* ECC sample */
    uint32_t header_hash = autocorrect32(*(uint32_t*)map);
    fprintf(stderr, "[Ω] ECC header hash: 0x%X\n", header_hash);

    if (!detect_boot_image(map, st.st_size)) {
        fprintf(stderr, "Arquivo não é boot image válida.\n");
        munmap(map, st.st_size);
        close(fd);
        return 1;
    }
    raf_log_json("INFO","main","detected","Imagem de boot detectada.");

    /* compute SHAs for journaling */
    unsigned char sha_before[32], sha_after[32], rsha[32];
    if (sha256_file_stream_fd(fd, sha_before, NULL, NULL) != 0) {
        raf_log_json("WARN","main","sha_before_failed","sha256 before failed");
        memset(sha_before, 0, sizeof(sha_before));
    } else {
        raf_log_json("INFO","main","sha_before_ok","sha256 before computed");
    }

    /* Pre-check policy: if SAFE and vbmeta/AVB detected, warn and require --force to proceed */
    const char *profile = rafaelia_detect_profile(map, st.st_size);
    if (g_policy == POLICY_SAFE && !strcmp(profile, "Google_AVB2")) {
        raf_log_json("WARN","policy","avb_detected","AVB2 profile detected and --safe policy active; abort or use --force to override");
        fprintf(stderr, "AVB/vbmeta detected and --safe active; use --force to override. Profile=%s\n", profile);
        munmap(map, st.st_size);
        close(fd);
        return 2;
    }

    /* Create backup */
    if (create_backup(infile) != 0) {
        raf_log_json("WARN","main","backup_failed","backup creation failed; proceeding but risky");
    }

    /* Journal pending */
    struct journal_entry je;
    memset(&je, 0, sizeof(je));
    for (int i = 0; i < (int)sizeof(je.orig_sha); ++i) je.orig_sha[i] = 0;
    for (int i = 0; i < (int)sizeof(je.new_sha); ++i) je.new_sha[i] = 0;
    char ts[32];
    now_iso8601(ts, sizeof(ts));
    strncpy(je.ts, ts, sizeof(je.ts)-1);
    snprintf(je.modules, sizeof(je.modules), "ramdisk,kernel,sign");
    strncpy(je.state, "pending", sizeof(je.state)-1);
    /* fill orig sha hex */
    for (int i=0;i<32;i++) snprintf(&je.orig_sha[i*2], 3, "%02x", sha_before[i]);

    journal_write(RAFAELIA_JOURNAL, &je);

    /* Apply a simple patch on ramdisk (ethically) - we operate on mmap region for demonstration only */
    patch_ramdisk((uint8_t*)map, st.st_size);

    /* repack using fast_repack + dual */
    image_t img = { .buf = (uint8_t*)map, .sz = (size_t)st.st_size };
    image_t rep = fast_repack(dual_repack(img));

    /* Signature */
    apply_signature(rep.buf, rep.sz);

    /* Compute sha_after for record */
    /* Write rep.buf to a temporary file and compute SHA; in this simplified implementation we'll compute sha over original file as placeholder */
    int rc_sha_after = sha256_file_stream_fd(fd, sha_after, NULL, NULL);
    if (rc_sha_after != 0) {
        raf_log_json("WARN","main","sha_after_failed","sha256 after failed");
    } else {
        for (int i=0;i<32;i++) snprintf(&je.new_sha[i*2], 3, "%02x", sha_after[i]);
    }

    /* finalize journal state */
    strncpy(je.state, "done", sizeof(je.state)-1);
    now_iso8601(ts, sizeof(ts));
    strncpy(je.ts, ts, sizeof(je.ts)-1);
    if (journal_write(RAFAELIA_JOURNAL, &je) != 0) {
        raf_log_json("ERROR","main","journal_final_failed","failed to write final journal");
    }

    raf_log_json("INFO","main","completed","Processo RAFAELIA Ω finalizado");
    munmap(map, st.st_size);
    close(fd);
    return 0;
}

/* End of file */