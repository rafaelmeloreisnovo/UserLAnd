/*
rafael_api.h - Minimal RAFAELIA C API (header)

SPDX-License-Identifier: Apache-2.0

Uso:
- Fornece a API C que o glue.c (JNI) e o código nativo devem implementar.
- Funções são deliberadamente pequenas para começar: init, on_key, get_suggestions, commit, shutdown.
- Implementações podem ser stubs até que o motor completo seja adicionado.
*/

#ifndef RAFAEL_API_H
#define RAFAEL_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rafael_platform_bindings {
    void *user_data;
    void (*commit_text_cb)(void *user_data, const char *utf8, size_t len);
    void *(*alloc_cb)(void *user_data, size_t size);
    void (*free_cb)(void *user_data, void *ptr);
    void (*log_cb)(void *user_data, int level, const char *msg);
} rafael_platform_bindings_t;

/* lifecycle */
int rafael_init(rafael_platform_bindings_t *bindings);
int rafael_shutdown(void);

/* input */
int rafael_on_key(uint32_t keycode, uint32_t modifiers, uint64_t timestamp_ms);

/* suggestions:
   - prefix_utf8: input prefix
   - out_buf: pointer to C array of char* (allocated by callee with alloc_cb or malloc)
   - out_count: number of suggestions returned
   - ethica_exponent: exponent param
   Return 0 on success, negative on error.
*/
int rafael_get_suggestions(const char *prefix_utf8, char ***out_buf, uint32_t *out_count, double ethica_exponent);
void rafael_api_free_suggestions(char **buf, uint32_t count);

/* commit / learning */
int rafael_commit_text(const char *utf8, size_t len);
int rafael_learn_word(const char *utf8, size_t len, uint32_t flags);

/* persistence */
int rafael_load_prefs(const char *path);
int rafael_save_prefs(const char *path);
int rafael_apply_theme(const uint8_t *blob, size_t len);

/* allocators (defaults provided by shim) */
void *rafael_api_alloc(size_t size);
void rafael_api_free(void *ptr);

/* logging helper */
void rafael_log(int level, const char *msg);

#ifdef __cplusplus
}
#endif

#endif /* RAFAEL_API_H */