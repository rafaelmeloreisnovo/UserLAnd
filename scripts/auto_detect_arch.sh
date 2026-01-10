#!/usr/bin/env bash
# ==============================================================================
# auto_detect_arch.sh - Architecture and Platform Auto-Detection
# ==============================================================================
#
# Copyright (c) 2024 Rafael Melo Reis
# Licensed under RAFAELIA MODULE LICENSE
#
# This script provides comprehensive architecture and platform detection for:
# - Cross-compilation targeting Android devices
# - Native compilation on Linux/macOS/BSD hosts
# - Dependency compatibility verification
#
# Output format: JSON or shell-evaluable environment variables
#
# Usage:
#   ./auto_detect_arch.sh [--json|--shell|--verify-deps]
#
# Examples:
#   eval $(./auto_detect_arch.sh --shell)
#   ./auto_detect_arch.sh --json > platform_info.json
#   ./auto_detect_arch.sh --verify-deps
# ==============================================================================

set -euo pipefail

# Script metadata
readonly SCRIPT_VERSION="1.0.0"
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# ==============================================================================
# Host Architecture Detection
# ==============================================================================

detect_host_arch() {
    local arch
    arch="$(uname -m)"
    
    case "$arch" in
        x86_64|amd64)
            echo "x86_64"
            ;;
        aarch64|arm64)
            echo "aarch64"
            ;;
        armv7l|armv7)
            echo "arm"
            ;;
        i686|i386)
            echo "x86"
            ;;
        riscv64)
            echo "riscv64"
            ;;
        ppc64le)
            echo "ppc64le"
            ;;
        *)
            echo "$arch"
            ;;
    esac
}

# ==============================================================================
# Host OS Detection
# ==============================================================================

detect_host_os() {
    local os
    os="$(uname -s)"
    
    case "$os" in
        Linux)
            # Check for Android
            if [[ -f /system/build.prop ]] || [[ -n "${ANDROID_ROOT:-}" ]]; then
                echo "android"
            else
                echo "linux"
            fi
            ;;
        Darwin)
            echo "macos"
            ;;
        FreeBSD)
            echo "freebsd"
            ;;
        OpenBSD)
            echo "openbsd"
            ;;
        NetBSD)
            echo "netbsd"
            ;;
        CYGWIN*|MINGW*|MSYS*)
            echo "windows"
            ;;
        *)
            echo "unknown"
            ;;
    esac
}

# ==============================================================================
# NDK Detection
# ==============================================================================

detect_ndk_path() {
    # Check environment variables first
    local ndk_path="${ANDROID_NDK_HOME:-${NDK_HOME:-${ANDROID_NDK:-}}}"
    
    # Check common NDK locations
    if [[ -z "$ndk_path" ]] || [[ ! -d "$ndk_path" ]]; then
        local possible_paths=(
            "$ROOT_DIR/ndk"
            "$HOME/Android/Sdk/ndk"
            "$HOME/Library/Android/sdk/ndk"
            "/usr/local/android-sdk/ndk"
            "/opt/android-sdk/ndk"
        )
        
        for path in "${possible_paths[@]}"; do
            if [[ -d "$path" ]]; then
                # Find the latest NDK version in the directory
                local latest_ndk
                latest_ndk=$(find "$path" -maxdepth 1 -type d -name '[0-9]*' 2>/dev/null | sort -V | tail -1)
                if [[ -n "$latest_ndk" ]] && [[ -d "$latest_ndk/toolchains" ]]; then
                    ndk_path="$latest_ndk"
                    break
                elif [[ -d "$path/toolchains" ]]; then
                    ndk_path="$path"
                    break
                fi
            fi
        done
    fi
    
    echo "${ndk_path:-}"
}

detect_ndk_version() {
    local ndk_path="$1"
    
    if [[ -z "$ndk_path" ]] || [[ ! -d "$ndk_path" ]]; then
        echo ""
        return
    fi
    
    # Try to read version from source.properties
    local props_file="$ndk_path/source.properties"
    if [[ -f "$props_file" ]]; then
        grep 'Pkg.Revision' "$props_file" 2>/dev/null | sed 's/Pkg.Revision\s*=\s*//' | head -1
    else
        # Fallback: extract from path if it contains version
        basename "$ndk_path" | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' || echo ""
    fi
}

