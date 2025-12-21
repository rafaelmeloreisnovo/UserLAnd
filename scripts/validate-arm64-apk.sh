#!/usr/bin/env bash
#
# validate-arm64-apk.sh
# Validates that an APK contains ARM64 libraries and is properly structured
#
# Usage: validate-arm64-apk.sh <path-to-apk>
#
# Returns: 0 if valid, 1 if invalid

set -euo pipefail

# Colors and emojis for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

APK_PATH="${1:-}"

usage() {
  echo "Usage: $0 <path-to-apk>"
  echo ""
  echo "Validates ARM64 APK structure and contents"
  exit 1
}

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

if [[ -z "$APK_PATH" ]]; then
  log_error "No APK path provided"
  usage
fi

if [[ ! -f "$APK_PATH" ]]; then
  log_error "APK file not found: $APK_PATH"
  exit 1
fi

log_info "Validating APK: $(basename "$APK_PATH")"
echo ""

# Check 1: Validate ZIP structure
log_info "Checking ZIP structure..."
if unzip -t "$APK_PATH" > /dev/null 2>&1; then
  log_success "ZIP structure is valid"
else
  log_error "APK has corrupted ZIP structure"
  exit 1
fi
echo ""

# Check 2: Check file size (must be > 10MB)
log_info "Checking APK size..."
APK_SIZE=$(stat -c%s "$APK_PATH" 2>/dev/null || stat -f%z "$APK_PATH" 2>/dev/null)
APK_SIZE_MB=$((APK_SIZE / 1024 / 1024))

if [[ $APK_SIZE -gt 10485760 ]]; then
  log_success "APK size is valid: ${APK_SIZE_MB} MB"
else
  log_error "APK is too small: ${APK_SIZE_MB} MB (must be > 10 MB)"
  exit 1
fi
echo ""

# Check 3: Verify ARM64 libraries present
log_info "Checking for ARM64 libraries..."
ARM64_LIBS=$(unzip -l "$APK_PATH" | grep "lib/arm64-v8a/" | grep "\.so$" || true)

if [[ -z "$ARM64_LIBS" ]]; then
  log_error "No ARM64 libraries found in APK"
  log_info "APK contents:"
  unzip -l "$APK_PATH" | grep "lib/" || log_warning "No lib directory found"
  exit 1
fi

log_success "ARM64 libraries found:"
echo "$ARM64_LIBS" | while read -r line; do
  lib_name=$(echo "$line" | awk '{print $NF}')
  # Compute the actual size in bytes for this library to ensure numfmt
  # receives a clean numeric value, independent of unzip -l output format.
  lib_size=$(unzip -p "$APK_PATH" "$lib_name" 2>/dev/null | wc -c | tr -d '[:space:]')
  if [[ -z "$lib_size" ]]; then
    lib_size="0"
  fi
  if command -v numfmt > /dev/null 2>&1; then
    formatted_size=$(numfmt --to=iec-i --suffix=B "$lib_size" 2>/dev/null || echo "${lib_size} bytes")
  else
    formatted_size="${lib_size} bytes"
  fi
  echo "  📦 $lib_name ($formatted_size)"
done
echo ""

# Check 4: Verify it's unsigned (no META-INF signing files)
log_info "Checking signing status..."
SIGNING_FILES=$(unzip -l "$APK_PATH" | grep -E "META-INF/.*(RSA|DSA|EC)$" || true)

if [[ -z "$SIGNING_FILES" ]]; then
  log_success "APK is unsigned (as expected)"
else
  log_warning "APK appears to be signed"
  echo "$SIGNING_FILES"
fi
echo ""

# Check 5: Calculate SHA256
log_info "Calculating SHA256..."
if command -v sha256sum > /dev/null 2>&1; then
  SHA256=$(sha256sum "$APK_PATH" | awk '{print $1}')
elif command -v shasum > /dev/null 2>&1; then
  SHA256=$(shasum -a 256 "$APK_PATH" | awk '{print $1}')
else
  log_warning "sha256sum/shasum not available, skipping hash"
  SHA256="N/A"
fi

if [[ "$SHA256" != "N/A" ]]; then
  log_success "SHA256: $SHA256"
  echo ""
fi

# Final summary
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
log_success "APK Validation PASSED"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "📊 Summary:"
echo "  • File: $(basename "$APK_PATH")"
echo "  • Size: ${APK_SIZE_MB} MB"
echo "  • Architecture: ARM64 (arm64-v8a)"
echo "  • Status: Unsigned"
if [[ "$SHA256" != "N/A" ]]; then
  echo "  • SHA256: $SHA256"
fi
echo ""

exit 0
