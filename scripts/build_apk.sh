#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_SDK_DIR="$ROOT_DIR/build/android-sdk"
SDK_DIR="${SDK_DIR:-$DEFAULT_SDK_DIR}"
TOOLS_DIR=""
DEFAULT_COMPILE_SDK="35"
DEFAULT_BUILD_TOOLS_VERSION="35.0.0"
APP_BUILD_GRADLE="$ROOT_DIR/app/build.gradle"
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
      BUILD_TASK="$2"
      shift 2
      ;;
    --gradle-arg)
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

COMPILE_SDK_VERSION="$(extract_compile_sdk_from_app || true)"
if [[ -z "$COMPILE_SDK_VERSION" ]]; then
  COMPILE_SDK_VERSION="$DEFAULT_COMPILE_SDK"
fi

BUILD_TOOLS_VERSION="$(extract_build_tools_version_from_app || true)"
if [[ -z "$BUILD_TOOLS_VERSION" ]]; then
  BUILD_TOOLS_VERSION="$DEFAULT_BUILD_TOOLS_VERSION"
fi

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
    tmpdir=$(mktemp -d)
    pushd "$tmpdir" >/dev/null
    curl -LO https://dl.google.com/android/repository/commandlinetools-linux-9477386_latest.zip
    unzip -q commandlinetools-linux-9477386_latest.zip
    mkdir -p "$SDK_DIR/cmdline-tools"
    mv cmdline-tools "$TOOLS_DIR"
    popd >/dev/null
    rm -rf "$tmpdir"
  fi

  export ANDROID_SDK_ROOT="$SDK_DIR"
  # sdkmanager closes the input pipe after processing all licenses, which can
  # deliver SIGPIPE to `yes` under `set -o pipefail`. Allow that benign exit
  # while still accepting all licenses.
  yes | "$TOOLS_DIR/bin/sdkmanager" --licenses >/dev/null || true
  "$TOOLS_DIR/bin/sdkmanager" --install "platform-tools" "$PLATFORM_PACKAGE" "$BUILD_TOOLS_PACKAGE"
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
  if [[ -f "$ROOT_DIR/local.properties" ]]; then
    existing_sdk_dir=$(grep '^sdk.dir=' "$ROOT_DIR/local.properties" | head -n1 | cut -d'=' -f2-)
  fi

  if [[ "$existing_sdk_dir" == "$SDK_DIR" ]]; then
    echo "local.properties already points at $SDK_DIR"
    return
  fi

  cat > "$ROOT_DIR/local.properties" <<EOF_LOCAL
sdk.dir=$SDK_DIR
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
if [[ -n "$ABI_FILTERS" ]]; then
  GRADLE_ARGS+=("-PabiFilters=$ABI_FILTERS")
fi

"$ROOT_DIR/gradlew" "$BUILD_TASK" ${GRADLE_ARGS[@]+"${GRADLE_ARGS[@]}"}
