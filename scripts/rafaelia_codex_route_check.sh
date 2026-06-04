#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0
# RAFAELIA × UserLAnd Codex route checker
# Read-only preflight: classify facts, mark gaps, and prevent false-stable claims.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

OUT_DIR="${OUT_DIR:-build/rafaelia_codex_route}"
mkdir -p "$OUT_DIR"

failures=0
warnings=0

ok() { printf '[OK] %s\n' "$*"; }
warn() { warnings=$((warnings + 1)); printf '[WARN] %s\n' "$*" >&2; }
fail() { failures=$((failures + 1)); printf '[FAIL] %s\n' "$*" >&2; }

require_file() {
  local path="$1"
  if [[ -f "$path" ]]; then
    ok "file exists: $path"
  else
    fail "missing file: $path"
  fi
}

require_executable_or_warn() {
  local path="$1"
  if [[ -x "$path" ]]; then
    ok "executable: $path"
  elif [[ -f "$path" ]]; then
    warn "file exists but is not executable: $path"
  else
    fail "missing script: $path"
  fi
}

extract_ndk_version() {
  sed -n 's/^[[:space:]]*ndkVersion[[:space:]]*=[[:space:]]*\([^[:space:]]*\).*/\1/p' gradle.properties | head -n1
}

write_snapshot() {
  git rev-parse HEAD > "$OUT_DIR/HEAD.txt" 2>/dev/null || echo TOKEN_VAZIO > "$OUT_DIR/HEAD.txt"
  git status --short > "$OUT_DIR/git_status.txt" 2>/dev/null || true
  git ls-files > "$OUT_DIR/git_files.txt" 2>/dev/null || true
  find . -path ./.git -prune -o -type f -print | sort > "$OUT_DIR/find_files.txt"
  sha256sum README.md gradle.properties scripts/build_apk.sh scripts/build_static.sh scripts/ensure_compilation.sh \
    docs/TECHNICAL_DOCUMENTATION.md docs/RAFAELIA_STABLE_UNION_CODEX_ROUTE.md \
    > "$OUT_DIR/core_hashes.sha256" 2>/dev/null || true
}

check_required_files() {
  require_file README.md
  require_file gradle.properties
  require_file build.gradle
  require_file settings.gradle
  require_file app/build.gradle
  require_file docs/TECHNICAL_DOCUMENTATION.md
  require_file docs/COMPILATION_GUARANTEE.md
  require_file docs/RAFAELIA_STABLE_UNION_CODEX_ROUTE.md
  require_file core/librafaelia/raf_kerneld.py
  require_file raf_cpu_core.c
  require_file raf_mem_core.c
  require_file raf_disk_core.c
  require_file raf_arch_probe.c
}

check_scripts() {
  require_executable_or_warn scripts/build_apk.sh
  require_executable_or_warn scripts/build_unsigned_apk.sh
  require_executable_or_warn scripts/build_static.sh
  require_executable_or_warn scripts/ensure_compilation.sh
  require_executable_or_warn scripts/compile_check.sh
  require_executable_or_warn scripts/raf_kerneld.sh
  require_executable_or_warn scripts/raf_status.sh
}

check_version_coherence() {
  local ndk_version
  ndk_version="$(extract_ndk_version || true)"
  if [[ -z "$ndk_version" ]]; then
    fail "gradle.properties does not define ndkVersion"
    return
  fi
  ok "canonical ndkVersion=$ndk_version"

  if grep -R "21\.4\.7075529" README.md docs .github/workflows 2>/dev/null | grep -v codex_route_check >/dev/null; then
    warn "legacy NDK 21.4.7075529 reference still appears in docs/workflows; align to gradle.properties or explain historical context"
    grep -R "21\.4\.7075529" README.md docs .github/workflows 2>/dev/null > "$OUT_DIR/legacy_ndk_refs.txt" || true
  else
    ok "no legacy NDK 21.4.7075529 references found in docs/workflows"
  fi

  if ! grep -q "$ndk_version" docs/TECHNICAL_DOCUMENTATION.md; then
    fail "docs/TECHNICAL_DOCUMENTATION.md does not mention canonical ndkVersion=$ndk_version"
  else
    ok "technical docs mention canonical NDK"
  fi
}

check_token_empty_policy() {
  if grep -q "TOKEN_VAZIO" docs/RAFAELIA_STABLE_UNION_CODEX_ROUTE.md; then
    ok "TOKEN_VAZIO policy is documented"
  else
    fail "TOKEN_VAZIO policy missing from stable union route"
  fi
}

check_sensitive_candidates() {
  local report="$OUT_DIR/sensitive_candidates.txt"
  find . -path ./.git -prune -o -type f \
    \( -name '*.jks' -o -name '*.keystore' -o -name '*.pem' -o -name '*.key' -o -name '*.p12' -o -name '.env' -o -name 'local.properties' \) \
    -print | sort > "$report"
  if [[ -s "$report" ]]; then
    warn "sensitive/signing/local files found; review before commit: $report"
  else
    ok "no obvious keystore/key/env/local.properties candidates found"
  fi
}

check_docs_navigation() {
  if grep -q "RAFAELIA_STABLE_UNION_CODEX_ROUTE" README.md; then
    ok "README links the stable union route"
  else
    warn "README does not yet link docs/RAFAELIA_STABLE_UNION_CODEX_ROUTE.md"
  fi
}

write_report() {
  cat > "$OUT_DIR/REPORT.md" <<EOF_REPORT
# RAFAELIA Codex Route Check

## Status

- failures: $failures
- warnings: $warnings

## Invariant

Fato > hipotese > metafora. TOKEN_VAZIO > preenchimento falso.

## Generated files

- HEAD.txt
- git_status.txt
- git_files.txt
- find_files.txt
- core_hashes.sha256
- sensitive_candidates.txt
- legacy_ndk_refs.txt, when applicable

## Next

1. Fix any FAIL item.
2. Review WARN items.
3. Run static helper build.
4. Run Android compilation guarantee.
5. Attach logs before claiming stable integration.
EOF_REPORT
}

main() {
  echo 'RAFAELIA × UserLAnd Codex route check'
  write_snapshot
  check_required_files
  check_scripts
  check_version_coherence
  check_token_empty_policy
  check_sensitive_candidates
  check_docs_navigation
  write_report

  echo "Report: $OUT_DIR/REPORT.md"
  if [[ "$failures" -gt 0 ]]; then
    echo "Route check failed with $failures failure(s) and $warnings warning(s)." >&2
    exit 1
  fi
  echo "Route check passed with $warnings warning(s)."
}

main "$@"
