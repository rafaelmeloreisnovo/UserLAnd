#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

ok(){ echo "[OK] $*"; }
warn(){ echo "[WARN] $*"; }
err(){ echo "[ERR] $*"; }

fail=0

echo "== UserLAnd start doctor (bootstrap/kernel/proot/abi/java) =="

if command -v java >/dev/null 2>&1; then
  java_major="$(java -version 2>&1 | sed -n 's/.*version "\([0-9][0-9]*\).*/\1/p' | head -n1)"
  if [[ -n "$java_major" && "$java_major" -ge 17 ]]; then
    ok "Java $java_major detected (17+)."
  else
    err "Java is older than 17 (detected: ${java_major:-unknown})."
    fail=1
  fi
else
  err "Java not found in PATH."
  fail=1
fi

if [[ -x ./gradlew ]]; then
  ok "gradlew executable is present."
else
  err "gradlew missing or not executable."
  fail=1
fi

if [[ -f app/build.gradle ]]; then
  compile_sdk="$(sed -n 's/^[[:space:]]*compileSdk[[:space:]]*=?[[:space:]]*//p; s/^[[:space:]]*compileSdk[[:space:]]\+//p' app/build.gradle | head -n1 | tr -d ' ')"
  if [[ -n "$compile_sdk" ]]; then
    ok "compileSdk=$compile_sdk"
  else
    warn "compileSdk not parsed from app/build.gradle"
  fi
fi

if [[ -f app/src/main/java/tech/ula/utils/UlaFiles.kt ]]; then
  if rg -q 'val proot = File\(supportDir, "proot"\)' app/src/main/java/tech/ula/utils/UlaFiles.kt; then
    ok "PRoot support path is configured in UlaFiles.kt"
  else
    warn "PRoot support path not found in UlaFiles.kt"
  fi
else
  warn "UlaFiles.kt not found"
fi

if [[ -f app/src/main/java/tech/ula/utils/BusyboxExecutor.kt ]]; then
  if rg -q 'prootIsPresent\(\)' app/src/main/java/tech/ula/utils/BusyboxExecutor.kt; then
    ok "BusyboxExecutor checks proot presence before command execution"
  else
    warn "BusyboxExecutor proot presence guard not found"
  fi
fi

if [[ -f app/src/main/java/tech/ula/ServerService.kt ]]; then
  if rg -q '"start" ->' app/src/main/java/tech/ula/ServerService.kt; then
    ok "ServerService start command handler found"
  else
    err "ServerService start command handler not found"
    fail=1
  fi
fi


if [[ -f termux-app/terminal-term/src/main/java/com/termux/app/TermuxService.java ]]; then
  if rg -q 'buildStartupFailureCommand' termux-app/terminal-term/src/main/java/com/termux/app/TermuxService.java; then
    ok "Terminal service has shell startup fallback diagnostics"
  else
    warn "Terminal service startup fallback diagnostics not found"
  fi
  if rg -q 'firstExecutableSupportFile\("dbclient"\)' termux-app/terminal-term/src/main/java/com/termux/app/TermuxService.java; then
    ok "Terminal service validates dbclient before opening SSH shell"
  else
    warn "Terminal service dbclient validation not found"
  fi
fi

if [[ "$fail" -ne 0 ]]; then
  echo "== RESULT: FAIL =="
  exit 1
fi

echo "== RESULT: PASS =="
