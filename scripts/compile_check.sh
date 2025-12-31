#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

OUT_DIR="${OUT_DIR:-"$ROOT_DIR/build/bin"}"

STRICT_CFLAGS="-O2 -pipe -fvisibility=hidden -Wall -Wextra -Werror"
CFLAGS="${CFLAGS:-$STRICT_CFLAGS}"

export CFLAGS
export OUT_DIR

"$ROOT_DIR/scripts/build_static.sh"

required_bins=(
  "raf_cpu_core"
  "raf_mem_core"
  "raf_disk_core"
  "raf_arch_probe"
)

for bin in "${required_bins[@]}"; do
  bin_path="$OUT_DIR/$bin"
  if [[ ! -x "$bin_path" ]]; then
    echo "[ERROR] Expected compiled binary not found or not executable: $bin_path" >&2
    exit 1
  fi
done

echo "[OK] Compilation checks passed."
