#!/bin/bash
# RAFAELIA: THE FINAL TRUTH (CORRECTED ARCHITECTURE)
# [1] SYSCALLS: Mmap(6 args), OpenAt(4 args), Close(57)
# [2] CRYPTO: SHA-256 + CRC32 (Compact & Deterministic)
# [3] MEMORY: Overlay Frame via MAP_ANON

set -euo pipefail

rm -rf Rafaelia_Corrected
mkdir -p Rafaelia_Corrected/app/src/main/cpp
mkdir -p Rafaelia_Corrected/app/src/main/java/com/rafaelia/engine

# -----------------------------------------------------------------------
# 1. HEADER (DEFINIÇÕES AARCH64 KERNEL 5.15)
# -----------------------------------------------------------------------
cat << 'EOC' > Rafaelia_Corrected/app/src/main/cpp/rafaelia.h
#ifndef RAFAELIA_H
#define RAFAELIA_H

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t  s64;

// CONFIGURAÇÃO CAT.OF
#define MAGIC_CATOF    0x4341544F
#define VERSION_CATOF  1
#define PAYLOAD_WORDS  2048

// ESTRUTURAS
typedef struct {
    u32 magic;
    u16 version;
    u16 reserved;
    u32 payload_bytes;
    u32 crc32;
    u8  sha256[32];
} __attribute__((packed)) catof_header_t;

typedef struct {
    catof_header_t header;
    u64 payload[PAYLOAD_WORDS];
} Overlay_Frame;

// SYSCALL NUMBERS (ARM64)
#define SYS_OPENAT      56
#define SYS_CLOSE       57   // CORREÇÃO: 57, não 3
#define SYS_READ        63
#define SYS_WRITE       64
#define SYS_NANOSLEEP   101
#define SYS_KILL        129
#define SYS_MMAP        222

// FLAGS
#define AT_FDCWD        (-100)
#define O_CREAT         0100
#define O_WRONLY        01
#define O_APPEND        02000

#define PROT_READ       0x1
#define PROT_WRITE      0x2
#define PROT_RW         (PROT_READ | PROT_WRITE)

#define MAP_PRIVATE     0x02
#define MAP_ANONYMOUS   0x20
#define MAP_ANON_PRIV   (MAP_PRIVATE | MAP_ANONYMOUS)

#endif
EOC

# -----------------------------------------------------------------------
# 2. MOTOR NATIVO (BARE METAL IMPLEMENTATION)
# -----------------------------------------------------------------------
cat << 'EOC' > Rafaelia_Corrected/app/src/main/cpp/native-lib.c
#include <jni.h>
#include "rafaelia.h"

// --- MEMORY UTILS (NO LIBC) ---
static void _mini_memcpy(void *dest, const void *src, u64 n) {
    u8 *d = (u8*)dest; const u8 *s = (const u8*)src;
    while (n--) *d++ = *s++;
}
static void _mini_memset(void *dest, u8 val, u64 n) {
    u8 *d = (u8*)dest; while (n--) *d++ = val;
}

// --- CRC32 ---
static u32 crc32_calc(const void *data, u64 len) {
    const u8 *p = (const u8*)data;
    u32 crc = 0xFFFFFFFFu;
    for (u64 i = 0; i < len; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320u & (-(int)(crc & 1)));
    }
    return ~crc;
}

// --- SHA-256 ---
#define ROTR(x,n) (((x) >> (n)) | ((x) << (32-(n))))
#define CH(x,y,z) (((x)&(y)) ^ (~(x)&(z)))
#define MAJ(x,y,z) (((x)&(y)) ^ ((x)&(z)) ^ ((y)&(z)))

