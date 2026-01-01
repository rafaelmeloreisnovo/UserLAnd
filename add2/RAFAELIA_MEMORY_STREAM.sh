#!/usr/bin/env bash
CORE="$HOME/RAFAELIA_CORE"
BACK="$CORE/BACKUPS"
mkdir -p "$BACK"
while true; do
  TS=$(date +%s)
  cp "$CORE/FUSION_OMEGA.json" "$BACK/FUSION_${TS}.json" 2>/dev/null
  cp "$CORE/DECISION_OMEGA.json" "$BACK/DECISION_${TS}.json" 2>/dev/null
  sleep 300
done
