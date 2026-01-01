#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "[build] add2 (RAFAELIA multi-arch)"
(cd "$ROOT_DIR/add2" && ./build_multiarch.sh)

echo "[build] add (BLAKE3 example)"
(cd "$ROOT_DIR/add" && make -f Makefile.testing example)

want_apk="${1:-}"
if [[ "$want_apk" == "--apk" ]]; then
  echo "[build] UserLAnd APK"
  if [[ -n "${ANDROID_HOME:-}" ]]; then
    (cd "$ROOT_DIR" && ./gradlew assembleDebug)
  elif [[ -f "$ROOT_DIR/local.properties" ]] && grep -q '^sdk.dir=' "$ROOT_DIR/local.properties"; then
    (cd "$ROOT_DIR" && ./gradlew assembleDebug)
  else
    echo "[error] Android SDK not configured. Set ANDROID_HOME or add sdk.dir=... to local.properties." >&2
    exit 1
  fi
else
  if [[ -n "${ANDROID_HOME:-}" ]] || ([[ -f "$ROOT_DIR/local.properties" ]] && grep -q '^sdk.dir=' "$ROOT_DIR/local.properties"); then
    echo "[build] UserLAnd APK (auto)"
    (cd "$ROOT_DIR" && ./gradlew assembleDebug)
  else
    echo "[warn] Android SDK not configured; skipping APK build. Use --apk to require it." >&2
  fi
fi
