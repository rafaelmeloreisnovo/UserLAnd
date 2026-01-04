#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
# Script to ensure compilation requirements are met and validate build integrity
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Exit codes
EXIT_SUCCESS=0
EXIT_FAILURE=1

# Counters
CHECKS_PASSED=0
CHECKS_FAILED=0
CHECKS_WARNED=0

log_info() {
    echo -e "${BLUE}[INFO]${NC} $*"
}

log_success() {
    echo -e "${GREEN}[✓]${NC} $*"
    ((CHECKS_PASSED++))
}

log_warning() {
    echo -e "${YELLOW}[⚠]${NC} $*"
    ((CHECKS_WARNED++))
}

log_error() {
    echo -e "${RED}[✗]${NC} $*"
    ((CHECKS_FAILED++))
}

print_separator() {
    echo "================================================================"
}

# Function to check Java version
check_java_version() {
    log_info "Checking Java version..."
    
    if ! command -v java &> /dev/null; then
        log_error "Java is not installed"
        return 1
    fi
    
    local java_version
    # Handle different Java version formats (e.g., "17.0.x", "1.8.0_xyz")
    java_version=$(java -version 2>&1 | grep -i version | \
        sed -E 's/.*version "?([0-9]+)\.?([0-9]+)?.*"?/\1/' | head -n1)
    
    # For Java 8 and earlier (1.8.0 format), extract the minor version
    if [[ "$java_version" == "1" ]]; then
        java_version=$(java -version 2>&1 | grep -i version | \
            sed -E 's/.*version "?1\.([0-9]+).*"?/\1/' | head -n1)
    fi
    
    if [[ -n "$java_version" ]] && [[ "$java_version" -ge 17 ]]; then
        log_success "Java version $java_version detected (required: 17+)"
    else
        log_error "Java version $java_version is too old (required: 17+)"
        return 1
    fi
}

# Function to check Gradle wrapper
check_gradle_wrapper() {
    log_info "Checking Gradle wrapper..."
    
    if [[ ! -f "$ROOT_DIR/gradlew" ]]; then
        log_error "Gradle wrapper not found"
        return 1
    fi
    
    if [[ ! -x "$ROOT_DIR/gradlew" ]]; then
        log_warning "Gradle wrapper is not executable, fixing..."
        chmod +x "$ROOT_DIR/gradlew"
    fi
    
    log_success "Gradle wrapper is present and executable"
}

# Function to check Gradle version
check_gradle_version() {
    log_info "Checking Gradle version..."
    
    if ./gradlew --version &> /dev/null; then
        local gradle_version
        gradle_version=$(./gradlew --version | grep "^Gradle" | awk '{print $2}')
        log_success "Gradle version $gradle_version"
    else
        log_error "Failed to get Gradle version"
        return 1
    fi
}

# Function to check Android SDK
check_android_sdk() {
    log_info "Checking Android SDK..."
    
    local sdk_dir=""
    
    # Check local.properties
    if [[ -f "$ROOT_DIR/local.properties" ]]; then
        sdk_dir=$(grep "^sdk.dir=" "$ROOT_DIR/local.properties" | cut -d= -f2)
    fi
    
    # Check environment variables
    if [[ -z "$sdk_dir" ]]; then
        sdk_dir="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}"
    fi
    
    if [[ -n "$sdk_dir" ]] && [[ -d "$sdk_dir" ]]; then
        log_success "Android SDK found at: $sdk_dir"
    else
        log_warning "Android SDK not configured (will be downloaded on build)"
    fi
}

# Function to check NDK
check_ndk() {
    log_info "Checking Android NDK..."
    
    local expected_ndk
    expected_ndk=$(grep "^ndkVersion=" "$ROOT_DIR/gradle.properties" | cut -d= -f2)
    
    if [[ -n "$expected_ndk" ]]; then
        log_info "Expected NDK version: $expected_ndk"
    fi
    
    local ndk_dir="${ANDROID_NDK_HOME:-${NDK_HOME:-}}"
    
    if [[ -n "$ndk_dir" ]] && [[ -d "$ndk_dir" ]]; then
        log_success "NDK found at: $ndk_dir"
    else
        log_warning "NDK not configured (will be downloaded on build)"
    fi
}

# Function to validate build.gradle files
check_build_files() {
    log_info "Checking build configuration files..."
    
    local required_files=(
        "build.gradle"
        "settings.gradle"
        "app/build.gradle"
        "gradle.properties"
    )
    
    for file in "${required_files[@]}"; do
        if [[ -f "$ROOT_DIR/$file" ]]; then
            log_success "Found: $file"
        else
            log_error "Missing: $file"
            return 1
        fi
    done
}

# Function to check critical source files
check_source_files() {
    log_info "Checking critical source files..."
    
    local kotlin_sources
    kotlin_sources=$(find app/src/main/java -name "*.kt" 2>/dev/null | wc -l)
    
    if [[ "$kotlin_sources" -gt 0 ]]; then
        log_success "Found $kotlin_sources Kotlin source files"
    else
        log_error "No Kotlin source files found"
        return 1
    fi
    
    # Check main activity
    if [[ -f "app/src/main/java/tech/ula/MainActivity.kt" ]]; then
        log_success "MainActivity.kt exists"
    else
        log_error "MainActivity.kt not found"
        return 1
    fi
}

