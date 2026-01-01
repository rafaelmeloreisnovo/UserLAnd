#!/usr/bin/env bash
set -euo pipefail

# Repository root (one level above scripts/)
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Source path validation helpers (for advanced path checking if needed)
SCRIPT_DIR="$(dirname "${BASH_SOURCE[0]}")"
if [[ -f "$SCRIPT_DIR/path_validator.sh" ]]; then
  source "$SCRIPT_DIR/path_validator.sh"
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
# -flto: link-time optimization (optional, requires linker support)
# -DNDEBUG: disable assertions in production
# -fno-exceptions: disable C++ exceptions (smaller code, C doesn't need this but safe)
# -fomit-frame-pointer: smaller code, better register allocation
CFLAGS="${CFLAGS:-"-O3 -pipe -fvisibility=hidden -ffunction-sections -fdata-sections -DNDEBUG -fomit-frame-pointer -fno-asynchronous-unwind-tables"}"

# Extra linker flags: enable garbage collection of unused sections
# -Wl,--gc-sections: remove unused code and data
# -Wl,--as-needed: only link libraries actually used
# -Wl,-s: strip symbols for smaller binaries
LDFLAGS="${LDFLAGS:-"-Wl,--gc-sections -Wl,--as-needed -Wl,-s"}"

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

if ! command -v "$CC" >/dev/null 2>&1; then
  echo "Compiler '$CC' not found (set CC or install gcc/clang)" >&2
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
