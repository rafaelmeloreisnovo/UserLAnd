#!/usr/bin/env bash
set -euo pipefail
CORE="$HOME/RAFAELIA_CORE"
DEC="$CORE/DECISION_OMEGA.json"

# Safety check for paths
if [[ -z "$CORE" || ! "$CORE" =~ RAFAELIA_CORE || ${#CORE} -lt 10 ]]; then
  echo "[ERROR] Invalid CORE path: $CORE" >&2
  exit 1
fi
if [[ -z "$DEC" || ! "$DEC" =~ DECISION_OMEGA.json || ${#DEC} -lt 15 ]]; then
  echo "[ERROR] Invalid DEC path: $DEC" >&2
  exit 1
fi

while true; do
  ACTION=$(jq -r '.next_action // empty' "$DEC")
  if [ -n "$ACTION" ]; then
    echo "[⚡️] Executando: $ACTION"
    eval "$ACTION"
    TMP_FILE="$CORE/tmp.$$"
    # Safety check before mv
    if [[ -n "$TMP_FILE" && "$TMP_FILE" =~ $CORE && ${#TMP_FILE} -gt 10 ]]; then
      jq 'del(.next_action)' "$DEC" > "$TMP_FILE" && mv "$TMP_FILE" "$DEC"
    else
      echo "[ERROR] Safety check failed for tmp file: $TMP_FILE" >&2
      exit 1
    fi
  fi
  sleep 10
done
