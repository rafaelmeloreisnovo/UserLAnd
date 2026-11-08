#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
"$root/build.sh" all
for abi in armv7 arm64 host; do
  elf="$root/out/raf_linux_$abi"
  test -s "$elf"
  file_out=$(readelf -h "$elf")
  printf '%s' "$file_out" | grep -q 'Type:.*EXEC'
  case "$abi" in
    armv7) printf '%s' "$file_out" | grep -q 'Machine:.*ARM' ;;
    arm64) printf '%s' "$file_out" | grep -q 'Machine:.*AArch64' ;;
    host) printf '%s' "$file_out" | grep -q 'Machine:.*X86-64' ;;
  esac
  if readelf -l "$elf" | grep -Eq 'INTERP|DYNAMIC'; then
    echo "FAIL dynamic/interpreter: $abi" >&2; exit 1
  fi
  if readelf -d "$elf" 2>/dev/null | grep -q '(NEEDED)'; then
    echo "FAIL library dependency: $abi" >&2; exit 1
  fi
  if test -n "$(nm -u "$elf")"; then
    echo "FAIL unresolved symbols: $abi" >&2; exit 1
  fi
  printf 'PASS ELF_STATIC_NO_UNDEFINED %s\n' "$abi"
done
actual=$("$root/out/raf_linux_host")
test "$actual" = 'RAFAELIA_FREESTANDING_ABI_V1'
printf 'PASS HOST_EXECUTION\n'
if test -e /dev/full; then
  set +e
  "$root/out/raf_linux_host" >/dev/full 2>/dev/null
  failure_code=$?
  set -e
  test "$failure_code" -eq 74
  printf 'PASS HOST_WRITE_FAILURE_EXIT_74\n'
fi
printf 'NOT_RUN PHYSICAL_ANDROID_ARMV7_ARM64\n'
