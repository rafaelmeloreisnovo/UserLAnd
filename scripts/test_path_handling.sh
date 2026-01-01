#!/usr/bin/env bash
# Test script for path handling and validation
# Tests all scenarios described in docs/PATH_HANDLING.md
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VALIDATOR="$SCRIPT_DIR/path_validator.sh"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "================================================================="
echo "Path Handling and Validation Test Suite"
echo "================================================================="
echo ""

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

test_pass() {
    echo "✅ PASS: $1"
    ((PASSED_TESTS++)) || true
    ((TOTAL_TESTS++)) || true
}

test_fail() {
    echo "❌ FAIL: $1"
    ((FAILED_TESTS++)) || true
    ((TOTAL_TESTS++)) || true
}

echo "=== Unit Tests: Path Validation ==="
echo ""

# Test 1: Normal path
echo "Test 1: Normal path should be OK"
if $VALIDATOR check "/home/user/project/file.c" >/dev/null 2>&1; then
    test_pass "Normal path accepted"
else
    test_fail "Normal path rejected"
fi
echo ""

# Test 2: Doubled path
echo "Test 2: Doubled path should be detected"
if ! $VALIDATOR check "/home/user/user/file.c" >/dev/null 2>&1; then
    test_pass "Doubled path detected"
else
    test_fail "Doubled path not detected"
fi
echo ""

# Test 3: Normalize doubled path
echo "Test 3: Normalize doubled path"
result=$($VALIDATOR normalize "/path/to/dir/dir/file")
expected="/path/to/dir/file"
if [[ "$result" == "$expected" ]]; then
    test_pass "Path normalized correctly: $result"
else
    test_fail "Path normalization incorrect: expected '$expected', got '$result'"
fi
echo ""

# Test 4: GitHub Actions workspace (should be OK)
echo "Test 4: GitHub Actions workspace path should be preserved"
if $VALIDATOR check "/home/runner/work/UserLAnd/UserLAnd" >/dev/null 2>&1; then
    test_pass "GitHub Actions workspace accepted"
else
    test_fail "GitHub Actions workspace rejected"
fi
echo ""

# Test 5: GitHub Actions normalization (should preserve)
echo "Test 5: GitHub Actions path normalization should preserve structure"
result=$($VALIDATOR normalize "/home/runner/work/UserLAnd/UserLAnd/src/main.c")
expected="/home/runner/work/UserLAnd/UserLAnd/src/main.c"
if [[ "$result" == "$expected" ]]; then
    test_pass "GitHub Actions path preserved: $result"
else
    test_fail "GitHub Actions path incorrectly normalized: expected '$expected', got '$result'"
fi
echo ""

# Test 6: Safe path joining
echo "Test 6: Safe path joining"
result=$($VALIDATOR join "/base/path" "relative/component")
expected="/base/path/relative/component"
if [[ "$result" == "$expected" ]]; then
    test_pass "Path joined correctly: $result"
else
    test_fail "Path joining incorrect: expected '$expected', got '$result'"
fi
echo ""

# Test 7: Join GitHub Actions path
echo "Test 7: Join GitHub Actions workspace path"
result=$($VALIDATOR join "/home/runner/work/UserLAnd/UserLAnd" "src/main.c")
expected="/home/runner/work/UserLAnd/UserLAnd/src/main.c"
if [[ "$result" == "$expected" ]]; then
    test_pass "GitHub Actions path joined correctly: $result"
else
    test_fail "GitHub Actions path joining incorrect: expected '$expected', got '$result'"
fi
echo ""

# Test 8: Common directory names
echo "Test 8: Common directory names (lib/lib) should be OK"
if $VALIDATOR check "/usr/lib/lib/module.so" >/dev/null 2>&1; then
    test_pass "Common directory names accepted"
else
    test_fail "Common directory names rejected"
fi
echo ""

# Test 8b: Common directory names should NOT be normalized
echo "Test 8b: Common directory names should be preserved during normalization"
result=$($VALIDATOR normalize "/usr/lib/lib/module.so")
expected="/usr/lib/lib/module.so"
if [[ "$result" == "$expected" ]]; then
    test_pass "Common directory names preserved: $result"
else
    test_fail "Common directory names incorrectly normalized: expected '$expected', got '$result'"
fi
echo ""

# Test 9: Tripled path normalization
echo "Test 9: Normalize tripled path"
result=$($VALIDATOR normalize "/path/to/UserLAnd/UserLAnd/UserLAnd/file")
expected="/path/to/UserLAnd/file"
if [[ "$result" == "$expected" ]]; then
    test_pass "Tripled path normalized correctly: $result"
else
    test_fail "Tripled path normalization incorrect: expected '$expected', got '$result'"
fi
echo ""

echo "=== Integration Tests: Build System ==="
echo ""

# Test 10: Build with default paths
echo "Test 10: Build with default paths"
cd "$ROOT_DIR"
rm -rf build/
if ./scripts/build_static.sh >/dev/null 2>&1; then
    if [[ -f "$ROOT_DIR/build/bin/raf_cpu_core" ]]; then
        test_pass "Build with default paths succeeded"
    else
        test_fail "Build completed but binary not found"
    fi
else
    test_fail "Build with default paths failed"
fi
echo ""

# Test 11: Build with explicit SRC_DIR
echo "Test 11: Build with explicit SRC_DIR"
cd "$ROOT_DIR"
rm -rf build/
if SRC_DIR="$ROOT_DIR" ./scripts/build_static.sh >/dev/null 2>&1; then
    if [[ -f "$ROOT_DIR/build/bin/raf_mem_core" ]]; then
        test_pass "Build with explicit SRC_DIR succeeded"
    else
        test_fail "Build completed but binary not found"
    fi
else
    test_fail "Build with explicit SRC_DIR failed"
fi
echo ""

# Test 12: Rafaelia module integration
echo "Test 12: Rafaelia module builds correctly"
if [[ -f "$ROOT_DIR/rafaelia/rafaelia_arch_probe.c" ]]; then
    cd "$ROOT_DIR/rafaelia"
    make clean >/dev/null 2>&1 || true
    if make >/dev/null 2>&1; then
        if [[ -f "bin/rafaelia_arch_probe" ]]; then
            test_pass "Rafaelia module built successfully"
        else
            test_fail "Rafaelia make completed but binary not found"
        fi
    else
        test_fail "Rafaelia module build failed"
    fi
    make clean >/dev/null 2>&1 || true
else
    echo "⚠️  SKIP: Rafaelia source not found"
fi
echo ""

# Clean up
cd "$ROOT_DIR"
rm -rf build/ rafaelia/bin rafaelia/obj *.o 2>/dev/null || true

echo "================================================================="
echo "Test Summary"
echo "================================================================="
echo "Total tests:  $TOTAL_TESTS"
echo "Passed:       $PASSED_TESTS"
echo "Failed:       $FAILED_TESTS"
echo ""

if [[ $FAILED_TESTS -eq 0 ]]; then
    echo "✅ All tests passed!"
    exit 0
else
    echo "❌ Some tests failed!"
    exit 1
fi
