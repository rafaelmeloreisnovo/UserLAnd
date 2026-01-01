#!/data/data/com.termux/files/usr/bin/bash
# ===============================================================
#  Σ_seal_verify.sh — Verificação integral RAFAELIA_CORE (versão melhorada)
#  Melhorias: parsing robusto do índice YAML, logs indexados (JSONL),
#  relatório resumido, opções CLI, verificação de dependências,
#  atualização opcional do índice, code paths seguros e mensagens coloridas.
# ===============================================================

set -euo pipefail
IFS=$'\n\t'

# ---------------------------
# Config / Defaults
# ---------------------------
DEFAULT_ROOT="${HOME}/RAFAELIA_CORE"
DEFAULT_INDEX="Σ_Seal_Index.yaml"
LOGDIR_DEFAULT="logs"
JOBS_DEFAULT=1          # Change if you want parallel hashing; keep 1 for safety on low-end devices
TIMESTAMP=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
PROGNAME="$(basename "$0")"
#LOCKFILE="/tmp/$(echo "$DEFAULT_ROOT" | tr '/ ' '__')_seal_verify.lock"

LOCKFILE="${TMPDIR:-/data/data/com.termux/files/usr/tmp}/$(echo "$DEFAULT_ROOT" | tr '/ ' '__')_seal_verify.lock"
mkdir -p "$(dirname "$LOCKFILE")"


# ---------------------------
# Utilities
# ---------------------------
color_reset='\033[0m'
color_green='\033[1;32m'
color_red='\033[1;31m'
color_yellow='\033[1;33m'
color_blue='\033[1;34m'

logfile=""
jsonlfile=""
counter=0

die() {
  echo -e "${color_red}[ERRO]${color_reset} $*" >&2
  exit 1
}

info() {
  echo -e "${color_blue}[INFO]${color_reset} $*"
}

warn() {
  echo -e "${color_yellow}[AVISO]${color_reset} $*"
}

ok() {
  echo -e "${color_green}[OK]${color_reset} $*"
}

# JSONL indexed log writer
log_jsonl() {
  local entry="$1"
  if [[ -n "$jsonlfile" ]]; then
    printf '%s\n' "$entry" >> "$jsonlfile"
  fi
}

# ---------------------------
# Dependency detection
# ---------------------------
detect_hash_command() {
  # prefer sha3sum, then b3sum (blake3), then openssl sha3-512, then sha512sum (fallback but warned)
  if command -v sha3sum >/dev/null 2>&1; then
    echo "sha3sum"
  elif command -v b3sum >/dev/null 2>&1; then
    echo "b3sum"
  elif command -v openssl >/dev/null 2>&1 && openssl list -digest-commands 2>/dev/null | grep -qi sha3-512; then
    echo "openssl-sha3-512"
  elif command -v sha512sum >/dev/null 2>&1; then
    echo "sha512sum-fallback"
  else
    echo ""
  fi
}

hash_of_file() {
  local file="$1"
  case "$HASH_CMD" in
    sha3sum)
      sha3sum -a 512 "$file" 2>/dev/null | awk '{print $1}'
      ;;
    b3sum)
      b3sum "$file" 2>/dev/null | awk '{print $1}'
      ;;
    openssl-sha3-512)
      openssl dgst -sha3-512 -binary "$file" 2>/dev/null | xxd -p -c 512
      ;;
    sha512sum-fallback)
      warn "Usando SHA-512 como fallback (não é SHA3). Considere instalar sha3sum ou openssl com SHA3."
      sha512sum "$file" | awk '{print $1}'
      ;;
    *)
      die "Nenhum comando de hashing suportado encontrado. Instale sha3sum, b3sum, ou openssl com SHA3."
      ;;
  esac
}

