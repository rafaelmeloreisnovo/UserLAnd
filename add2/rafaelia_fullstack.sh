#!/bin/bash
# RAFAELIA FULL STACK — "CIMENTO"
# Do zero ao APK debug em um comando.
# Requisitos:
#   - ANDROID_SDK_ROOT ou ANDROID_HOME configurado
#   - NDK instalado (via SDK Manager) e CMake instalado
#   - Java 17 (recomendado) + Gradle (ou internet p/ wrapper baixar)
# Uso:
#   bash rafaelia_fullstack.sh
#   cd Rafaelia_FullStack && ./gradlew assembleDebug

set -euo pipefail

PROJECT="Rafaelia_FullStack"
PKG="com.rafaelia.engine"
APP_DIR="$PROJECT/app"

echo "==[1/7] Limpando e criando estrutura =="
rm -rf "$PROJECT"
mkdir -p "$APP_DIR/src/main/java/com/rafaelia/engine"
mkdir -p "$APP_DIR/src/main/cpp"
mkdir -p "$PROJECT/gradle/wrapper"

echo "==[2/7] Arquivos Gradle (root + settings + wrapper props) =="

cat << 'EOT' > "$PROJECT/settings.gradle"
rootProject.name = "Rafaelia_FullStack"
include(":app")
EOT

cat << 'EOT' > "$PROJECT/build.gradle"
buildscript {
    repositories { google(); mavenCentral() }
    dependencies { classpath "com.android.tools.build:gradle:8.2.2" }
}
allprojects { repositories { google(); mavenCentral() } }
EOT

cat << 'EOT' > "$PROJECT/gradle.properties"
org.gradle.jvmargs=-Xmx2g -Dfile.encoding=UTF-8
android.useAndroidX=true
android.nonTransitiveRClass=true
EOT

# Wrapper: o gradle wrapper baixa a distro automaticamente quando você roda ./gradlew
cat << 'EOT' > "$PROJECT/gradle/wrapper/gradle-wrapper.properties"
distributionBase=GRADLE_USER_HOME
distributionPath=wrapper/dists
zipStoreBase=GRADLE_USER_HOME
zipStorePath=wrapper/dists
distributionUrl=https\://services.gradle.org/distributions/gradle-8.5-bin.zip
EOT

# gradlew + gradlew.bat mínimos (sem depender de gradle instalado)
cat << 'EOT' > "$PROJECT/gradlew"
#!/bin/sh
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
WRAPPER_JAR="$DIR/gradle/wrapper/gradle-wrapper.jar"
PROPS="$DIR/gradle/wrapper/gradle-wrapper.properties"

if [ ! -f "$WRAPPER_JAR" ]; then
  echo "Baixando gradle-wrapper.jar..."
  mkdir -p "$DIR/gradle/wrapper"
  # wrapper jar oficial pequeno
  curl -fsSL -o "$WRAPPER_JAR" "https://repo1.maven.org/maven2/org/gradle/gradle-wrapper/8.5/gradle-wrapper-8.5.jar"
fi

exec java -classpath "$WRAPPER_JAR" org.gradle.wrapper.GradleWrapperMain "$@"
EOT
chmod +x "$PROJECT/gradlew"

# (Opcional) Windows
cat << 'EOT' > "$PROJECT/gradlew.bat"
@echo off
set DIR=%~dp0
set WRAPPER_JAR=%DIR%gradle\wrapper\gradle-wrapper.jar
if not exist "%WRAPPER_JAR%" (
  echo Downloading gradle-wrapper.jar...
  powershell -Command "Invoke-WebRequest -Uri https://repo1.maven.org/maven2/org/gradle/gradle-wrapper/8.5/gradle-wrapper-8.5.jar -OutFile %WRAPPER_JAR%"
)
java -classpath "%WRAPPER_JAR%" org.gradle.wrapper.GradleWrapperMain %*
EOT

echo "==[3/7] App Gradle + CMake =="
cat << 'EOT' > "$APP_DIR/build.gradle"
plugins { id "com.android.application" }

android {
    namespace "com.rafaelia.engine"
    compileSdk 34

    defaultConfig {
        applicationId "com.rafaelia.engine"
        minSdk 29
        targetSdk 34
        versionCode 1
        versionName "1.0"

        externalNativeBuild {
            cmake {
                cppFlags ""
            }
        }
    }

    buildTypes {
        debug {
            debuggable true
        }
        release {
            minifyEnabled false
        }
    }

    externalNativeBuild {
        cmake { path "CMakeLists.txt" }
    }
}

dependencies { }
EOT

cat << 'EOT' > "$APP_DIR/CMakeLists.txt"
cmake_minimum_required(VERSION 3.10.2)
project(rafaelia_core C)

add_library(rafaelia_core SHARED
    src/main/cpp/native-lib.c
)

