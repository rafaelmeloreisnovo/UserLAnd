#!/bin/bash

# Minimal shim kept for compatibility; prefers the low-level C orchestrator.
if [ -x "./swarm_runner" ]; then
  exec ./swarm_runner "$@"
fi

echo "swarm_runner binary not found. Compile with: gcc -std=c11 -Wall -Wextra -Werror -o swarm_runner swarm_runner.c" >&2
exit 1
# Low-level launcher shim that prefers the native C orchestrator.
# Avoids shell loops for runtime control; keeps behavior aligned with the
# original swarm cadence while relying on compiled scheduling.

set -euo pipefail

if [ ! -x ./run_swarm ]; then
    echo "[build] Compiling run_swarm (C orchestrator)..."
    gcc -std=c11 -Wall -Wextra -O2 run_swarm.c -o run_swarm
fi

exec ./run_swarm "$@"
