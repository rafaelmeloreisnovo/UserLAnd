#!/usr/bin/env bash
# Σ_guardian.sh — guardião ético/operacional para Termux-RAFAELIA
# Uso: Σ_guardian.sh -- run <cmd...> | check <script.sh> | audit <dir>
set -euo pipefail
trap 'echo "[ERR] $BASH_COMMAND -> exit=$?" >&2' ERR

deny() { echo "[DENY] $*"; exit 3; }

is_external() {
  case "$PWD" in /sdcard*|/storage/emulated/*) return 0;; esac
  return 1
}

check_env() {
  is_external && deny "Execução em storage externo — use \$HOME."
  command -v termux-info >/dev/null 2>&1 || echo "[WARN] termux-info ausente."
}

check_line() {
  local line="$1" lc; lc="$(printf '%s' "$line" | tr '[:upper:]' '[:lower:]')"
  [[ "$lc" =~ (^|[[:space:]])sudo([[:space:]]|$) ]] && deny "sudo bloqueado em Termux."
  [[ "$lc" == *"pip install --upgrade pip"* ]] && deny "Upgrade do pip bloqueado; use pkg install python-pip."
  [[ "$lc" == *"cargo build"* && "$lc" != *" -j"* ]] && echo "[ADAPT] cargo: adicionando -j1."
  [[ "$lc" == *"maturin"* && "$lc" != *" -j"* ]] && echo "[ADAPT] maturin: adicionando -j1."
  return 0
}

case "${1:-}" in
  run)
    shift
    check_env
    s="$*"
    check_line "$s" || exit $?
    # adapta -j1
    s="${s/cargo build/cargo build -j1}"
    s="${s/cargo install/cargo install -j1}"
    s="$(printf '%s' "$s" | sed 's/\bmaturin\b/maturin -j1/g')"
    echo "[RUN] $s"
    bash -lc "$s"
    ;;
  check)
    shift
    check_env
    awk '1' "${1:?script}" | while IFS= read -r L; do check_line "$L"; done
    echo "[OK] Check passou."
    ;;
  audit)
    shift
    check_env
    find "${1:-.}" -type f -name "*.sh" -print0 | while IFS= read -r -d '' f; do
      echo "[AUDIT] $f"; awk '1' "$f" | while IFS= read -r L; do check_line "$L" || true; done
    done
    ;;
  *) echo "Uso: $0 run <cmd...> | check <script.sh> | audit <dir>"; exit 2;;
esac