target_compile_options(rafaelia_core PRIVATE
    -O3 -Wall -Wextra -Werror
)

target_link_libraries(rafaelia_core log)
EOT

echo "==[4/7] Manifesto + UI Java (Start/Stop) =="
cat << 'EOT' > "$APP_DIR/src/main/AndroidManifest.xml"
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.rafaelia.engine">
    <application
        android:label="RAFAELIA"
        android:extractNativeLibs="true">
        <activity
            android:name=".MainActivity"
            android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>
</manifest>
EOT

cat << 'EOT' > "$APP_DIR/src/main/java/com/rafaelia/engine/MainActivity.java"
package com.rafaelia.engine;

import android.app.Activity;
import android.os.Bundle;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

public class MainActivity extends Activity {
    static { System.loadLibrary("rafaelia_core"); }

    private native void startEngine(String outPath);
    private native void stopEngine();

    private TextView status;
    private volatile boolean running = false;

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);

        status = new TextView(this);
        status.setText("RAFAELIA: idle");

        Button start = new Button(this);
        start.setText("START");

        Button stop = new Button(this);
        stop.setText("STOP");

        start.setOnClickListener(v -> {
            if (running) return;
            running = true;
            status.setText("RAFAELIA: running");
            String outPath = getFilesDir().getAbsolutePath() + "/rafaelia.catof";
            new Thread(() -> startEngine(outPath)).start();
        });

        stop.setOnClickListener(v -> {
            if (!running) return;
            stopEngine();
            running = false;
            status.setText("RAFAELIA: stopped");
        });

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.addView(status);
        root.addView(start);
        root.addView(stop);

        setContentView(root);
    }

    @Override
    protected void onDestroy() {
        stopEngine();
        super.onDestroy();
    }
}
EOT

echo "==[5/7] Header Cat.OF + Core Nativo (CRC32 + SHA256 completo) =="
cat << 'EOT' > "$APP_DIR/src/main/cpp/rafaelia.h"
#ifndef RAFAELIA_H
#define RAFAELIA_H

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define MAGIC_CATOF   0x4341544Ful /* 'CATO' */
#define VERSION_CATOF 1u
#define PAYLOAD_WORDS 2048u

typedef struct __attribute__((packed)) {
    u32 magic;
    u16 version;
    u16 reserved;
    u32 payload_bytes;
    u32 crc32;
    u8  sha256[32];
} catof_header_t;

typedef struct __attribute__((packed)) {
    catof_header_t h;
    u64 payload[PAYLOAD_WORDS];
} overlay_frame_t;

#endif
EOT

cat << 'EOT' > "$APP_DIR/src/main/cpp/native-lib.c"
#include <jni.h>
#include <android/log.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "rafaelia.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "RAFAELIA", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "RAFAELIA", __VA_ARGS__)

/* ==================== Engine State ==================== */
static volatile int g_run = 0;

/* ==================== CRC32 ==================== */
static u32 crc32_calc(const void* data, size_t len) {
    const u8* p = (const u8*)data;
    u32 crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (-(int)(crc & 1)));
        }
    }
    return ~crc;
}

/* ==================== SHA-256 (com padding completo) ==================== */
#define ROTR32(x,n) (((x) >> (n)) | ((x) << (32-(n))))
#define CH(x,y,z)   (((x)&(y)) ^ (~(x)&(z)))
#define MAJ(x,y,z)  (((x)&(y)) ^ ((x)&(z)) ^ ((y)&(z)))
#define BSIG0(x)    (ROTR32((x),2) ^ ROTR32((x),13) ^ ROTR32((x),22))
#define BSIG1(x)    (ROTR32((x),6) ^ ROTR32((x),11) ^ ROTR32((x),25))
#define SSIG0(x)    (ROTR32((x),7) ^ ROTR32((x),18) ^ ((x)>>3))
#define SSIG1(x)    (ROTR32((x),17) ^ ROTR32((x),19) ^ ((x)>>10))

