#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# This helper builds an unsigned release APK even when signing credentials are
# present in the environment. It delegates to build_apk.sh so SDK bootstrapping
# and Java detection stay in one place.

exec "$ROOT_DIR/scripts/build_apk.sh" --task assembleRelease --gradle-arg "-PunsignedApk=true" "$@"