# ==============================================================================
# Android SDK Detection
# ==============================================================================

detect_android_sdk_path() {
    local sdk_path="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
    
    if [[ -z "$sdk_path" ]] || [[ ! -d "$sdk_path" ]]; then
        local possible_paths=(
            "$HOME/Android/Sdk"
            "$HOME/Library/Android/sdk"
            "/usr/local/android-sdk"
            "/opt/android-sdk"
            "/usr/local/lib/android/sdk"
        )
        
        for path in "${possible_paths[@]}"; do
            if [[ -d "$path/platform-tools" ]]; then
                sdk_path="$path"
                break
            fi
        done
    fi
    
    echo "${sdk_path:-}"
}

# ==============================================================================
# Compiler Detection
# ==============================================================================

detect_c_compiler() {
    local cc="${CC:-}"
    
    if [[ -z "$cc" ]]; then
        # Prefer clang for better cross-compilation support
        if command -v clang >/dev/null 2>&1; then
            cc="clang"
        elif command -v gcc >/dev/null 2>&1; then
            cc="gcc"
        elif command -v cc >/dev/null 2>&1; then
            cc="cc"
        fi
    fi
    
    echo "$cc"
}

detect_compiler_version() {
    local cc="$1"
    
    if [[ -z "$cc" ]]; then
        echo ""
        return
    fi
    
    "$cc" --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1 || echo ""
}

# ==============================================================================
# Java Detection
# ==============================================================================

detect_java_version() {
    if command -v java >/dev/null 2>&1; then
        java -version 2>&1 | head -1 | grep -oE '"[0-9]+' | tr -d '"' || echo ""
    else
        echo ""
    fi
}

# ==============================================================================
# Gradle Detection
# ==============================================================================

detect_gradle_version() {
    if [[ -f "$ROOT_DIR/gradle/wrapper/gradle-wrapper.properties" ]]; then
        grep 'distributionUrl' "$ROOT_DIR/gradle/wrapper/gradle-wrapper.properties" 2>/dev/null | \
            grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' || echo ""
    elif command -v gradle >/dev/null 2>&1; then
        gradle --version 2>/dev/null | grep 'Gradle' | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' || echo ""
    else
        echo ""
    fi
}

# ==============================================================================
# Supported Android Architectures
# ==============================================================================

get_supported_android_archs() {
    # Architectures supported by Android NDK
    echo "arm64-v8a armeabi-v7a x86_64 x86"
}

get_android_arch_for_host() {
    local host_arch="$1"
    
    case "$host_arch" in
        x86_64|amd64)
            echo "x86_64"
            ;;
        aarch64|arm64)
            echo "arm64-v8a"
            ;;
        arm|armv7l)
            echo "armeabi-v7a"
            ;;
        i686|i386|x86)
            echo "x86"
            ;;
        *)
            echo ""
            ;;
    esac
}

# ==============================================================================
# NDK Target Triple Mapping
# ==============================================================================

get_ndk_target_triple() {
    local android_arch="$1"
    local api_level="${2:-21}"
    
    case "$android_arch" in
        arm64-v8a)
            echo "aarch64-linux-android${api_level}"
            ;;
        armeabi-v7a)
            echo "armv7a-linux-androideabi${api_level}"
            ;;
        x86_64)
            echo "x86_64-linux-android${api_level}"
            ;;
        x86)
            echo "i686-linux-android${api_level}"
            ;;
        *)
            echo ""
            ;;
    esac
}

# ==============================================================================
# Dependency Verification
# ==============================================================================