# Function to check native build configuration
check_native_build() {
    log_info "Checking native build configuration..."
    
    if [[ -f "core/librafaelia/CMakeLists.txt" ]]; then
        log_success "CMakeLists.txt found"
    else
        log_warning "CMakeLists.txt not found (optional for some builds)"
    fi
    
    # Check native source files
    local c_sources
    c_sources=$(find . -maxdepth 1 -name "raf_*.c" 2>/dev/null | wc -l)
    
    if [[ "$c_sources" -gt 0 ]]; then
        log_success "Found $c_sources native C source files"
    else
        log_warning "No native C source files found"
    fi
}

# Function to validate dependencies
check_dependencies() {
    log_info "Validating Gradle dependencies..."
    
    if ./gradlew dependencies &> /tmp/gradle_deps.log; then
        log_success "Gradle dependencies resolved successfully"
    else
        log_error "Failed to resolve Gradle dependencies"
        log_info "Check /tmp/gradle_deps.log for details"
        return 1
    fi
}

# Function to run compilation test
test_compilation() {
    log_info "Running compilation test..."
    
    local start_time
    start_time=$(date +%s)
    
    if ./gradlew assembleDebug --no-daemon --stacktrace &> /tmp/gradle_build.log; then
        local end_time
        end_time=$(date +%s)
        local duration=$((end_time - start_time))
        log_success "Debug APK compiled successfully in ${duration}s"
        
        # Verify APK exists
        if [[ -f "app/build/outputs/apk/debug/app-debug.apk" ]]; then
            local apk_size
            apk_size=$(du -h "app/build/outputs/apk/debug/app-debug.apk" | cut -f1)
            log_success "APK generated: $apk_size"
        else
            log_error "APK file not found after successful build"
            return 1
        fi
    else
        log_error "Compilation failed"
        log_info "Check /tmp/gradle_build.log for details"
        return 1
    fi
}

# Function to run native compilation check
test_native_compilation() {
    log_info "Running native compilation check..."
    
    if [[ ! -f "$ROOT_DIR/scripts/compile_check.sh" ]]; then
        log_warning "Native compilation check script not found (optional)"
        return 0
    fi
    
    if [[ ! -x "$ROOT_DIR/scripts/compile_check.sh" ]]; then
        log_warning "Native compilation check script not executable"
        return 0
    fi
    
    if "$ROOT_DIR/scripts/compile_check.sh" &> /tmp/native_build.log; then
        log_success "Native binaries compiled successfully"
    else
        log_warning "Native compilation check failed (may not be critical)"
        log_info "Check /tmp/native_build.log for details"
    fi
}

# Function to check for common issues
check_common_issues() {
    log_info "Checking for common build issues..."
    
    # Check for android:extractNativeLibs warning
    if grep -q 'android:extractNativeLibs' app/src/main/AndroidManifest.xml 2>/dev/null; then
        log_warning "android:extractNativeLibs found in AndroidManifest.xml (consider moving to build.gradle)"
    fi
    
    # Check available disk space
    local available_space
    available_space=$(df -h . | awk 'NR==2 {print $4}')
    log_info "Available disk space: $available_space"
    
    # Check memory
    if command -v free &> /dev/null; then
        local available_mem
        available_mem=$(free -h | awk 'NR==2 {print $7}')
        log_info "Available memory: $available_mem"
    fi
}

# Main execution
main() {
    print_separator
    log_info "UserLAnd Compilation Guarantee Script"
    log_info "Ensuring all compilation requirements are met..."
    print_separator
    
    # Run all checks
    check_java_version || true
    check_gradle_wrapper || true
    check_gradle_version || true
    check_android_sdk || true
    check_ndk || true
    check_build_files || true
    check_source_files || true
    check_native_build || true
    check_common_issues || true
    
    print_separator
    log_info "Performing dependency resolution..."
    print_separator
    check_dependencies || true
    
    # Only run compilation test if requested
    if [[ "${RUN_COMPILATION_TEST:-false}" == "true" ]]; then
        print_separator
        log_info "Running full compilation test..."
        print_separator
        test_compilation || true
        test_native_compilation || true
    else
        log_info "Skipping compilation test (set RUN_COMPILATION_TEST=true to enable)"
    fi
    
    # Print summary
    print_separator
    log_info "Compilation Guarantee Summary:"
    echo -e "  ${GREEN}Passed:${NC}  $CHECKS_PASSED"
    echo -e "  ${YELLOW}Warnings:${NC} $CHECKS_WARNED"
    echo -e "  ${RED}Failed:${NC}  $CHECKS_FAILED"
    print_separator
    
    if [[ $CHECKS_FAILED -eq 0 ]]; then
        log_success "All critical checks passed! Compilation should succeed."
        return $EXIT_SUCCESS
    else
        log_error "Some checks failed. Please fix the issues above."
        return $EXIT_FAILURE
    fi
}

# Run main function
main "$@"
