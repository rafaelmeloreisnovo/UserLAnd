#include <jni.h>
#include <stdint.h>

#include "raf_compile_key.h"

uint32_t raf_crc32(const uint8_t *data, uint32_t len);

JNIEXPORT jint JNICALL
Java_tech_ula_rafaelia_RafaeliaNative_crc32(JNIEnv *env, jclass clazz, jbyteArray arr) {
    if (arr == 0) return 0;
    jsize len = (*env)->GetArrayLength(env, arr);
    jbyte *ptr = (*env)->GetByteArrayElements(env, arr, 0);
    if (ptr == 0) return 0;
    uint32_t out = raf_crc32((const uint8_t *)ptr, (uint32_t)len);
    (*env)->ReleaseByteArrayElements(env, arr, ptr, JNI_ABORT);
    return (jint)out;
}

JNIEXPORT jlong JNICALL
Java_tech_ula_rafaelia_RafaeliaNative_compileKeyReportNative(JNIEnv *env,
                                                             jclass clazz,
                                                             jbyteArray arr,
                                                             jint flags,
                                                             jint expectedCrc32) {
    if (arr == 0) {
        RafCompileKeyReport report = raf_compile_key_validate(0, 0u, (uint32_t)flags, (uint32_t)expectedCrc32);
        return ((jlong)report.status << 32) | ((jlong)report.entropy_milli << 16) | (jlong)report.tag14;
    }

    jsize len = (*env)->GetArrayLength(env, arr);
    jbyte *ptr = (*env)->GetByteArrayElements(env, arr, 0);
    if (ptr == 0) return ((jlong)RAF_COMPILE_STATUS_NULL_INPUT << 32);

    RafCompileKeyReport report = raf_compile_key_validate((const uint8_t *)ptr,
                                                          (uint32_t)len,
                                                          (uint32_t)flags,
                                                          (uint32_t)expectedCrc32);
    (*env)->ReleaseByteArrayElements(env, arr, ptr, JNI_ABORT);

    return ((jlong)report.status << 32) |
           ((jlong)report.entropy_milli << 16) |
           (jlong)report.tag14;
}