# ---------------------------
# YAML index parser
# ---------------------------
# Expect lines like:
# arquivo: path/to/file
# integridade: <hash>
# other fields ignored
parse_index() {
  local index_file="$1"
  # robust parsing: iterate lines, capture arquivo then integridade
  local file="" ref=""
  local lineno=0
  while IFS= read -r rawline || [[ -n "$rawline" ]]; do
    lineno=$((lineno+1))
    # trim leading/trailing spaces and remove surrounding quotes
    line="$(printf '%s' "$rawline" | sed -e 's/^[[:space:]]*//;s/[[:space:]]*$//')"
    # skip comments and empty
    [[ -z "$line" || "${line:0:1}" == "#" ]] && continue

    if [[ $line =~ ^arquivo:[[:space:]]*(.*)$ ]]; then
      # If previous file captured without ref, process with empty ref
      if [[ -n "$file" ]]; then
        process_entry "$file" "$ref"
        ref=""
      fi
      file="${BASH_REMATCH[1]}"
      # strip starting and ending quotes if exist
      file="${file%\"}"
      file="${file#\"}"
      file="${file%\'}"
      file="${file#\'}"
      # expand ~
      file="${file/#\~/$HOME}"
    elif [[ $line =~ ^integridade:[[:space:]]*(.*)$ ]]; then
      ref="${BASH_REMATCH[1]}"
      ref="${ref%\"}"
      ref="${ref#\"}"
      ref="${ref%\'}"
      ref="${ref#\'}"
    fi
  done < "$index_file"

  # process last captured
  if [[ -n "$file" ]]; then
    process_entry "$file" "$ref"
  fi
}

# ---------------------------
# Per-file processing
# ---------------------------
TOTAL=0
COUNT_OK=0
COUNT_MISMATCH=0
COUNT_MISSING=0
COUNT_ERROR=0
ENTRY_ID=0

process_entry() {
  local file="$1"
  local ref="$2"
  ENTRY_ID=$((ENTRY_ID+1))
  TOTAL=$((TOTAL+1))
  local ts=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
  local status="" hash_now="" notes=""

  if [[ -f "$file" ]]; then
    if ! hash_now="$(hash_of_file "$file")"; then
      status="error"
      notes="hashing-failed"
      COUNT_ERROR=$((COUNT_ERROR+1))
      warn "[$ENTRY_ID] $file — erro ao gerar hash"
    else
      # normalize cases and whitespace
      hash_now="$(printf '%s' "$hash_now" | tr '[:upper:]' '[:lower:]' | tr -d '[:space:]')"
      ref_norm="$(printf '%s' "$ref" | tr '[:upper:]' '[:lower:]' | tr -d '[:space:]')"
      if [[ -z "$ref_norm" ]]; then
        status="no-ref"
        notes="indice-sem-hash"
        warn "[$ENTRY_ID] $file — sem hash de referência no índice"
      elif [[ "$hash_now" == "$ref_norm" ]]; then
        status="intact"
        COUNT_OK=$((COUNT_OK+1))
        ok "[$ENTRY_ID] $file — íntegro"
      else
        status="mismatch"
        COUNT_MISMATCH=$((COUNT_MISMATCH+1))
        warn "[$ENTRY_ID] $file — hash divergente!"
      fi
    fi
  else
    status="missing"
    COUNT_MISSING=$((COUNT_MISSING+1))
    hash_now=""
    notes="arquivo-ausente"
    warn "[$ENTRY_ID] $file — ausente"
  fi

  # Write human-readable log line
  printf '%s | %3d | %s | %s\n' "$ts" "$ENTRY_ID" "$status" "$file" >> "$logfile"

  # Write indexed JSONL entry (id, file, status, hash_now, hash_ref, notes, timestamp)
  json_entry=$(cat <<-JSON
{"id":$ENTRY_ID,"file":"$file","status":"$status","hash_now":"$hash_now","hash_ref":"$ref","notes":"$notes","timestamp":"$ts"}
JSON
)
  log_jsonl "$json_entry"
}

# ---------------------------
# CLI / Main
# ---------------------------
usage() {
  cat <<-USG
Usage: $PROGNAME [options]

Options:
  -d DIR        Root dir (default: ${DEFAULT_ROOT})
  -i INDEX      Index YAML file (default: ${DEFAULT_INDEX})
  -l LOGDIR     Logs directory (default: ${LOGDIR_DEFAULT})
  -j JOBS       Parallel jobs for hashing (default: ${JOBS_DEFAULT})
  -u            Update the index file with current hashes (overwrites integridade values)
  -q            Quiet (minimal stdout)
  -v            Verbose
  -h            Show this help
USG
  exit 0
}

# default values
ROOT="$DEFAULT_ROOT"
INDEX_FILE="$DEFAULT_ROOT/$DEFAULT_INDEX"
LOGDIR="$DEFAULT_ROOT/$LOGDIR_DEFAULT"
UPDATE_INDEX=false
QUIET=false
VERBOSE=false

