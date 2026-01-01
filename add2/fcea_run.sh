#!/data/data/com.termux/files/usr/bin/bash
# FCEA_RUN :: Orquestrador simples de serviços FCEA
# Usa: $HOME/fcea_services.conf

set -euo pipefail

CONF="${FCEA_CONF:-$HOME/fcea_services.conf}"

ts(){ date -u +"%Y-%m-%dT%H:%M:%SZ"; }

usage() {
  cat << USAGE
[FCEA-RUN] Uso:
  fcea_run.sh list
  fcea_run.sh status              # status de todos
  fcea_run.sh status <nome>
  fcea_run.sh start <nome>
  fcea_run.sh stop <nome>
  fcea_run.sh start-all
  fcea_run.sh stop-all

Config: $CONF   (formato: nome|enabled|cmd)
USAGE
}

[ -f "$CONF" ] || { echo "[FCEA-RUN] Arquivo de serviços não encontrado: $CONF" >&2; exit 1; }

load_services() {
  # remove comentários e linhas vazias
  grep -v '^[[:space:]]*#' "$CONF" 2>/dev/null | sed '/^[[:space:]]*$/d'
}

list_services() {
  echo "[FCEA-RUN] Serviços definidos em $CONF:"
  load_services | while IFS='|' read -r name enabled cmd; do
    echo "  - $name (enabled=$enabled) :: $cmd"
  done
}

status_one() {
  local name="$1"
  local line
  line=$(load_services | awk -F'|' -v n="$name" '$1==n {print}')
  if [ -z "$line" ]; then
    echo "[FCEA-RUN] Serviço '$name' não encontrado."
    return 1
  fi
  IFS='|' read -r _ enabled cmd <<< "$line"
  if pgrep -f "$cmd" >/dev/null 2>&1; then
    echo "[OK] $name → ATIVO"
  else
    echo "[..] $name → parado"
  fi
}

status_all() {
  load_services | while IFS='|' read -r name enabled cmd; do
    if pgrep -f "$cmd" >/dev/null 2>&1; then
      echo "[OK] $name → ATIVO"
    else
      echo "[..] $name → parado"
    fi
  done
}

start_one() {
  local name="$1"
  local line
  line=$(load_services | awk -F'|' -v n="$name" '$1==n {print}')
  if [ -z "$line" ]; then
    echo "[FCEA-RUN] Serviço '$name' não encontrado."
    return 1
  fi
  IFS='|' read -r _ enabled cmd <<< "$line"
  if pgrep -f "$cmd" >/dev/null 2>&1; then
    echo "[FCEA-RUN] $name já está rodando."
    return 0
  fi
  echo "[FCEA-RUN] Iniciando $name ..."
  nohup bash -lc "$cmd" >> "$HOME/fcea_logs/${name}.log" 2>&1 &
  echo "[FCEA-RUN] $name lançado em background."
}

stop_one() {
  local name="$1"
  local line
  line=$(load_services | awk -F'|' -v n="$name" '$1==n {print}')
  if [ -z "$line" ]; then
    echo "[FCEA-RUN] Serviço '$name' não encontrado."
    return 1
  fi
  IFS='|' read -r _ enabled cmd <<< "$line"
  if pgrep -f "$cmd" >/dev/null 2>&1; then
    echo "[FCEA-RUN] Encerrando $name ..."
    pkill -f "$cmd" || true
  else
    echo "[FCEA-RUN] $name já estava parado."
  fi
}

start_all() {
  load_services | while IFS='|' read -r name enabled cmd; do
    if [ "$enabled" = "1" ]; then
      echo "[FCEA-RUN] [START-ALL] → $name"
      start_one "$name"
    else
      echo "[FCEA-RUN] [SKIP] $name (enabled=0)"
    fi
  done
}

stop_all() {
  load_services | while IFS='|' read -r name enabled cmd; do
    echo "[FCEA-RUN] [STOP-ALL] → $name"
    stop_one "$name"
  done
}

cmd="${1:-}"

case "$cmd" in
  list)
    list_services
    ;;
  status)
    if [ $# -eq 2 ]; then
      status_one "$2"
    else
      status_all
    fi
    ;;
  start)
    [ $# -ge 2 ] || { usage; exit 1; }
    start_one "$2"
    ;;
  stop)
    [ $# -ge 2 ] || { usage; exit 1; }
    stop_one "$2"
    ;;
  start-all)
    start_all
    ;;
  stop-all)
    stop_all
    ;;
  ""|-h|--help|help)
    usage
    ;;
  *)
    echo "[FCEA-RUN] Comando desconhecido: $cmd"
    usage
    exit 1
    ;;
esac
