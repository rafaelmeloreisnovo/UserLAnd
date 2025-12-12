#!/usr/bin/env bash
# Minimal shim delegating to low-level runner
set -euo pipefail

BIN="./rafael_userland_debug_runner"
SRC="rafael_userland_debug_runner.c"

if [[ ! -x "$BIN" ]]; then
  if [[ -f "$SRC" ]]; then
    gcc -std=c11 -Wall -Wextra -Werror -O2 -o "$BIN" "$SRC" -lm
  else
    echo "runner source missing" >&2
    exit 1
  fi
fi

exec "$BIN"
