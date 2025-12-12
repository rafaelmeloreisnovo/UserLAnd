#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TASK="assembleRelease"
FORWARDED_ARGS=()

usage() {
  cat <<'USAGE'
Usage: scripts/build_unsigned_apk.sh [options]

Builds an unsigned APK by forwarding most options to scripts/build_apk.sh and
forcing signing to be disabled via -PunsignedApk=true and USERLAND_UNSIGNED_APK.

Options:
  --task <gradle_task>   Gradle task to run (default: assembleRelease).
  --sdk-dir <path>       Location to install or reuse the Android SDK.
  --skip-sdk             Do not download or update the Android SDK.
  --skip-build           Skip invoking Gradle after ensuring dependencies.
  -h, --help             Show this help message and exit.

Examples:
  scripts/build_unsigned_apk.sh
  scripts/build_unsigned_apk.sh --sdk-dir /opt/android-sdk --task bundleRelease
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --task)
      BUILD_TASK="$2"
      shift 2
      ;;
    --sdk-dir|--skip-sdk|--skip-build)
      FORWARDED_ARGS+=("$1")
      if [[ "$1" == "--sdk-dir" ]]; then
        FORWARDED_ARGS+=("$2")
        shift 2
      else
        shift
      fi
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

USERLAND_UNSIGNED_APK=1 "${ROOT_DIR}/scripts/build_apk.sh" \
  --gradle-arg "-PunsignedApk=true" \
  --task "$BUILD_TASK" "${FORWARDED_ARGS[@]}"
