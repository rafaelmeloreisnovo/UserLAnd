#!/usr/bin/env bash
#
# auto-repair-build.sh
# Attempts to automatically repair build issues by cleaning caches and rebuilding
#
# Usage: auto-repair-build.sh [build-task]
#
# Default build task is "assembleRelease"

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

BUILD_TASK="${1:-assembleRelease}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

log_info() {
  echo -e "${BLUE}ℹ️  $*${NC}"
}

log_success() {
  echo -e "${GREEN}✅ $*${NC}"
}

log_warning() {
  echo -e "${YELLOW}⚠️  $*${NC}"
}

log_error() {
  echo -e "${RED}❌ $*${NC}"
}

log_step() {
  echo ""
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
  echo -e "${BLUE}🔧 $*${NC}"
  echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
}

cd "$ROOT_DIR"

log_step "Step 1: Stopping Gradle daemons"
log_info "Terminating any running Gradle daemons..."
./gradlew --stop || log_warning "No Gradle daemons were running"
log_success "Gradle daemons stopped"

log_step "Step 2: Cleaning Gradle cache"
log_info "Clearing Gradle cache directories..."

# Clean local Gradle cache
if [[ -d "$HOME/.gradle/caches" ]]; then
  GRADLE_CACHE_DIR="$HOME/.gradle/caches"
  # Safety check: ensure path is non-empty and contains expected pattern
  if [[ -n "$GRADLE_CACHE_DIR" && "$GRADLE_CACHE_DIR" == *".gradle/caches"* && ${#GRADLE_CACHE_DIR} -gt 10 ]]; then
    log_info "Removing $GRADLE_CACHE_DIR"
    rm -rf "$GRADLE_CACHE_DIR"
    log_success "Local Gradle caches cleared"
  else
    log_error "Safety check failed for Gradle cache path: $GRADLE_CACHE_DIR"
    exit 1
  fi
else
  log_info "No local Gradle cache found"
fi

# Clean project .gradle directory
if [[ -d ".gradle" ]]; then
  PROJ_GRADLE_DIR=".gradle"
  # Safety check: ensure we're in project directory and path is safe
  if [[ -n "$PROJ_GRADLE_DIR" && "$PROJ_GRADLE_DIR" == ".gradle" && -f "$ROOT_DIR/gradlew" ]]; then
    log_info "Removing .gradle directory"
    rm -rf "$PROJ_GRADLE_DIR"
    log_success "Project .gradle directory cleared"
  else
    log_error "Safety check failed for project .gradle directory"
    exit 1
  fi
else
  log_info "No project .gradle directory found"
fi

log_step "Step 3: Removing build directories"
log_info "Cleaning build outputs..."

# Clean app build directory
if [[ -d "app/build" ]]; then
  APP_BUILD_DIR="app/build"
  # Safety check: ensure path contains expected pattern and is in project
  if [[ -n "$APP_BUILD_DIR" && "$APP_BUILD_DIR" == "app/build" && -f "$ROOT_DIR/app/build.gradle" ]]; then
    log_info "Removing $APP_BUILD_DIR"
    rm -rf "$APP_BUILD_DIR"
    log_success "App build directory cleared"
  else
    log_error "Safety check failed for app/build directory"
    exit 1
  fi
else
  log_info "No app/build directory found"
fi

# Clean core build directory
if [[ -d "core/build" ]]; then
  CORE_BUILD_DIR="core/build"
  # Safety check: ensure path contains expected pattern
  if [[ -n "$CORE_BUILD_DIR" && "$CORE_BUILD_DIR" == "core/build" ]]; then
    log_info "Removing $CORE_BUILD_DIR"
    rm -rf "$CORE_BUILD_DIR"
    log_success "Core build directory cleared"
  else
    log_error "Safety check failed for core/build directory"
    exit 1
  fi
else
  log_info "No core/build directory found"
fi

# Clean root build directory
if [[ -d "build" ]]; then
  ROOT_BUILD_DIR="build"
  # Safety check: ensure path is exactly "build" and we're in project root
  if [[ -n "$ROOT_BUILD_DIR" && "$ROOT_BUILD_DIR" == "build" && -f "$ROOT_DIR/gradlew" ]]; then
    log_info "Removing root build directory"
    rm -rf "$ROOT_BUILD_DIR"
    log_success "Root build directory cleared"
  else
    log_error "Safety check failed for root build directory"
    exit 1
  fi
else
  log_info "No root build directory found"
fi

log_step "Step 4: Resyncing dependencies"
log_info "Running dependency sync..."

if ./gradlew --no-daemon dependencies > /dev/null 2>&1; then
  log_success "Dependencies synced successfully"
else
  log_warning "Dependency sync had issues (may be normal)"
fi

log_step "Step 5: Fetching assets"
log_info "Downloading required assets..."

MAX_RETRIES=3
RETRY_COUNT=0

while [[ $RETRY_COUNT -lt $MAX_RETRIES ]]; do
  if ./gradlew --no-daemon :app:fetchAssets; then
    log_success "Assets fetched successfully"
    break
  else
    RETRY_COUNT=$((RETRY_COUNT + 1))
    if [[ $RETRY_COUNT -lt $MAX_RETRIES ]]; then
      log_warning "Asset fetch failed, retrying ($RETRY_COUNT/$MAX_RETRIES)..."
      sleep 5
    else
      log_error "Failed to fetch assets after $MAX_RETRIES attempts"
      exit 1
    fi
  fi
done

log_step "Step 6: Attempting rebuild"
log_info "Building with task: $BUILD_TASK"
log_info "Setting USERLAND_UNSIGNED_APK=1 for unsigned build"

export USERLAND_UNSIGNED_APK=1

if ./gradlew --no-daemon "$BUILD_TASK" -PuserlandUnsignedApk=true --stacktrace --info; then
  log_success "Rebuild completed successfully!"
  echo ""
  log_success "Auto-repair SUCCEEDED"
  echo ""
  log_info "Generated APKs:"
  find app/build/outputs/apk -name "*.apk" -exec ls -lh {} \; 2>/dev/null || log_info "No APKs found yet"
  exit 0
else
  log_error "Rebuild failed"
  echo ""
  log_error "Auto-repair FAILED"
  echo ""
  log_info "Possible issues to investigate:"
  echo "  • Network connectivity problems"
  echo "  • Missing NDK components"
  echo "  • Incompatible dependency versions"
  echo "  • Disk space issues"
  exit 1
fi