static const u32 K256[64] = {
  0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
  0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
  0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
  0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
  0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
  0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
  0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
  0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

static void sha256_one(const u8* msg, size_t len, u8 out[32]) {
    u32 H[8] = {
        0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
        0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u
    };

    // Padding: cria buffer em heap (len + 1 + pad + 8)
    u64 bitlen = (u64)len * 8u;
    size_t new_len = len + 1;
    while ((new_len % 64) != 56) new_len++;
    new_len += 8;

    u8* buf = (u8*)malloc(new_len);
    if (!buf) { memset(out, 0, 32); return; }

    memcpy(buf, msg, len);
    buf[len] = 0x80;
    memset(buf + len + 1, 0, new_len - (len + 1) - 8);

    // big-endian bit length
    for (int i = 0; i < 8; i++) {
        buf[new_len - 1 - i] = (u8)(bitlen >> (8 * i));
    }

    for (size_t off = 0; off < new_len; off += 64) {
        u32 W[64];
        const u8* b = buf + off;

        for (int t = 0; t < 16; t++) {
            W[t] = ((u32)b[t*4] << 24) | ((u32)b[t*4+1] << 16) | ((u32)b[t*4+2] << 8) | (u32)b[t*4+3];
        }
        for (int t = 16; t < 64; t++) {
            W[t] = SSIG1(W[t-2]) + W[t-7] + SSIG0(W[t-15]) + W[t-16];
        }

        u32 a=H[0],b2=H[1],c=H[2],d=H[3],e=H[4],f=H[5],g=H[6],h=H[7];

        for (int t = 0; t < 64; t++) {
            u32 T1 = h + BSIG1(e) + CH(e,f,g) + K256[t] + W[t];
            u32 T2 = BSIG0(a) + MAJ(a,b2,c);
            h=g; g=f; f=e; e=d + T1; d=c; c=b2; b2=a; a=T1 + T2;
        }

        H[0]+=a; H[1]+=b2; H[2]+=c; H[3]+=d;
        H[4]+=e; H[5]+=f;  H[6]+=g; H[7]+=h;
    }

    free(buf);

    for (int i = 0; i < 8; i++) {
        out[i*4+0] = (u8)(H[i] >> 24);
        out[i*4+1] = (u8)(H[i] >> 16);
        out[i*4+2] = (u8)(H[i] >> 8);
        out[i*4+3] = (u8)(H[i]);
    }
}

/* ==================== IO helpers ==================== */
static int open_out(const char* path) {
    int fd = open(path, O_CREAT | O_WRONLY | O_APPEND, 0600);
    return fd;
}

static int write_all(int fd, const void* buf, size_t len) {
    const u8* p = (const u8*)buf;
    size_t off = 0;
    while (off < len) {
        ssize_t w = write(fd, p + off, len - off);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        off += (size_t)w;
    }
    return 0;
}

/* ==================== JNI ==================== */
JNIEXPORT void JNICALL
Java_com_rafaelia_engine_MainActivity_stopEngine(JNIEnv* env, jobject obj) {
    (void)env; (void)obj;
    g_run = 0;
}

JNIEXPORT void JNICALL
Java_com_rafaelia_engine_MainActivity_startEngine(JNIEnv* env, jobject obj, jstring outPath) {
    (void)env; (void)obj;
    const char* path = (*env)->GetStringUTFChars(env, outPath, 0);
    if (!path) return;

    int fd = open_out(path);
    if (fd < 0) {
        LOGE("open failed: %s", strerror(errno));
        (*env)->ReleaseStringUTFChars(env, outPath, path);
        return;
    }

    overlay_frame_t fr;
    memset(&fr, 0, sizeof(fr));

    // init payload determinístico
    for (u32 i = 0; i < PAYLOAD_WORDS; i++) {
        fr.payload[i] = 0x963u + (u64)i;
    }

    g_run = 1;
    LOGI("Engine started -> %s", path);

    while (g_run) {
        // mutação determinística
        fr.payload[0] ^= (fr.payload[1] + fr.payload[2]);

        // header Cat.OF completo
        fr.h.magic = (u32)MAGIC_CATOF;
        fr.h.version = (u16)VERSION_CATOF;
        fr.h.reserved = 0;
        fr.h.payload_bytes = (u32)sizeof(fr.payload);

        fr.h.crc32 = crc32_calc(fr.payload, sizeof(fr.payload));
        sha256_one((const u8*)fr.payload, sizeof(fr.payload), fr.h.sha256);

        // escreve frame inteiro (header+payload)
        if (write_all(fd, &fr, sizeof(fr)) != 0) {
            LOGE("write failed: %s", strerror(errno));
            break;
        }

        // 10ms
        usleep(10000);
    }

    close(fd);
    (*env)->ReleaseStringUTFChars(env, outPath, path);
    LOGI("Engine stopped.");
}
EOT

echo "==[6/7] (Opcional) local.properties =="
SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}"
if [ -n "${SDK}" ]; then
  cat << EOT > "$PROJECT/local.properties"
sdk.dir=${SDK}
EOT
  echo "local.properties escrito com sdk.dir=$SDK"
else
  echo "Aviso: ANDROID_SDK_ROOT/ANDROID_HOME não definido. Se o Gradle reclamar, crie $PROJECT/local.properties com sdk.dir=/caminho/do/sdk"
fi

echo "==[7/7] Pronto. Build =="
echo "cd $PROJECT"
echo "./gradlew assembleDebug"
echo ""
echo "APK debug:"
echo "$PROJECT/app/build/outputs/apk/debug/app-debug.apk"
