#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_SDK_DIR="$ROOT_DIR/build/android-sdk"
SDK_DIR="${SDK_DIR:-$DEFAULT_SDK_DIR}"
TOOLS_DIR=""
DEFAULT_COMPILE_SDK="35"
DEFAULT_BUILD_TOOLS_VERSION="35.0.0"
APP_BUILD_GRADLE="$ROOT_DIR/app/build.gradle"
GRADLE_PROPERTIES="$ROOT_DIR/gradle.properties"
BUILD_TASK="assembleDebug"
SKIP_SDK=false
SKIP_BUILD=false
GRADLE_ARGS=()
ABI_FILTERS=""
REQUIRED_JAVA_MAJOR="17"
ARCHITECTURES=""

usage() {
  cat <<'USAGE'
Usage: scripts/build_apk.sh [options]

Prepares a local Android SDK and runs the Gradle task needed to build the APK.

Options:
  --task <gradle_task>   Gradle task to run (default: assembleDebug).
  --gradle-arg <arg>     Additional argument to forward to Gradle (repeatable).
  --arch <abi-list>      Comma-separated ABI list to build (e.g. arm64-v8a).
  --abi <abi_list>       Comma-separated ABI list to keep (e.g. arm64-v8a).
  --sdk-dir <path>       Location to install or reuse the Android SDK
                         (default: build/android-sdk).
  --skip-sdk             Do not download or update the Android SDK.
  --skip-build           Skip invoking Gradle after ensuring dependencies.
  -h, --help             Show this help message and exit.

The script expects Java 17 or newer. It will try to auto-detect a compatible
JAVA_HOME via $JAVA_HOME_11_X64, $JAVA_HOME, /usr/libexec/java_home (macOS), or
common Debian/Ubuntu locations such as /usr/lib/jvm/java-17-openjdk-amd64. Set
JAVA_HOME manually if none of those are available.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --task)
      if [[ $# -lt 2 || -z "${2:-}" || "$2" == -* ]]; then
        echo "Error: --task requires a non-empty value." >&2
        usage
        exit 1
      fi
      BUILD_TASK="$2"
      shift 2
      ;;
    --gradle-arg)
      if [[ $# -lt 2 || -z "${2:-}" ]]; then
        echo "Error: --gradle-arg requires a non-empty value." >&2
        usage
        exit 1
      fi
      GRADLE_ARGS+=("$2")
      shift 2
      ;;
    --arch)
      if [[ $# -lt 2 || -z "${2:-}" || "$2" == -* ]]; then
        echo "Error: --arch requires a non-empty value." >&2
        usage
        exit 1
      fi
      ARCHITECTURES="$2"
      shift 2
      ;;
    --abi)
      if [[ $# -lt 2 || -z "${2:-}" || "$2" == -* ]]; then
        echo "Error: --abi requires a non-empty value." >&2
        usage
        exit 1
      fi
      ABI_FILTERS="$2"
      GRADLE_ARGS+=("-PabiFilters=$2")
      shift 2
      ;;
    --sdk-dir)
      if [[ $# -lt 2 || -z "${2:-}" || "$2" == -* ]]; then
        echo "Error: --sdk-dir requires a non-empty value." >&2
        usage
        exit 1
      fi
      SDK_DIR="$2"
      shift 2
      ;;
    --skip-sdk)
      SKIP_SDK=true
      shift
      ;;
    --skip-build)
      SKIP_BUILD=true
      shift
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

TOOLS_DIR="$SDK_DIR/cmdline-tools/latest"

extract_compile_sdk_from_app() {
  if [[ ! -f "$APP_BUILD_GRADLE" ]]; then
    return 1
  fi
  sed -n 's/^[[:space:]]*compileSdk[[:space:]]*=?[[:space:]]*//p' "$APP_BUILD_GRADLE" | head -n1
}

extract_build_tools_version_from_app() {
  if [[ ! -f "$APP_BUILD_GRADLE" ]]; then
    return 1
  fi
  sed -n 's/^[[:space:]]*buildToolsVersion[[:space:]]*=?[[:space:]]*["'\'']\([^"'\'']*\)["'\''].*/\1/p' "$APP_BUILD_GRADLE" | head -n1
}

extract_ndk_version_from_properties() {
  if [[ ! -f "$GRADLE_PROPERTIES" ]]; then
    return 1
  fi
  sed -n 's/^[[:space:]]*ndkVersion[[:space:]]*=[[:space:]]*["'\'']\([^"'\'']*\)["'\''][[:space:]]*$/\1/p; s/^[[:space:]]*ndkVersion[[:space:]]*=[[:space:]]*\([^[:space:]]*\).*/\1/p' "$GRADLE_PROPERTIES" | head -n1
}

find_java_home() {
  local candidate
  for candidate in "${JAVA_HOME_11_X64:-}" "${JAVA_HOME:-}" "$(/usr/libexec/java_home -v ${REQUIRED_JAVA_MAJOR} 2>/dev/null || true)" \
    "/usr/lib/jvm/java-${REQUIRED_JAVA_MAJOR}-openjdk-amd64" "/usr/lib/jvm/java-${REQUIRED_JAVA_MAJOR}-openjdk"; do
    if [[ -n "$candidate" && -x "$candidate/bin/java" ]]; then
      "$candidate/bin/java" -version >/tmp/java_version.log 2>&1 || true
      java_version_output=$(cat /tmp/java_version.log)
      java_major=$(echo "$java_version_output" | sed -n 's/.*version "\([0-9]\{2\}\|[0-9]\)\..*/\1/p' | head -n1)
      if [[ -n "$java_major" && "$java_major" -ge "$REQUIRED_JAVA_MAJOR" ]]; then
        echo "$candidate"
        return 0
      fi
    fi
  done
  return 1
}

sdkmanager_failure() {
  local context="$1"
  local log_file="$2"
  local status="$3"
  echo "Error: sdkmanager failed while ${context} (exit code: ${status})." >&2
  echo "Probable causes: network instability, missing/invalid package, insufficient permissions in SDK_DIR, or invalid/corrupted cmdline-tools." >&2
  echo "Troubleshooting log: ${log_file}" >&2
  tail -n 30 "$log_file" >&2 || true
  exit "$status"
}

run_sdkmanager_licenses() {
  local log_file="$SDK_DIR/sdkmanager-licenses.log"
  local -a pipe_statuses=()
  local yes_status=0
  local sdk_status=0

  echo "Accepting Android SDK licenses..."
  set +e
  yes | "$TOOLS_DIR/bin/sdkmanager" --licenses >"$log_file" 2>&1
  pipe_statuses=("${PIPESTATUS[@]}")
  set -e

  yes_status="${pipe_statuses[0]:-0}"
  sdk_status="${pipe_statuses[1]:-0}"

  if [[ "$sdk_status" -ne 0 ]]; then
    sdkmanager_failure "accepting licenses" "$log_file" "$sdk_status"
  fi

  if [[ "$yes_status" -ne 0 && "$yes_status" -ne 141 ]]; then
    echo "Error: unexpected 'yes' exit while piping licenses (exit code: $yes_status)." >&2
    echo "Troubleshooting log: $log_file" >&2
    tail -n 30 "$log_file" >&2 || true
    exit "$yes_status"
  fi

  echo "Licenses accepted. Log: $log_file"
}

run_sdkmanager_install() {
  local log_file="$SDK_DIR/sdkmanager-install.log"
  local status=0

  echo "Installing Android SDK packages: $*"
  set +e
  "$TOOLS_DIR/bin/sdkmanager" --install "$@" >"$log_file" 2>&1
  status=$?
  set -e

  if [[ "$status" -ne 0 ]]; then
    sdkmanager_failure "installing packages" "$log_file" "$status"
  fi

  echo "Packages installed. Log: $log_file"
}

COMPILE_SDK_VERSION="$(extract_compile_sdk_from_app || true)"
if [[ -z "$COMPILE_SDK_VERSION" ]]; then
  COMPILE_SDK_VERSION="$DEFAULT_COMPILE_SDK"
fi

BUILD_TOOLS_VERSION="$(extract_build_tools_version_from_app || true)"
if [[ -z "$BUILD_TOOLS_VERSION" ]]; then
  BUILD_TOOLS_VERSION="$DEFAULT_BUILD_TOOLS_VERSION"
fi

NDK_VERSION="$(extract_ndk_version_from_properties || true)"

PLATFORM_PACKAGE="platforms;android-${COMPILE_SDK_VERSION}"
BUILD_TOOLS_PACKAGE="build-tools;${BUILD_TOOLS_VERSION}"

JAVA_HOME_VALUE=$(find_java_home || true)
if [[ -z "$JAVA_HOME_VALUE" ]]; then
  echo "Java ${REQUIRED_JAVA_MAJOR}+ was not found. Install it and set JAVA_HOME (or JAVA_HOME_11_X64)." >&2
  exit 1
fi
export JAVA_HOME="$JAVA_HOME_VALUE"
export PATH="$JAVA_HOME/bin:$PATH"
echo "Using JAVA_HOME=$JAVA_HOME"

if [[ "$SKIP_SDK" == false ]]; then
  mkdir -p "$SDK_DIR"
  if [[ ! -x "$TOOLS_DIR/bin/sdkmanager" ]]; then
    echo "Downloading Android command line tools into $SDK_DIR ..."
    case "$(uname -s)" in
      Linux)
        CMDLINE_TOOLS_ZIP="commandlinetools-linux-9477386_latest.zip"
        ;;
      Darwin)
        CMDLINE_TOOLS_ZIP="commandlinetools-mac-9477386_latest.zip"
        ;;
      MINGW*|MSYS*|CYGWIN*)
        CMDLINE_TOOLS_ZIP="commandlinetools-win-9477386_latest.zip"
        ;;
      *)
        echo "Error: unsupported host OS '$(uname -s)' for Android cmdline-tools download. Supported: Linux, Darwin, MINGW/MSYS/CYGWIN." >&2
        exit 1
        ;;
    esac
    tmpdir=$(mktemp -d)
    pushd "$tmpdir" >/dev/null
    curl -LO "https://dl.google.com/android/repository/$CMDLINE_TOOLS_ZIP"
    unzip -q "$CMDLINE_TOOLS_ZIP"
    mkdir -p "$SDK_DIR/cmdline-tools"
    mv cmdline-tools "$TOOLS_DIR"
    popd >/dev/null
    # Safety check before removing tmpdir
    if [[ -n "$tmpdir" && "$tmpdir" =~ ^/tmp/ && ${#tmpdir} -gt 5 && -d "$tmpdir" ]]; then
      rm -rf "$tmpdir"
    else
      echo "Warning: Skipped tmpdir removal due to safety check: $tmpdir" >&2
    fi
  fi

  export ANDROID_SDK_ROOT="$SDK_DIR"
  run_sdkmanager_licenses
  SDK_PACKAGES=("platform-tools" "$PLATFORM_PACKAGE" "$BUILD_TOOLS_PACKAGE")
  if [[ -n "$NDK_VERSION" ]]; then
    SDK_PACKAGES+=("ndk;${NDK_VERSION}")
  fi
  run_sdkmanager_install "${SDK_PACKAGES[@]}"
else
  echo "Skipping SDK install/update as requested."
  if [[ ! -x "$TOOLS_DIR/bin/sdkmanager" ]]; then
    if [[ ! -d "$SDK_DIR" ]]; then
      echo "SDK directory $SDK_DIR does not exist. Remove --skip-sdk or provide --sdk-dir pointing to a valid SDK." >&2
    else
      echo "Android command line tools not found in $TOOLS_DIR. Remove --skip-sdk to bootstrap them." >&2
    fi
    exit 1
  fi
fi

export ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$SDK_DIR}"

write_local_properties() {
  local existing_sdk_dir=""
  local escaped_sdk_dir=""

  escaped_sdk_dir="${SDK_DIR//\\/\\\\}"
  escaped_sdk_dir="${escaped_sdk_dir// /\\ }"

  if [[ -f "$ROOT_DIR/local.properties" ]]; then
    existing_sdk_dir=$(grep -m1 '^sdk.dir=' "$ROOT_DIR/local.properties" | cut -d'=' -f2- || true)
  fi

  if [[ "$existing_sdk_dir" == "$escaped_sdk_dir" ]]; then
    echo "local.properties already points at $SDK_DIR"
    return
  fi

  cat > "$ROOT_DIR/local.properties" <<EOF_LOCAL
sdk.dir=$escaped_sdk_dir
EOF_LOCAL
  echo "Wrote local.properties pointing at $SDK_DIR"
}

write_local_properties

if [[ "$SKIP_BUILD" == true ]]; then
  echo "Skipping Gradle build (--skip-build)."
  exit 0
fi

if [[ -n "$ARCHITECTURES" ]]; then
  GRADLE_ARGS+=("-Parchitectures=$ARCHITECTURES")
fi

"$ROOT_DIR/gradlew" "$BUILD_TASK" ${GRADLE_ARGS[@]+"${GRADLE_ARGS[@]}"}