static const u32 K256[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,
  0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
  0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,
  0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,
  0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
  0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,
  0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,
  0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
  0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static void sha256(const u8 *msg, u64 len, u8 out[32]) {
    u32 H[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    u8 block[64];
    u64 i;
    for (i = 0; i + 64 <= len; i += 64) {
        _mini_memcpy(block, msg + i, 64);
        u32 W[64];
        for (int t = 0; t < 16; t++) W[t] = (block[t*4]<<24)|(block[t*4+1]<<16)|(block[t*4+2]<<8)|(block[t*4+3]);
        for (int t = 16; t < 64; t++) {
            u32 s0 = (ROTR(W[t-15],7) ^ ROTR(W[t-15],18) ^ (W[t-15]>>3));
            u32 s1 = (ROTR(W[t-2],17) ^ ROTR(W[t-2],19) ^ (W[t-2]>>10));
            W[t] = W[t-16] + s0 + W[t-7] + s1;
        }
        u32 a=H[0],b=H[1],c=H[2],d=H[3],e=H[4],f=H[5],g=H[6],h=H[7];
        for (int t = 0; t < 64; t++) {
            u32 S1 = (ROTR(e,6) ^ ROTR(e,11) ^ ROTR(e,25));
            u32 T1 = h + S1 + CH(e,f,g) + K256[t] + W[t];
            u32 S0 = (ROTR(a,2) ^ ROTR(a,13) ^ ROTR(a,22));
            u32 T2 = S0 + MAJ(a,b,c);
            h=g; g=f; f=e; e=d+T1; d=c; c=b; b=a; a=T1+T2;
        }
        H[0]+=a; H[1]+=b; H[2]+=c; H[3]+=d; H[4]+=e; H[5]+=f; H[6]+=g; H[7]+=h;
    }
    for (int j = 0; j < 8; j++) {
        out[j*4+0]=(H[j]>>24)&0xFF; out[j*4+1]=(H[j]>>16)&0xFF; out[j*4+2]=(H[j]>>8)&0xFF; out[j*4+3]=H[j]&0xFF;
    }
}

// --- SYSCALL WRAPPERS (CORRIGIDOS: 6 ARGS para MMAP) ---
static inline u64 _sys_call6(u64 n, u64 a, u64 b, u64 c, u64 d, u64 e, u64 f) {
    register u64 x8 asm("x8") = n;
    register u64 x0 asm("x0") = a;
    register u64 x1 asm("x1") = b;
    register u64 x2 asm("x2") = c;
    register u64 x3 asm("x3") = d;
    register u64 x4 asm("x4") = e;
    register u64 x5 asm("x5") = f;
    asm volatile (
        "svc #0\n"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
        : "memory", "x30"
    );
    return x0;
}

static inline u64 _sys_call4(u64 n, u64 a, u64 b, u64 c, u64 d) {
    return _sys_call6(n, a, b, c, d, 0, 0);
}

static inline u64 _sys_call3(u64 n, u64 a, u64 b, u64 c) {
    return _sys_call6(n, a, b, c, 0, 0, 0);
}

// --- MAIN ENGINE ---
static Overlay_Frame* OF = 0;

JNIEXPORT void JNICALL Java_com_rafaelia_engine_MainActivity_startEngine(JNIEnv* env, jobject obj) {
    (void)env; (void)obj;

    // 1. MMAP (6 ARGUMENTOS) - CORRECT
    OF = (Overlay_Frame*)_sys_call6(
        SYS_MMAP, 0, (u64)sizeof(Overlay_Frame),
        (u64)PROT_RW, (u64)MAP_ANON_PRIV, (u64)-1, 0
    );

    if ((u64)OF >= (u64)-4095) return; // Mmap falhou

    // 2. OPENAT (4 ARGUMENTOS: Com permissão 0600) - CORRECT
    u64 fd = _sys_call4(
        SYS_OPENAT, (u64)AT_FDCWD, 
        (u64)"/data/data/com.rafaelia.engine/rafaelia.db",
        (u64)(O_CREAT | O_WRONLY | O_APPEND), (u64)0600
    );
    if ((s64)fd < 0) return;

    // 3. INIT
    for (int i = 0; i < PAYLOAD_WORDS; i++) OF->payload[i] = 0x963 + i;

    // 4. LOOP
    for (;;) {
        // Mutação
        OF->payload[0] ^= (OF->payload[1] + OF->payload[2]);

        // Frame
        OF->header.magic = MAGIC_CATOF;
        OF->header.version = VERSION_CATOF;
        OF->header.reserved = 0;
        OF->header.payload_bytes = (u32)sizeof(OF->payload);

        // Crypto
        OF->header.crc32 = crc32_calc(OF->payload, (u64)sizeof(OF->payload));
        _mini_memset(OF->header.sha256, 0, 32);
        sha256((u8*)OF->payload, (u64)sizeof(OF->payload), OF->header.sha256);

        // Zumbificador (Opcional, mas arquitetado)
        // _sys_call3(SYS_KILL, 0, 19, 0);

        // Escrita
        _sys_call3(SYS_WRITE, fd, (u64)&OF->header, (u64)sizeof(catof_header_t));
        _sys_call3(SYS_WRITE, fd, (u64)OF->payload, (u64)sizeof(OF->payload));

        // Nanosleep
        u64 t[2] = {0, 10000000ULL};
        _sys_call3(SYS_NANOSLEEP, (u64)t, 0, 0);
    }
    
    // Cleanup (Unreachable no loop infinito, mas correto arquiteturalmente)
    // _sys_call3(SYS_CLOSE, fd, 0, 0);
}
EOC

# -----------------------------------------------------------------------
# 3. MANIFESTO (MINIMALISTA)
# -----------------------------------------------------------------------
cat << 'EOC' > Rafaelia_Corrected/app/src/main/AndroidManifest.xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="com.rafaelia.engine">
    <application android:label="RAFAELIA FINAL" android:extractNativeLibs="true" android:hasCode="true">
        <activity android:name=".MainActivity" android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN"/>
                <category android:name="android.intent.category.LAUNCHER"/>
            </intent-filter>
        </activity>
    </application>
</manifest>
EOC

# -----------------------------------------------------------------------
# 4. JAVA BOOTSTRAP
# -----------------------------------------------------------------------
cat << 'EOC' > Rafaelia_Corrected/app/src/main/java/com/rafaelia/engine/MainActivity.java
package com.rafaelia.engine;
import android.app.Activity;
import android.os.Bundle;

public class MainActivity extends Activity {
    static { System.loadLibrary("rafaelia_core"); }
    private native void startEngine();
    @Override protected void onCreate(Bundle s) { super.onCreate(s); new Thread(this::startEngine).start(); }
}
EOC

# -----------------------------------------------------------------------
# 5. CMAKE (OPTIMIZED)
# -----------------------------------------------------------------------
cat << 'EOC' > Rafaelia_Corrected/app/CMakeLists.txt
cmake_minimum_required(VERSION 3.10.2)
add_library(rafaelia_core SHARED src/main/cpp/native-lib.c)
target_compile_options(rafaelia_core PRIVATE -O3 -Wall -Wextra -fno-builtin)
target_link_libraries(rafaelia_core log)
EOC

echo "RAFAELIA CORRECTED: MMAP(6), OPENAT(4), CLOSE(57), CRYPTO(SHA/CRC) INTEGRO."