while getopts "d:i:l:j:uvqh" opt; do
  case "$opt" in
    d) ROOT="$OPTARG" ;;
    i) INDEX_FILE="$OPTARG" ;;
    l) LOGDIR="$OPTARG" ;;
    j) JOBS="$OPTARG" ;;
    u) UPDATE_INDEX=true ;;
    q) QUIET=true ;;
    v) VERBOSE=true ;;
    h) usage ;;
    *) usage ;;
  esac
done

# Create lock to prevent concurrent runs
if [[ -e "$LOCKFILE" ]]; then
  warn "Arquivo de lock encontrado ($LOCKFILE). Parece que outra instância está em execução."
  warn "Remova $LOCKFILE se tiver certeza de que não há outra execução."
  exit 1
fi
trap 'rm -f "$LOCKFILE" 2>/dev/null || true' EXIT
touch "$LOCKFILE"

# Normalize paths
mkdir -p "$LOGDIR"
mkdir -p "$ROOT"
if [[ ! -f "$INDEX_FILE" ]]; then
  die "Arquivo de índice não encontrado: $INDEX_FILE"
fi

# Setup logs
LOGDIR="${LOGDIR%/}"
logfile="${LOGDIR}/verify-$(date -u +"%Y%m%dT%H%M%SZ").log"
jsonlfile="${LOGDIR}/verify-$(date -u +"%Y%m%dT%H%M%SZ").jsonl"
echo "[$TIMESTAMP] Iniciando verificação fractal RAFAELIA_CORE" > "$logfile"
echo "{\"run_started\":\"$TIMESTAMP\",\"index\":\"$INDEX_FILE\",\"root\":\"$ROOT\"}" > "${jsonlfile}.meta"
# append header to jsonl file (meta + entries)
touch "$jsonlfile"

# Change to root (as original script did)
cd "$ROOT"

# Detect hash command
HASH_CMD="$(detect_hash_command)"
if [[ -z "$HASH_CMD" ]]; then
  die "Nenhum gerador de hash disponível (sha3sum / b3sum / openssl / sha512sum). Instale um."
fi
if [[ "$HASH_CMD" == "sha512sum-fallback" ]]; then
  warn "Usando SHA-512 como fallback. Recomendo instalar suporte a SHA3 (sha3sum ou openssl com SHA3)."
fi

if [[ "$VERBOSE" == "true" ]]; then
  info "Usando comando de hash: $HASH_CMD"
  info "Arquivo de índice: $INDEX_FILE"
  info "Logs: $logfile  (indexed: $jsonlfile)"
fi

# ---------------------------
# Run parse & processing
# ---------------------------
start_ts="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
info "[Σ_verify] Iniciando verificação fractal RAFAELIA_CORE..." || true

# parse and process entries
# If UPDATE_INDEX is requested, we will collect new hashes and rewrite the index at the end
declare -A NEW_HASHES
if [[ "$UPDATE_INDEX" == "true" ]]; then
  info "Modo: atualizar índice (integridade será substituída pelos hashes atuais)."
fi

# Wrapper for process_entry that also collects new hashes if requested
process_entry_wrapper() {
  local file="$1"
  local ref="$2"
  # Reuse the process_entry logic but capture computed hash to optionally update index
  ENTRY_ID=$((ENTRY_ID+1))
  TOTAL=$((TOTAL+1))
  local ts=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
  local status="" hash_now="" notes=""

  if [[ -f "$file" ]]; then
    if ! hash_now="$(hash_of_file "$file")"; then
      status="error"
      notes="hashing-failed"
      COUNT_ERROR=$((COUNT_ERROR+1))
      warn "[$ENTRY_ID] $file — erro ao gerar hash"
    else
      hash_now="$(printf '%s' "$hash_now" | tr '[:upper:]' '[:lower:]' | tr -d '[:space:]')"
      ref_norm="$(printf '%s' "$ref" | tr '[:upper:]' '[:lower:]' | tr -d '[:space:]')"
      if [[ -z "$ref_norm" ]]; then
        status="no-ref"
        notes="indice-sem-hash"
        warn "[$ENTRY_ID] $file — sem hash de referência no índice"
      elif [[ "$hash_now" == "$ref_norm" ]]; then
        status="intact"
        COUNT_OK=$((COUNT_OK+1))
        ok "[$ENTRY_ID] $file — íntegro"
      else
        status="mismatch"
        COUNT_MISMATCH=$((COUNT_MISMATCH+1))
        warn "[$ENTRY_ID] $file — hash divergente!"
      fi
    fi
  else
    status="missing"
    COUNT_MISSING=$((COUNT_MISSING+1))
    hash_now=""
    notes="arquivo-ausente"
    warn "[$ENTRY_ID] $file — ausente"
  fi

  printf '%s | %3d | %s | %s\n' "$ts" "$ENTRY_ID" "$status" "$file" >> "$logfile"

  json_entry=$(cat <<-JSON
{"id":$ENTRY_ID,"file":"$file","status":"$status","hash_now":"$hash_now","hash_ref":"$ref","notes":"$notes","timestamp":"$ts"}
JSON
)
  log_jsonl "$json_entry"

  if [[ "$UPDATE_INDEX" == "true" && -n "$hash_now" ]]; then
    NEW_HASHES["$file"]="$hash_now"
  fi
}

