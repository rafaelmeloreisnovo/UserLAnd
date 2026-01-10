#!/usr/bin/env bash
set -euo pipefail

# ==============================================================================
# build_static.sh - Cross-Architecture Static Binary Builder
# ==============================================================================
#
# This script builds low-level helper binaries with support for:
# - Native compilation for the host platform
# - Cross-compilation for Android using NDK
# - Automatic architecture detection and configuration
#
# Supported architectures:
# - Android: arm64-v8a, armeabi-v7a, x86_64, x86
# - Linux: x86_64, aarch64, arm, i386
# - macOS: arm64, x86_64
# ==============================================================================

# Repository root (one level above scripts/)
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Source path validation helpers (for advanced path checking if needed)
SCRIPT_DIR="$(dirname "${BASH_SOURCE[0]}")"
if [[ -f "$SCRIPT_DIR/path_validator.sh" ]]; then
  source "$SCRIPT_DIR/path_validator.sh"
fi

# Source auto-detection if available
if [[ -f "$SCRIPT_DIR/auto_detect_arch.sh" ]]; then
  source "$SCRIPT_DIR/auto_detect_arch.sh"
fi

normalize_and_warn() {
  local label="$1"
  local value="$2"
  local normalized="$value"
  local have_normalize=0
  local have_doubled=0
  local has_problem=0

  if declare -f normalize_path >/dev/null 2>&1; then
    have_normalize=1
  fi
  if declare -f has_doubled_path >/dev/null 2>&1; then
    have_doubled=1
  fi

  if (( have_doubled )) && has_doubled_path "$value"; then
    has_problem=1
  fi

  if (( has_problem )); then
    if (( have_normalize )); then
      normalized="$(normalize_path "$value")"
      if [[ "$normalized" != "$value" ]]; then
        echo "[WARN] Normalized $label from '$value' to '$normalized'" >&2
      else
        echo "[WARN] $label contains consecutive duplicate segments: $value" >&2
      fi
    else
      echo "[WARN] $label contains consecutive duplicate segments, but normalization is unavailable: $value" >&2
    fi
  elif (( have_normalize && ! have_doubled )); then
    echo "[WARN] Path normalization is available but doubled-path checking is unavailable; keeping $label unchanged: $value" >&2
  fi

  printf '%s\n' "$normalized"
}

# Set SRC_DIR with basic validation
# Note: /home/runner/work/UserLAnd/UserLAnd is the CORRECT GitHub Actions path
# (organization/repository structure), not a doubling error
SRC_DIR="$(normalize_and_warn "SRC_DIR" "${SRC_DIR:-"$ROOT_DIR"}")"

# Output directory for compiled binaries
OUT_DIR="$(normalize_and_warn "OUT_DIR" "${OUT_DIR:-"$ROOT_DIR/build/bin"}")"

# Default C compiler (overridden when the NDK is present)
CC="${CC:-cc}"

# Optional cross-target, e.g.:
#   TARGET="--target=x86_64-linux-musl"
TARGET="${TARGET:-}"

# Default CFLAGS: aggressive optimization for low-level baremetal operation
# -O3: maximum optimization level
# -pipe: use pipes instead of temp files (faster compilation)
# -fvisibility=hidden: reduce symbol visibility for smaller binaries
# -ffunction-sections -fdata-sections: enable dead code elimination
# -DNDEBUG: disable assertions in production
# -fomit-frame-pointer: smaller code, better register allocation (use -fno-omit-frame-pointer for debug builds)
# -fno-asynchronous-unwind-tables: no unwind tables (smaller binaries)
# NOTE: For debug builds, override with: CFLAGS="-O0 -g -fno-omit-frame-pointer"
CFLAGS="${CFLAGS:-"-O3 -pipe -fvisibility=hidden \
  -ffunction-sections -fdata-sections \
  -DNDEBUG -fomit-frame-pointer \
  -fno-asynchronous-unwind-tables"}"

# Extra linker flags: enable garbage collection of unused sections
# -Wl,--gc-sections: remove unused code and data
# -Wl,--as-needed: only link libraries actually used
# -Wl,-s: strip symbols for smaller binaries (omit for debug builds)
# NOTE: For debug builds, override with: LDFLAGS="-Wl,--gc-sections"
LDFLAGS="${LDFLAGS:-"-Wl,--gc-sections -Wl,--as-needed -Wl,-s"}"

