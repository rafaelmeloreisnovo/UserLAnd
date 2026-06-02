#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ABIS="arm64-v8a,armeabi-v7a"
SDK_ARGS=()
SIGNED=false
BUILD_DEBUG=true
BUILD_BETA=true
DIST_ROOT="$ROOT_DIR/dist/beta-apks"

usage() {
  cat <<'USAGE'
Usage: scripts/build_beta_artifacts.sh [options]

Builds and validates per-ABI debug and beta APK artifacts. By default beta APKs
are unsigned for safe CI pretesting. Pass --signed to require the existing
ANDROID_KEY_* environment and keystore.jks signing path.

Options:
  --abis <list>       Comma-separated ABI list (default: arm64-v8a,armeabi-v7a).
  --signed            Build beta with configured signing credentials.
  --unsigned          Force unsigned beta (default).
  --no-debug          Skip debug APK build.
  --no-beta           Skip beta APK build.
  --sdk-dir <path>    Forward SDK directory to scripts/build_apk.sh.
  --skip-sdk          Forward --skip-sdk to scripts/build_apk.sh.
  --dist <path>       Output directory (default: dist/beta-apks).
  -h, --help          Show help.
USAGE
}

abi_suffix() {
  case "$1" in
    arm64-v8a) echo arm64 ;;
    armeabi-v7a) echo arm32 ;;
    x86_64) echo x86_64 ;;
    x86) echo x86 ;;
    *) echo "$1" | tr -c 'A-Za-z0-9._-' '_' ;;
  esac
}

latest_apk() {
  local dir="$1"
  find "$dir" -maxdepth 1 -name '*.apk' -type f -printf '%T@ %p\n' | sort -nr | awk 'NR==1 {print $2}'
}

copy_and_validate() {
  local src="$1"
  local abi="$2"
  local variant="$3"
  local signing_expect="$4"
  local suffix out_dir out_apk report

  suffix=$(abi_suffix "$abi")
  out_dir="$DIST_ROOT/$suffix/$variant"
  mkdir -p "$out_dir"
  out_apk="$out_dir/userland-${variant}-${suffix}.apk"
  report="$out_dir/userland-${variant}-${suffix}.report"

  cp "$src" "$out_apk"
  "$ROOT_DIR/scripts/validate_apk.sh" --apk "$out_apk" --abi "$abi" "$signing_expect" --report "$report"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --abis)
      ABIS="${2-}"
      shift 2
      ;;
    --signed)
      SIGNED=true
      shift
      ;;
    --unsigned)
      SIGNED=false
      shift
      ;;
    --no-debug)
      BUILD_DEBUG=false
      shift
      ;;
    --no-beta)
      BUILD_BETA=false
      shift
      ;;
    --sdk-dir)
      SDK_ARGS+=(--sdk-dir "${2-}")
      shift 2
      ;;
    --skip-sdk)
      SDK_ARGS+=(--skip-sdk)
      shift
      ;;
    --dist)
      DIST_ROOT="${2-}"
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

IFS=',' read -r -a abi_array <<< "$ABIS"
mkdir -p "$DIST_ROOT"
summary="$DIST_ROOT/SUMMARY.tsv"
printf 'variant\tabi\tpath\tsize_bytes\tsha256\tsigning\n' > "$summary"

for abi in "${abi_array[@]}"; do
  abi="$(echo "$abi" | xargs)"
  [[ -z "$abi" ]] && continue
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  echo "Building artifacts for ABI: $abi"

  if [[ "$BUILD_DEBUG" == true ]]; then
    "$ROOT_DIR/scripts/build_apk.sh" --task :app:assembleDebug --abi "$abi" "${SDK_ARGS[@]}"
    debug_apk=$(latest_apk "$ROOT_DIR/app/build/outputs/apk/debug")
    copy_and_validate "$debug_apk" "$abi" debug --expect-signed
  fi

  if [[ "$BUILD_BETA" == true ]]; then
    if [[ "$SIGNED" == true ]]; then
      "$ROOT_DIR/scripts/build_apk.sh" --task :app:assembleBeta --abi "$abi" "${SDK_ARGS[@]}"
      beta_signing=--expect-signed
    else
      USERLAND_UNSIGNED_APK=1 "$ROOT_DIR/scripts/build_apk.sh" \
        --task :app:assembleBeta \
        --abi "$abi" \
        --gradle-arg -PunsignedApk=true \
        --gradle-arg -PuserlandUnsignedApk=true \
        "${SDK_ARGS[@]}"
      beta_signing=--expect-unsigned
    fi
    beta_apk=$(latest_apk "$ROOT_DIR/app/build/outputs/apk/beta")
    copy_and_validate "$beta_apk" "$abi" beta "$beta_signing"
  fi
done

while IFS= read -r report; do
  variant=$(basename "$(dirname "$report")")
  abi=$(sed -n 's/^ABI=//p' "$report")
  path=$(sed -n 's/^APK=//p' "$report")
  size=$(sed -n 's/^SIZE_BYTES=//p' "$report")
  sha=$(sed -n 's/^SHA256=//p' "$report")
  signing=$(sed -n 's/^SIGNING=//p' "$report")
  printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$variant" "$abi" "$path" "$size" "$sha" "$signing" >> "$summary"
done < <(find "$DIST_ROOT" -name '*.report' -type f | sort)

find "$DIST_ROOT" -name '*.apk' -type f -print0 | xargs -0 -r sha256sum > "$DIST_ROOT/SHA256SUMS.txt"

echo "✅ Build artifact summary: $summary"
cat "$summary"