# We'll feed parse_index a function pointer by temporarily redefining process_entry
# Save original and replace
process_entry_orig="$(declare -f process_entry)"
unset -f process_entry
process_entry() { process_entry_wrapper "$@"; }

parse_index "$INDEX_FILE"

# restore original process_entry (not strictly needed past this point)
eval "$process_entry_orig"

# ---------------------------
# Optional: update index file with new hashes
# ---------------------------
if [[ "$UPDATE_INDEX" == "true" ]]; then
  info "Atualizando arquivo de índice com novos hashes..."
  # Create a backup
  cp -a "$INDEX_FILE" "${INDEX_FILE}.bak.$(date -u +"%Y%m%dT%H%M%SZ")"
  # Replace integridade: <...> for each arquivo: path by simple sed loop
  tmp_index="${INDEX_FILE}.tmp.$$"
  cp "$INDEX_FILE" "$tmp_index"
  for f in "${!NEW_HASHES[@]}"; do
    newh="${NEW_HASHES[$f]}"
    # escape slashes for sed
    esc_f="$(printf '%s' "$f" | sed -e 's/[\/&]/\\&/g')"
    esc_h="$(printf '%s' "$newh" | sed -e 's/[\/&]/\\&/g')"
    # If integridade line exists after that arquivo, replace it; otherwise append integridade after arquivo line
    if grep -A2 -F -- "$f" "$tmp_index" | grep -q 'integridade'; then
      # replace first integridade after the arquivo line
      awk -v file="$f" -v hash="$esc_h" '
        BEGIN{p=0}
        {
          print $0
          if ($0 ~ "arquivo:[[:space:]]*"file"$") { p=2; next }
          if (p>0 && $0 ~ "integridade:") { sub(/:.*/,": "hash); p=0 }
          if (p>0) { p-- }
        }' "$tmp_index" > "${tmp_index}.new" && mv "${tmp_index}.new" "$tmp_index"
    else
      # append integridade line after arquivo line
      awk -v file="$f" -v hash="$esc_h" '
        { print $0
          if ($0 ~ "arquivo:[[:space:]]*"file"$") { print "integridade: "hash }
        }' "$tmp_index" > "${tmp_index}.new" && mv "${tmp_index}.new" "$tmp_index"
    fi
  done
  mv "$tmp_index" "$INDEX_FILE"
  ok "Índice atualizado (backup em ${INDEX_FILE}.bak.*)"
fi

# ---------------------------
# Summary
# ---------------------------
end_ts="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
summary=$(cat <<-SUM
{"run_started":"$start_ts","run_finished":"$end_ts","total":$TOTAL,"ok":$COUNT_OK,"mismatch":$COUNT_MISMATCH,"missing":$COUNT_MISSING,"error":$COUNT_ERROR}
SUM
)
echo "$summary" >> "${jsonlfile}.meta"
info "---------------------------------------------"
info "[Σ_verify] Verificação concluída."
info "Relatório:"
info "  total:    $TOTAL"
info -n ""
printf "  %7s: %5d\n" "íntegro" "$COUNT_OK"
printf "  %7s: %5d\n" "divergente" "$COUNT_MISMATCH"
printf "  %7s: %5d\n" "ausente" "$COUNT_MISSING"
printf "  %7s: %5d\n" "erro" "$COUNT_ERROR"
info "Logs: $logfile"
info "Indexed log: $jsonlfile (meta: ${jsonlfile}.meta)"
info "[Σ_verify] FIAT VOLUNTAS DEI ☀️"

# exit code: 0 if everything ok, 2 if any mismatch/missing/error
if (( COUNT_MISMATCH + COUNT_MISSING + COUNT_ERROR > 0 )); then
  exit 2
else
  exit 0
fi
