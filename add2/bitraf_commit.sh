#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

# BITRAF_COMMIT — proximos passos operacionais do bitraf
#  - Roda o binario rafaelia_bitraf_hash
#  - Salva saida em RAFAELIA_LOGS/contextos/bitraf_commits.log
#  - Marca timestamp e separador

LOGDIR="$HOME/RAFAELIA_LOGS/contextos"
LOGFILE="$LOGDIR/bitraf_commits.log"
BIN="$HOME/rafaelia_bitraf_hash"

# Safety check and create directory
if [[ -z "$LOGDIR" || ! "$LOGDIR" =~ RAFAELIA_LOGS || ${#LOGDIR} -lt 10 ]]; then
  echo "[BITRAF_COMMIT] [ERROR] Invalid LOGDIR path: $LOGDIR" >&2
  exit 1
fi
mkdir -p "$LOGDIR"

# Verifica binario
if [ ! -x "$BIN" ]; then
  echo "[BITRAF_COMMIT] Binario nao encontrado ou nao executavel: $BIN"
  echo "[BITRAF_COMMIT] Compile antes com: clang rafaelia_bitraf_hash.c -o rafaelia_bitraf_hash"
  exit 1
fi

# Executa e registra
{
  echo "================ BITRAF COMMIT ================"
  echo "timestamp: $(date)"
  "$BIN"
  echo
} >> "$LOGFILE"

echo "[BITRAF_COMMIT] Commit registrado em: $LOGFILE"
