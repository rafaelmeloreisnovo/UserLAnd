#include <jni.h>
#include <stdint.h>

uint32_t raf_crc32(const uint8_t *data, uint32_t len);

JNIEXPORT jint JNICALL
Java_tech_ula_rafaelia_RafaeliaNative_crc32(JNIEnv *env, jclass clazz, jbyteArray arr) {
    jsize len = (*env)->GetArrayLength(env, arr);
    jbyte *ptr = (*env)->GetByteArrayElements(env, arr, 0);
    uint32_t out = raf_crc32((const uint8_t *)ptr, (uint32_t)len);
    (*env)->ReleaseByteArrayElements(env, arr, ptr, JNI_ABORT);
    return (jint)out;
}