# Discover an Android NDK toolchain so native helpers can be built for devices
# instead of the host. This prefers environment variables but also supports a
# checked-in placeholder directory at ./ndk.
choose_ndk_toolchain() {
  local host_tag
  local ndk_dir="${NDK_DIR:-${ANDROID_NDK_HOME:-${NDK_HOME:-${ANDROID_NDK:-}}}}"

  # Auto-detect NDK from common locations
  if [[ -z "$ndk_dir" ]]; then
    local possible_ndk_paths=(
      "$ROOT_DIR/ndk"
      "$HOME/Android/Sdk/ndk"
      "$HOME/Library/Android/sdk/ndk"
      "/usr/local/android-sdk/ndk"
      "/opt/android-sdk/ndk"
      "/usr/local/lib/android/sdk/ndk"
    )
    
    for path in "${possible_ndk_paths[@]}"; do
      if [[ -d "$path" ]]; then
        # Find the latest NDK version in the directory
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

  if [[ -z "$ndk_dir" ]]; then
    echo "[INFO] No NDK found; using host compiler for native builds." >&2
    return 0
  fi

  # Detect host OS and architecture for NDK prebuilt binaries
  local host_os host_arch
  host_os="$(uname -s)"
  host_arch="$(uname -m)"
  
  case "$host_os" in
    Linux)
      if [[ "$host_arch" == "x86_64" ]] || [[ "$host_arch" == "amd64" ]]; then
        host_tag="linux-x86_64"
      else
        echo "[WARN] Unsupported host architecture '$host_arch' for NDK on Linux." >&2
        return 0
      fi
      ;;
    Darwin)
      if [[ "$host_arch" == "arm64" ]]; then
        host_tag="darwin-x86_64"  # NDK uses Rosetta or universal binaries
      else
        host_tag="darwin-x86_64"
      fi
      ;;
    *)
      echo "[WARN] Unsupported host OS '$host_os' for NDK autodiscovery." >&2
      return 0
      ;;
  esac

  local toolchain_base="$ndk_dir/toolchains/llvm/prebuilt/${NDK_HOST_TAG:-$host_tag}"
  if [[ ! -x "$toolchain_base/bin/clang" ]]; then
    echo "[WARN] NDK toolchain not found at $toolchain_base; falling back to host compiler." >&2
    return 0
  fi

  # API level: use NDK_API_LEVEL or default to 21 (Android 5.0 - minSdk)
  local api="${NDK_API_LEVEL:-21}"
  
  # Architecture: use NDK_ARCH or auto-detect from host
  local arch="${NDK_ARCH:-}"
  if [[ -z "$arch" ]]; then
    # Auto-detect target architecture based on host
    case "$host_arch" in
      x86_64|amd64) arch="x86_64" ;;
      aarch64|arm64) arch="arm64" ;;
      armv7l|arm) arch="arm" ;;
      i686|i386) arch="x86" ;;
      *) arch="arm64" ;;  # Default to ARM64 for Android devices
    esac
  fi
  
  local target_triple
  case "$arch" in
    arm64|aarch64) target_triple="aarch64-linux-android${api}" ;;
    arm|armeabi-v7a) target_triple="armv7a-linux-androideabi${api}" ;;
    x86_64) target_triple="x86_64-linux-android${api}" ;;
    x86|i686) target_triple="i686-linux-android${api}" ;;
    *)
      echo "[ERROR] Unsupported NDK_ARCH '$arch'; valid values: arm, arm64, x86, x86_64." >&2
      return 1
      ;;
  esac

  CC="$toolchain_base/bin/clang"
  TARGET="--target=$target_triple"
  
  # Add architecture-specific compiler flags
  case "$arch" in
    arm64|aarch64)
      CFLAGS="$CFLAGS -march=armv8-a"
      ;;
    arm|armeabi-v7a)
      CFLAGS="$CFLAGS -march=armv7-a -mthumb -mfpu=neon"
      ;;
  esac
  
  # Add Android 15 16KB page alignment for API 35+
  if [[ "$api" -ge 35 ]]; then
    LDFLAGS="$LDFLAGS -Wl,-z,max-page-size=16384"
    echo "[INFO] Android 15+ detected; enabling 16KB page alignment." >&2
  fi
  
  echo "[INFO] Using NDK toolchain at $toolchain_base for $target_triple" >&2
}

if ! choose_ndk_toolchain; then
  exit 1
fi

mkdir -p "$OUT_DIR"

build() {
  local src="$1"
  local bin_name="$2"
  local out="$OUT_DIR/$bin_name"
  
  # Construct source path
  local src_path="$SRC_DIR/$src"
  
  # Verify source file exists
  if [[ ! -f "$src_path" ]]; then
    echo "[ERROR] Source file not found: $src_path" >&2
    return 1
  fi

  echo "[build] $src -> $out" >&2

  if [[ -n "$TARGET" ]]; then
    # Cross or special target (e.g. MUSL)
    "$CC" $TARGET $CFLAGS "$src_path" -o "$out" $LDFLAGS
  else
    # Native build (glibc / default system toolchain)
    "$CC" $CFLAGS "$src_path" -o "$out" $LDFLAGS
  fi
}

echo "RAFAELIA static/dynamic builder"
echo "Architecture: ${NDK_ARCH:-host}"
echo "Target: ${TARGET:-native}"

if ! command -v "$CC" >/dev/null 2>&1; then
  echo "[ERROR] Compiler '$CC' not found (set CC or install gcc/clang)" >&2
  exit 1
fi

# Core binaries to build. Extend this list as needed.
build raf_cpu_core.c raf_cpu_core
build raf_mem_core.c raf_mem_core
build raf_disk_core.c raf_disk_core
build raf_arch_probe.c raf_arch_probe

# Build Rafaelia module components
if [[ -f "$SRC_DIR/rafaelia/rafaelia_arch_probe.c" ]]; then
  build rafaelia/rafaelia_arch_probe.c rafaelia_arch_probe_v2
fi

echo "[SUCCESS] All binaries built to: $OUT_DIR" >&2

# ==============================================================================
# Multi-Architecture Build Support
# ==============================================================================
# To build for all Android architectures, run:
#   for arch in arm64 arm x86_64 x86; do
#     NDK_ARCH=$arch OUT_DIR=build/bin/$arch ./scripts/build_static.sh
#   done
# ==============================================================================
