#!/usr/bin/env bash
set -euo pipefail

APK_PATH=""
TARGET_ABI=""
EXPECT_SIGNING="any"
MIN_SIZE_BYTES="1048576"
REPORT_PATH=""

usage() {
  cat <<'USAGE'
Usage: scripts/validate_apk.sh --apk <path> --abi <abi> [options]

Validates an APK before artifact upload. The checks are intentionally generic
so debug, beta, release, signed, and unsigned APKs can all be tested per ABI.

Options:
  --apk <path>              APK to validate.
  --abi <abi>               Required ABI, e.g. arm64-v8a or armeabi-v7a.
  --expect-signed           Fail if the APK has no META-INF signing block.
  --expect-unsigned         Fail if the APK has a META-INF signing block.
  --min-size-bytes <bytes>  Minimum APK size (default: 1048576).
  --report <path>           Write a machine-readable report.
  -h, --help                Show help.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --apk)
      APK_PATH="${2-}"
      shift 2
      ;;
    --abi)
      TARGET_ABI="${2-}"
      shift 2
      ;;
    --expect-signed)
      EXPECT_SIGNING="signed"
      shift
      ;;
    --expect-unsigned)
      EXPECT_SIGNING="unsigned"
      shift
      ;;
    --min-size-bytes)
      MIN_SIZE_BYTES="${2-}"
      shift 2
      ;;
    --report)
      REPORT_PATH="${2-}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 1
      ;;
  esac
done

if [[ -z "$APK_PATH" || -z "$TARGET_ABI" ]]; then
  echo "Error: --apk and --abi are required." >&2
  usage
  exit 1
fi

if [[ ! -f "$APK_PATH" ]]; then
  echo "Error: APK file not found: $APK_PATH" >&2
  exit 1
fi

if ! [[ "$MIN_SIZE_BYTES" =~ ^[0-9]+$ ]]; then
  echo "Error: --min-size-bytes must be numeric." >&2
  exit 1
fi

apk_name=$(basename "$APK_PATH")
echo "🔍 Validating $apk_name for ABI $TARGET_ABI"

unzip -t "$APK_PATH" >/dev/null

declared_entries=$(unzip -Z1 "$APK_PATH")
for required in AndroidManifest.xml classes.dex; do
  if ! printf '%s\n' "$declared_entries" | grep -qx "$required"; then
    echo "Error: missing required APK entry: $required" >&2
    exit 1
  fi
done

if [[ "$(uname -s)" == "Darwin" ]]; then
  apk_size=$(stat -f%z "$APK_PATH")
else
  apk_size=$(stat -c%s "$APK_PATH")
fi

if [[ "$apk_size" -lt "$MIN_SIZE_BYTES" ]]; then
  echo "Error: APK too small: ${apk_size} bytes < ${MIN_SIZE_BYTES} bytes" >&2
  exit 1
fi

target_libs=$(printf '%s\n' "$declared_entries" | grep "^lib/${TARGET_ABI}/.*\.so$" || true)
if [[ -z "$target_libs" ]]; then
  echo "Error: no native libraries found for $TARGET_ABI" >&2
  printf '%s\n' "$declared_entries" | grep '^lib/' || true
  exit 1
fi

other_libs=$(printf '%s\n' "$declared_entries" | grep '^lib/.*\.so$' | grep -v "^lib/${TARGET_ABI}/" || true)
if [[ -n "$other_libs" ]]; then
  echo "Error: APK contains non-target ABI libraries:" >&2
  echo "$other_libs" >&2
  exit 1
fi

signing_entries=$(printf '%s\n' "$declared_entries" | grep -E '^META-INF/.*\.(RSA|DSA|EC)$' || true)
signing_status="unsigned"
if [[ -n "$signing_entries" ]]; then
  signing_status="signed"
fi

apksigner_bin=""
for candidate in \
  "${ANDROID_HOME:-}/build-tools"/*/apksigner \
  "${ANDROID_SDK_ROOT:-}/build-tools"/*/apksigner; do
  if [[ -x "$candidate" ]]; then
    apksigner_bin="$candidate"
  fi
done

if [[ -n "$apksigner_bin" ]]; then
  if "$apksigner_bin" verify "$APK_PATH" >/dev/null 2>&1; then
    signing_status="signed"
  else
    signing_status="unsigned"
  fi
fi

case "$EXPECT_SIGNING" in
  signed)
    if [[ "$signing_status" != "signed" ]]; then
      echo "Error: APK expected signed but signing block was not found." >&2
      exit 1
    fi
    ;;
  unsigned)
    if [[ "$signing_status" != "unsigned" ]]; then
      echo "Error: APK expected unsigned but signing block was found." >&2
      exit 1
    fi
    ;;
esac

sha256=$(sha256sum "$APK_PATH" | awk '{print $1}')
lib_count=$(printf '%s\n' "$target_libs" | sed '/^$/d' | wc -l | tr -d '[:space:]')

{
  echo "APK=$APK_PATH"
  echo "ABI=$TARGET_ABI"
  echo "SIZE_BYTES=$apk_size"
  echo "SHA256=$sha256"
  echo "SIGNING=$signing_status"
  echo "NATIVE_LIB_COUNT=$lib_count"
} | tee "${REPORT_PATH:-/dev/stdout}" >/dev/null

echo "✅ APK validation passed"
echo "  • path: $APK_PATH"
echo "  • abi: $TARGET_ABI"
echo "  • size_bytes: $apk_size"
echo "  • sha256: $sha256"
echo "  • signing: $signing_status"
echo "  • native_lib_count: $lib_count"