verify_dependencies() {
    local status=0
    local host_os
    host_os=$(detect_host_os)
    
    echo "Dependency Verification Report"
    echo "=============================="
    echo ""
    
    # Check Java
    local java_version
    java_version=$(detect_java_version)
    if [[ -n "$java_version" ]]; then
        if [[ "$java_version" -ge 17 ]]; then
            echo "[OK] Java: version $java_version (17+ required)"
        else
            echo "[WARN] Java: version $java_version (17+ recommended for AGP 8.x)"
            status=1
        fi
    else
        echo "[FAIL] Java: not found"
        status=1
    fi
    
    # Check Gradle
    local gradle_version
    gradle_version=$(detect_gradle_version)
    if [[ -n "$gradle_version" ]]; then
        echo "[OK] Gradle: version $gradle_version"
    else
        echo "[WARN] Gradle: wrapper not found, will be downloaded"
    fi
    
    # Check C compiler
    local cc
    cc=$(detect_c_compiler)
    if [[ -n "$cc" ]]; then
        local cc_version
        cc_version=$(detect_compiler_version "$cc")
        echo "[OK] C Compiler: $cc ${cc_version:-unknown version}"
    else
        echo "[FAIL] C Compiler: not found (install gcc or clang)"
        status=1
    fi
    
    # Check Android SDK
    local sdk_path
    sdk_path=$(detect_android_sdk_path)
    if [[ -n "$sdk_path" ]]; then
        echo "[OK] Android SDK: $sdk_path"
    else
        echo "[WARN] Android SDK: not found (set ANDROID_HOME or ANDROID_SDK_ROOT)"
    fi
    
    # Check Android NDK
    local ndk_path
    ndk_path=$(detect_ndk_path)
    if [[ -n "$ndk_path" ]]; then
        local ndk_version
        ndk_version=$(detect_ndk_version "$ndk_path")
        if [[ -n "$ndk_version" ]]; then
            # Check if NDK version is 27+
            local major_version
            major_version=$(echo "$ndk_version" | cut -d. -f1)
            if [[ "$major_version" -ge 27 ]]; then
                echo "[OK] Android NDK: $ndk_version (Android 15 compatible)"
            else
                echo "[WARN] Android NDK: $ndk_version (NDK r27+ recommended for Android 15)"
            fi
        else
            echo "[OK] Android NDK: $ndk_path"
        fi
    else
        echo "[WARN] Android NDK: not found (set ANDROID_NDK_HOME for cross-compilation)"
    fi
    
    # Check CMake (for native builds)
    if command -v cmake >/dev/null 2>&1; then
        local cmake_version
        cmake_version=$(cmake --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')
        echo "[OK] CMake: version $cmake_version"
    else
        echo "[WARN] CMake: not found (optional, for native builds)"
    fi
    
    echo ""
    if [[ $status -eq 0 ]]; then
        echo "All critical dependencies satisfied."
    else
        echo "Some dependencies need attention. See warnings/failures above."
    fi
    
    return $status
}

# ==============================================================================
# Output Generation
# ==============================================================================

output_json() {
    local host_arch host_os ndk_path ndk_version sdk_path cc cc_version java_version gradle_version
    
    host_arch=$(detect_host_arch)
    host_os=$(detect_host_os)
    ndk_path=$(detect_ndk_path)
    ndk_version=$(detect_ndk_version "$ndk_path")
    sdk_path=$(detect_android_sdk_path)
    cc=$(detect_c_compiler)
    cc_version=$(detect_compiler_version "$cc")
    java_version=$(detect_java_version)
    gradle_version=$(detect_gradle_version)
    
    local android_arch
    android_arch=$(get_android_arch_for_host "$host_arch")
    
    # Determine Android 15 compatibility based on NDK version
    local android_15_compat="false"
    if [[ -n "$ndk_version" ]]; then
        local major_version="${ndk_version%%.*}"
        if [[ -n "$major_version" ]] && [[ "$major_version" =~ ^[0-9]+$ ]] && [[ "$major_version" -ge 27 ]]; then
            android_15_compat="true"
        fi
    fi
    
    cat <<EOF
{
  "version": "$SCRIPT_VERSION",
  "host": {
    "arch": "$host_arch",
    "os": "$host_os"
  },
  "android": {
    "sdk_path": "${sdk_path:-null}",
    "ndk_path": "${ndk_path:-null}",
    "ndk_version": "${ndk_version:-null}",
    "supported_archs": ["arm64-v8a", "armeabi-v7a", "x86_64", "x86"],
    "target_arch": "${android_arch:-null}",
    "min_api_level": 21,
    "target_api_level": 35,
    "android_15_compatible": $android_15_compat
  },
  "toolchain": {
    "c_compiler": "${cc:-null}",
    "c_compiler_version": "${cc_version:-null}",
    "java_version": "${java_version:-null}",
    "gradle_version": "${gradle_version:-null}"
  },
  "simd_support": {
    "arm64_neon": true,
    "arm32_neon": true,
    "x86_64_sse42": true,
    "x86_64_avx2": true,
    "x86_sse2": true
  }
}
EOF
}

output_shell() {
    local host_arch host_os ndk_path ndk_version sdk_path cc java_version gradle_version
    
    host_arch=$(detect_host_arch)
    host_os=$(detect_host_os)
    ndk_path=$(detect_ndk_path)
    ndk_version=$(detect_ndk_version "$ndk_path")
    sdk_path=$(detect_android_sdk_path)
    cc=$(detect_c_compiler)
    java_version=$(detect_java_version)
    gradle_version=$(detect_gradle_version)
    
    local android_arch
    android_arch=$(get_android_arch_for_host "$host_arch")
    
    cat <<EOF
# Auto-detected platform configuration
export RAF_HOST_ARCH="$host_arch"
export RAF_HOST_OS="$host_os"
export RAF_ANDROID_SDK="${sdk_path:-}"
export RAF_ANDROID_NDK="${ndk_path:-}"
export RAF_NDK_VERSION="${ndk_version:-}"
export RAF_TARGET_ARCH="${android_arch:-}"
export RAF_CC="${cc:-cc}"
export RAF_JAVA_VERSION="${java_version:-}"
export RAF_GRADLE_VERSION="${gradle_version:-}"
export RAF_MIN_API_LEVEL="21"
export RAF_TARGET_API_LEVEL="35"
export RAF_SUPPORTED_ARCHS="arm64-v8a armeabi-v7a x86_64 x86"

# NDK target triples for cross-compilation
export RAF_NDK_TRIPLE_ARM64="$(get_ndk_target_triple arm64-v8a 21)"
export RAF_NDK_TRIPLE_ARM="$(get_ndk_target_triple armeabi-v7a 21)"
export RAF_NDK_TRIPLE_X86_64="$(get_ndk_target_triple x86_64 21)"
export RAF_NDK_TRIPLE_X86="$(get_ndk_target_triple x86 21)"
EOF
}

# ==============================================================================
# Main
# ==============================================================================

main() {
    local mode="${1:-}"
    
    case "$mode" in
        --json|-j)
            output_json
            ;;
        --shell|-s)
            output_shell
            ;;
        --verify-deps|-v)
            verify_dependencies
            ;;
        --version)
            echo "auto_detect_arch.sh version $SCRIPT_VERSION"
            ;;
        --help|-h)
            cat <<EOF
auto_detect_arch.sh - Architecture and Platform Auto-Detection

Usage: $0 [OPTIONS]

Options:
  --json, -j          Output detection results as JSON
  --shell, -s         Output detection results as shell variables
  --verify-deps, -v   Verify all dependencies and report status
  --version           Show script version
  --help, -h          Show this help message

Examples:
  $0 --json > platform.json
  eval \$($0 --shell)
  $0 --verify-deps

Environment Variables:
  ANDROID_HOME, ANDROID_SDK_ROOT    Android SDK path
  ANDROID_NDK_HOME, NDK_HOME        Android NDK path
  CC                                C compiler to use

Supported Android Architectures:
  - arm64-v8a    (64-bit ARM, most modern devices)
  - armeabi-v7a  (32-bit ARM, older devices)
  - x86_64       (64-bit Intel/AMD, emulators)
  - x86          (32-bit Intel/AMD, older emulators)
EOF
            ;;
        "")
            # Default: show basic info
            echo "Host Architecture: $(detect_host_arch)"
            echo "Host OS: $(detect_host_os)"
            echo "Use --help for more options"
            ;;
        *)
            echo "Unknown option: $mode" >&2
            echo "Use --help for usage information" >&2
            exit 1
            ;;
    esac
}

main "$@"
