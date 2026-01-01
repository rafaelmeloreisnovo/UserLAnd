#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Check if Android SDK is configured
has_android_sdk() {
  [[ -n "${ANDROID_HOME:-}" ]] || ([[ -f "$ROOT_DIR/local.properties" ]] && grep -q '^sdk.dir=' "$ROOT_DIR/local.properties")
}

# Build the APK using gradlew
build_apk() {
  (cd "$ROOT_DIR" && ./gradlew assembleDebug)
}

echo "[build] add2 (RAFAELIA multi-arch)"
(cd "$ROOT_DIR/add2" && ./build_multiarch.sh)

echo "[build] add (BLAKE3 example)"
(cd "$ROOT_DIR/add" && make -f Makefile.testing example)

want_apk="${1:-}"
if [[ "$want_apk" == "--apk" ]]; then
  echo "[build] UserLAnd APK"
  if has_android_sdk; then
    build_apk
  else
    echo "[error] Android SDK not configured. Set ANDROID_HOME or add sdk.dir=... to local.properties." >&2
    exit 1
  fi
else
  if has_android_sdk; then
    echo "[build] UserLAnd APK (auto)"
    build_apk
  else
    echo "[warn] Android SDK not configured; skipping APK build. Use --apk to require it." >&2
  fi
fi
