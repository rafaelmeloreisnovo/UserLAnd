#!/usr/bin/env bash
CORE="$HOME/RAFAELIA_CORE"
while true; do
  inotifywait -e modify,create,delete "$CORE"/FUSION_OMEGA.json "$CORE"/DECISION_OMEGA.json "$CORE"/LOGS/*.log
  pgrep -f RAFAELIA_PULSAR_SUPREMO.sh > /dev/null || bash ~/RAFAELIA_PULSAR_SUPREMO.sh &
  sleep 5
done
