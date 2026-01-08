#!/usr/bin/env bash
set -euo pipefail
CORE="$HOME/RAFAELIA_CORE"
BACK="$CORE/BACKUPS"

# Safety checks for paths
if [[ -z "$CORE" || ! "$CORE" =~ RAFAELIA_CORE || ${#CORE} -lt 10 ]]; then
  echo "[ERROR] Invalid CORE path: $CORE" >&2
  exit 1
fi
if [[ -z "$BACK" || ! "$BACK" =~ BACKUPS || ${#BACK} -lt 10 ]]; then
  echo "[ERROR] Invalid BACK path: $BACK" >&2
  exit 1
fi

mkdir -p "$BACK"
while true; do
  TS=$(date +%s)
  FUSION_SRC="$CORE/FUSION_OMEGA.json"
  FUSION_DST="$BACK/FUSION_${TS}.json"
  DECISION_SRC="$CORE/DECISION_OMEGA.json"
  DECISION_DST="$BACK/DECISION_${TS}.json"
  
  # Safety checks before cp operations
  if [[ -n "$FUSION_SRC" && -f "$FUSION_SRC" && -n "$FUSION_DST" && "$FUSION_DST" =~ BACKUPS ]]; then
    cp "$FUSION_SRC" "$FUSION_DST" 2>/dev/null || true
  fi
  if [[ -n "$DECISION_SRC" && -f "$DECISION_SRC" && -n "$DECISION_DST" && "$DECISION_DST" =~ BACKUPS ]]; then
    cp "$DECISION_SRC" "$DECISION_DST" 2>/dev/null || true
  fi
  sleep 300
done
