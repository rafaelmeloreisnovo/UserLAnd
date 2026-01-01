#!/usr/bin/env bash
CORE="$HOME/RAFAELIA_CORE"
DEC="$CORE/DECISION_OMEGA.json"
while true; do
  echo "[🧿] SHA256: $(sha256sum "$DEC" 2>/dev/null)"
  sleep 30
done
