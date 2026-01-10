#!/usr/bin/env bash
# ==============================================================================
# build_all_archs.sh - Build Native Binaries for All Android Architectures
# ==============================================================================
#
# This script builds helper binaries for all supported Android architectures.
# Requires Android NDK to be installed and configured.
#
# Supported architectures:
# - arm64-v8a  (64-bit ARM, most modern devices)
# - armeabi-v7a (32-bit ARM, older devices)
# - x86_64     (64-bit Intel/AMD, emulators)
# - x86        (32-bit Intel/AMD, older emulators)
#
# Usage:
#   ./scripts/build_all_archs.sh [--api-level <level>] [--output-dir <dir>]
#
# Examples:
#   ./scripts/build_all_archs.sh                    # Build for all archs
#   ./scripts/build_all_archs.sh --api-level 35    # Target Android 15
#   ./scripts/build_all_archs.sh --output-dir libs # Custom output directory
# ==============================================================================

set -euo pipefail

# Script directory and repository root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default configuration
API_LEVEL="${API_LEVEL:-21}"
OUTPUT_BASE="${OUTPUT_DIR:-$ROOT_DIR/build/bin}"
ARCHS=("arm64" "arm" "x86_64" "x86")

# Parse command-line arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --api-level|-a)
            API_LEVEL="$2"
            shift 2
            ;;
        --output-dir|-o)
            OUTPUT_BASE="$2"
            shift 2
            ;;
        --archs)
            IFS=',' read -ra ARCHS <<< "$2"
            shift 2
            ;;
        --help|-h)
            cat <<EOF
Usage: $0 [OPTIONS]

Build native binaries for all Android architectures.

Options:
  --api-level, -a <level>   Android API level (default: 21)
  --output-dir, -o <dir>    Output directory (default: build/bin)
  --archs <arch1,arch2>     Comma-separated list of architectures
  --help, -h                Show this help message

Supported architectures: arm64, arm, x86_64, x86

Environment variables:
  NDK_DIR, ANDROID_NDK_HOME    Path to Android NDK
  API_LEVEL                     Android API level
  OUTPUT_DIR                    Output directory

Examples:
  $0                                    # Build for all archs
  $0 --api-level 35                     # Target Android 15
  $0 --archs arm64,x86_64              # Build only ARM64 and x86_64
EOF
            exit 0
            ;;
        *)
            echo "[ERROR] Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

# ==============================================================================
# NDK Detection
# ==============================================================================

detect_ndk() {
    local ndk_dir="${NDK_DIR:-${ANDROID_NDK_HOME:-${NDK_HOME:-${ANDROID_NDK:-}}}}"
    
    if [[ -z "$ndk_dir" ]] || [[ ! -d "$ndk_dir" ]]; then
        local possible_paths=(
            "$ROOT_DIR/ndk"
            "$HOME/Android/Sdk/ndk"
            "$HOME/Library/Android/sdk/ndk"
            "/usr/local/android-sdk/ndk"
            "/opt/android-sdk/ndk"
            "/usr/local/lib/android/sdk/ndk"
        )
        
        for path in "${possible_paths[@]}"; do
            if [[ -d "$path" ]]; then
                local latest_ndk
                latest_ndk=$(find "$path" -maxdepth 1 -type d -name '[0-9]*' 2>/dev/null | sort -V | tail -1)
                if [[ -n "$latest_ndk" ]] && [[ -d "$latest_ndk/toolchains" ]]; then
                    ndk_dir="$latest_ndk"
                    break
                elif [[ -d "$path/toolchains" ]]; then
                    ndk_dir="$path"
                    break
                fi
            fi
        done
    fi
    
    echo "$ndk_dir"
}

# ==============================================================================
# Architecture Mapping
# ==============================================================================

get_android_abi() {
    local arch="$1"
    case "$arch" in
        arm64|aarch64) echo "arm64-v8a" ;;
        arm|armeabi-v7a) echo "armeabi-v7a" ;;
        x86_64) echo "x86_64" ;;
        x86|i686) echo "x86" ;;
        *) echo "$arch" ;;
    esac
}

# ==============================================================================
# Main Build Process
# ==============================================================================

echo "=============================================="
echo "RAFAELIA Multi-Architecture Builder"
echo "=============================================="
echo ""

# Detect NDK
NDK_PATH=$(detect_ndk)

if [[ -z "$NDK_PATH" ]]; then
    echo "[ERROR] Android NDK not found. Set NDK_DIR or ANDROID_NDK_HOME." >&2
    echo "        Download from: https://developer.android.com/ndk/downloads" >&2
    exit 1
fi

echo "NDK Path: $NDK_PATH"
echo "API Level: $API_LEVEL"
echo "Output: $OUTPUT_BASE"
echo "Architectures: ${ARCHS[*]}"
echo ""

# Track build results
declare -A build_results
total_success=0
total_failed=0

# Build for each architecture
for arch in "${ARCHS[@]}"; do
    echo "----------------------------------------------"
    echo "Building for: $arch"
    echo "----------------------------------------------"
    
    # Get Android ABI name
    abi=$(get_android_abi "$arch")
    
    # Create output directory for this architecture
    arch_output="$OUTPUT_BASE/$abi"
    mkdir -p "$arch_output"
    
    # Set environment for build_static.sh
    export NDK_DIR="$NDK_PATH"
    export NDK_ARCH="$arch"
    export NDK_API_LEVEL="$API_LEVEL"
    export OUT_DIR="$arch_output"
    
    # Run the build
    if "$SCRIPT_DIR/build_static.sh"; then
        build_results["$arch"]="SUCCESS"
        ((total_success++))
        echo "[✓] $arch build completed"
    else
        build_results["$arch"]="FAILED"
        ((total_failed++))
        echo "[✗] $arch build failed"
    fi
    
    echo ""
done

# ==============================================================================
# Build Summary
# ==============================================================================

echo "=============================================="
echo "Build Summary"
echo "=============================================="
echo ""

for arch in "${ARCHS[@]}"; do
    abi=$(get_android_abi "$arch")
    status="${build_results[$arch]:-SKIPPED}"
    
    if [[ "$status" == "SUCCESS" ]]; then
        echo "[✓] $arch ($abi): $status"
        
        # List built binaries
        if [[ -d "$OUTPUT_BASE/$abi" ]]; then
            for bin in "$OUTPUT_BASE/$abi"/*; do
                if [[ -f "$bin" ]] && [[ -x "$bin" ]]; then
                    size=$(du -h "$bin" | cut -f1)
                    echo "    - $(basename "$bin") ($size)"
                fi
            done
        fi
    else
        echo "[✗] $arch ($abi): $status"
    fi
done

echo ""
echo "Total: $total_success successful, $total_failed failed"
echo ""

# Generate architecture compatibility report
if [[ $total_success -gt 0 ]]; then
    echo "Architecture Compatibility Report:"
    echo "  - arm64-v8a:   Modern 64-bit ARM devices (Android 5.0+)"
    echo "  - armeabi-v7a: Older 32-bit ARM devices (Android 5.0+)"
    echo "  - x86_64:      64-bit Intel emulators and Chromebooks"
    echo "  - x86:         32-bit Intel emulators (legacy)"
    echo ""
    echo "Output directory: $OUTPUT_BASE"
fi

# Exit with appropriate code
if [[ $total_failed -gt 0 ]]; then
    exit 1
fi

exit 0
