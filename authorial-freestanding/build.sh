#!/bin/sh
# Compiler/linker are build-time tools, never linked runtime dependencies.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${OUT_DIR:-"$root/out"}
cc=${CC:-clang}
mkdir -p "$out"

case "${1:-all}" in
  armv7) targets='armv7' ;;
  arm64) targets='arm64' ;;
  host) targets='host' ;;
  all) targets='armv7 arm64 host' ;;
  *) echo 'usage: build.sh {armv7|arm64|host|all}' >&2; exit 64 ;;
esac
for abi in $targets; do
  case "$abi" in
    armv7) target='armv7a-linux-gnueabi'; abi_flags='-marm -mfloat-abi=soft' ;;
    arm64) target='aarch64-linux-gnu'; abi_flags='' ;;
    host) target='x86_64-linux-gnu'; abi_flags='' ;;
  esac
  # shellcheck disable=SC2086 -- flags are controlled internal literals.
  "$cc" --target="$target" -fuse-ld=lld \
    -std=c11 -O2 -Wall -Wextra -Werror -Wconversion \
    -ffreestanding -fno-builtin -fno-stack-protector \
    -fno-pic -fno-pie -fomit-frame-pointer \
    -ffunction-sections -fdata-sections -nostdlib -static \
    -Wl,--build-id=none -Wl,--gc-sections -Wl,-e,_start -Wl,--fatal-warnings \
    $abi_flags -I"$root/include" \
    "$root/src/entry.c" "$root/src/syscall_linux.c" \
    -o "$out/raf_linux_$abi"
  printf 'BUILT %s\n' "$out/raf_linux_$abi"
done
