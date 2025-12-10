#!/bin/bash

# Minimal shim kept for compatibility; prefers the low-level C orchestrator.
if [ -x "./swarm_runner" ]; then
  exec ./swarm_runner "$@"
fi

echo "swarm_runner binary not found. Compile with: gcc -std=c11 -Wall -Wextra -Werror -o swarm_runner swarm_runner.c" >&2
exit 1
