#!/usr/bin/env bash
set -euo pipefail

# Repository root (one level above scripts/)
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="${SRC_DIR:-"$ROOT_DIR"}"

# Output directory for compiled binaries
OUT_DIR="${OUT_DIR:-"$ROOT_DIR/build/bin"}"

# Default C compiler (overridden when the NDK is present)
CC="${CC:-cc}"

# Optional cross-target, e.g.:
#   TARGET="--target=x86_64-linux-musl"
TARGET="${TARGET:-}"

# Default CFLAGS: optimized, hidden symbols, no -static by default.
CFLAGS="${CFLAGS:-"-O2 -pipe -fvisibility=hidden"}"

# Extra linker flags, if any.
LDFLAGS="${LDFLAGS:-}"

# Discover an Android NDK toolchain so native helpers can be built for devices
# instead of the host. This prefers environment variables but also supports a
# checked-in placeholder directory at ./ndk.
choose_ndk_toolchain() {
  local host_tag
  local ndk_dir="${NDK_DIR:-${ANDROID_NDK_HOME:-${NDK_HOME:-}}}"

  if [[ -z "$ndk_dir" && -d "$ROOT_DIR/ndk" ]]; then
    ndk_dir="$ROOT_DIR/ndk"
  fi

  if [[ -z "$ndk_dir" ]]; then
    return 0
  fi

  case "$(uname -s)" in
    Linux) host_tag="linux-x86_64" ;;
    Darwin) host_tag="darwin-x86_64" ;;
    *)
      echo "Unsupported host OS for NDK autodiscovery." >&2
      return 0
      ;;
  esac

  local toolchain_base="$ndk_dir/toolchains/llvm/prebuilt/${NDK_HOST_TAG:-$host_tag}"
  if [[ ! -x "$toolchain_base/bin/clang" ]]; then
    echo "NDK toolchain not found at $toolchain_base; falling back to host compiler." >&2
    return 0
  fi

  local api="${NDK_API_LEVEL:-21}"
  local arch="${NDK_ARCH:-arm64}"
  local target_triple
  case "$arch" in
    arm64) target_triple="aarch64-linux-android${api}" ;;
    arm) target_triple="armv7a-linux-androideabi${api}" ;;
    x86_64) target_triple="x86_64-linux-android${api}" ;;
    x86) target_triple="i686-linux-android${api}" ;;
    *)
      echo "Unsupported NDK_ARCH '$arch'; valid values: arm, arm64, x86, x86_64." >&2
      return 1
      ;;
  esac

  CC="$toolchain_base/bin/clang"
  TARGET="--target=$target_triple"
  echo "Using NDK toolchain at $toolchain_base for $target_triple" >&2
}

if ! choose_ndk_toolchain; then
  exit 1
fi

mkdir -p "$OUT_DIR"

build() {
  local src="$1"
  local bin_name="$2"
  local out="$OUT_DIR/$bin_name"

  echo "[build] $src -> $out" >&2

  if [[ -n "$TARGET" ]]; then
    # Cross or special target (e.g. MUSL)
    "$CC" $TARGET $CFLAGS "$SRC_DIR/$src" -o "$out" $LDFLAGS
  else
    # Native build (glibc / default system toolchain)
    "$CC" $CFLAGS "$SRC_DIR/$src" -o "$out" $LDFLAGS
  fi
}

echo "RAFAELIA static/dynamic builder"

if ! command -v "$CC" >/dev/null 2>&1; then
  echo "Compiler '$CC' not found (set CC or install gcc/clang)" >&2
  exit 1
fi

# Core binaries to build. Extend this list as needed.
build raf_cpu_core.c raf_cpu_core
build raf_mem_core.c raf_mem_core
build raf_disk_core.c raf_disk_core
build raf_arch_probe.c raf_arch_probe
