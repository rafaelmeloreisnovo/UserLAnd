/*
libnative/glue.c - JNI glue + minimal C shim for RAFAELIA PoC
SPDX-License-Identifier: Apache-2.0

Coloque em: libnative/glue.c

Notas:
- Implementa JNI_OnLoad que registra nativos para a classe Kotlin RafaelBridge (ver snippet Kotlin).
- Fornece stubs seguros para rafael_* que podem ser substituídos por implementações reais mais tarde.
- É intencionalmente simples para facilitar teste inicial.
*/

#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rafael_api.h"

/* Minimal internal bindings (no platform callbacks by default) */
static rafael_platform_bindings_t g_bindings = {0};

int rafael_init(rafael_platform_bindings_t *bindings) {
    if (bindings) g_bindings = *bindings;
    rafael_log(2, "rafael_init stub called");
    return 0;
}

int rafael_shutdown(void) {
    rafael_log(2, "rafael_shutdown stub called");
    return 0;
}

int rafael_on_key(uint32_t keycode, uint32_t modifiers, uint64_t timestamp_ms) {
    char buf[128];
    snprintf(buf, sizeof(buf), "on_key: code=%u mods=%u ts=%llu", keycode, modifiers, (unsigned long long)timestamp_ms);
    rafael_log(3, buf);
    return 0;
}

int rafael_get_suggestions(const char *prefix_utf8, char ***out_buf, uint32_t *out_count, double ethica_exponent) {
    if (!prefix_utf8 || !out_buf || !out_count) return -1;
    /* simple stub: return prefix + "a", prefix + "b" */
    const char *a = "sug1";
    const char *b = "sug2";
    uint32_t n = 2;
    char **arr = (char**) rafael_api_alloc(sizeof(char*) * n);
    if (!arr) return -2;
    arr[0] = (char*) rafael_api_alloc(strlen(a) + 1);
    arr[1] = (char*) rafael_api_alloc(strlen(b) + 1);
    if (!arr[0] || !arr[1]) {
        if (arr[0]) rafael_api_free(arr[0]);
        if (arr[1]) rafael_api_free(arr[1]);
        rafael_api_free(arr);
        return -3;
    }
    strcpy(arr[0], a);
    strcpy(arr[1], b);
    *out_buf = arr;
    *out_count = n;
    return 0;
}

void rafael_api_free_suggestions(char **buf, uint32_t count) {
    if (!buf) return;
    for (uint32_t i = 0; i < count; ++i) {
        if (buf[i]) rafael_api_free(buf[i]);
    }
    rafael_api_free(buf);
}

int rafael_commit_text(const char *utf8, size_t len) {
    rafael_log(2, "commit_text stub");
    return 0;
}

int rafael_learn_word(const char *utf8, size_t len, uint32_t flags) {
    rafael_log(2, "learn_word stub");
    return 0;
}

int rafael_load_prefs(const char *path) { rafael_log(2, "load_prefs stub"); return 0; }
int rafael_save_prefs(const char *path) { rafael_log(2, "save_prefs stub"); return 0; }
int rafael_apply_theme(const uint8_t *blob, size_t len) { rafael_log(2, "apply_theme stub"); return 0; }

/* simple allocator wrappers */
void *rafael_api_alloc(size_t size) { return malloc(size); }
void rafael_api_free(void *ptr) { free(ptr); }

void rafael_log(int level, const char *msg) {
    (void)level;
    if (msg) fprintf(stderr, "RAFAELIA: %s\n", msg);
}

/* JNI registration - class: dev.patrickgold.florisboard.inputmethod.RafaelBridge */
/* Implement minimal JNI wrappers if you will call from Kotlin. For the PoC we'll register a few natives. */

static JNINativeMethod methods[] = {
    /* name, signature, function ptr */
    {"nativeInit", "()I", (void*)/* will register in runtime via helper */ NULL}
};

/* Keep JNI_OnLoad minimal: no register (Kotlin can call System.loadLibrary and use manual native mapping) */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void)vm; (void)reserved;
    rafael_log(2, "JNI_OnLoad RAFAELIA loaded");
    return JNI_VERSION_1_6;
}