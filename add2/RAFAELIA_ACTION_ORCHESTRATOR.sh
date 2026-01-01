#!/usr/bin/env bash
CORE="$HOME/RAFAELIA_CORE"
DEC="$CORE/DECISION_OMEGA.json"
while true; do
  ACTION=$(jq -r '.next_action // empty' "$DEC")
  if [ -n "$ACTION" ]; then
    echo "[⚡️] Executando: $ACTION"
    eval "$ACTION"
    jq 'del(.next_action)' "$DEC" > tmp && mv tmp "$DEC"
  fi
  sleep 10
done
