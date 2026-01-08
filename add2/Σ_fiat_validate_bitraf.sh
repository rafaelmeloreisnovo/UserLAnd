#!/usr/bin/env bash
# Σ_fiat_validate_bitraf.sh
# Enhanced BitRAF64-aware FIAT validator for RAFAELIA
# - Portable detection of hashing backends (RAFAELIA_HASH / rafhash, blake3, sha3sum, openssl, python fallback, sha256sum)
# - Optional verification against an "expected hashes" YAML
# - CLI flags: --root, --out, --files-file, --verify-file, --concurrency, --dry-run, --color
# - Better YAML-safe output, robust temp cleanup, exit codes and human summary
set -euo pipefail
IFS=$'\n\t'

# Defaults (can be overridden with CLI)
ROOT="${HOME}/RAFAELIA_CORE"
OUT_INDEX="$ROOT/Σ_Index_FIAT.yaml"
FILES=("Σ_PEDAGOGIA_VIVA.yaml" "fiats_pratica.md" "FIAT_UNIFICADO.yaml")
VERIFY_FILE=""
TIMESTAMP=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
CONCURRENCY=1
DRY_RUN=false
COLOR=true

# Colors
if [[ -t 1 && "$COLOR" == true ]]; then
  C_RESET="\033[0m"
  C_INFO="\033[1;34m"
  C_OK="\033[1;32m"
  C_WARN="\033[1;33m"
  C_ERR="\033[1;31m"
else
  C_RESET="" C_INFO="" C_OK="" C_WARN="" C_ERR=""
fi

usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --root DIR            Root directory containing FIAT files (default: ${ROOT})
  --out FILE            Output index YAML (default: ${OUT_INDEX})
  --files-file FILE     Plain text file (one filename per line) to replace built-in FILES
  --verify-file FILE    YAML with expected hashes to verify against
  --concurrency N       Parallel workers for hashing (default: ${CONCURRENCY})
  --dry-run             Show what would be done without writing output
  --no-color            Disable colored output
  -h, --help            Show this help
EOF
  exit 2
}

# Simple CLI parse (POSIX-like)
while [[ $# -gt 0 ]]; do
  case "$1" in
    --root) ROOT="$2"; shift 2 ;;
    --out) OUT_INDEX="$2"; shift 2 ;;
    --files-file) FILES_FILE="$2"; shift 2 ;;
    --verify-file) VERIFY_FILE="$2"; shift 2 ;;
    --concurrency) CONCURRENCY="$2"; shift 2 ;;
    --dry-run) DRY_RUN=true; shift ;;
    --no-color) COLOR=false; shift ;;
    -h|--help) usage ;;
    *) echo -e "${C_ERR}[ERROR] Unknown option: $1${C_RESET}" >&2; usage ;;
  esac
done

# If a files-file was provided, read it
if [[ -n "${FILES_FILE-}" && -f "$FILES_FILE" ]]; then
  mapfile -t FILES < <(sed -n 's/^[[:space:]]*#.*//; /^[[:space:]]*$/d; p' "$FILES_FILE")
fi

# temp dir & cleanup
TMPDIR="$(mktemp -d)"
# Safety cleanup function
cleanup_tmpdir() {
  # Ensure TMPDIR is valid before removal
  if [[ -n "${TMPDIR:-}" && "$TMPDIR" =~ ^/tmp/ && ${#TMPDIR} -gt 5 && -d "$TMPDIR" ]]; then
    rm -rf "$TMPDIR"
  fi
  exit
}
trap 'cleanup_tmpdir' INT TERM EXIT

# dependency probes
have_cmd() { command -v "$1" >/dev/null 2>&1; }

# Probe capabilities and pick best available for various algorithms
probe_hash_capabilities() {
  HAS_RAFLIB=false
  HAS_BLAKE3=false
  HAS_SHA3SUM=false
  HAS_OPENSSL=false
  HAS_PYTHON=false
  HAS_PY_BLAKE3=false
  HAS_SHA256SUM=false

  if have_cmd RAFAELIA_HASH || have_cmd rafhash; then HAS_RAFLIB=true; fi
  if have_cmd blake3; then HAS_BLAKE3=true; fi
  if have_cmd sha3sum; then HAS_SHA3SUM=true; fi
  if have_cmd openssl; then
    # check openssl supports sha3
    if openssl list -digest-algorithms 2>/dev/null | grep -qi sha3; then
      HAS_OPENSSL=true
    fi
  fi
  if have_cmd python3; then
    # test python modules
    if python3 - <<'PY' 2>/dev/null
import sys,hashlib
try:
    hashlib.new("sha3_256")
    sys.exit(0)
except Exception:
    sys.exit(1)
PY
    then
      HAS_PYTHON=true
    fi
    if python3 - <<'PY' 2>/dev/null
try:
    import blake3
    print("ok")
except Exception:
    raise SystemExit(2)
PY
    then
      HAS_PY_BLAKE3=true
    fi
  fi
  if have_cmd sha256sum; then HAS_SHA256SUM=true; fi
}

probe_hash_capabilities

# Load verify-file expected hashes into an associative array for quick lookup
declare -A EXPECTED_SHA3 EXPECTED_BLAKE3 EXPECTED_SHA256
if [[ -n "$VERIFY_FILE" && -f "$VERIFY_FILE" ]]; then
  # Very small YAML parser: expects keys file: "<name>" and sha3_256/blake3/sha256 entries per stanza
  # This keeps the script dependency-free. If the file is more complex, use yq/jq externally.
  current_file=""
  while IFS= read -r line; do
    key=$(echo "$line" | sed -E 's/^[[:space:]]*([a-zA-Z0-9_]+):.*$/\1/;t;d')
    if echo "$line" | grep -E '^[[:space:]]*file:[[:space:]]*".*"$' >/dev/null; then
      current_file=$(echo "$line" | sed -E 's/^[[:space:]]*file:[[:space:]]*"(.*)".*$/\1/')
    elif [[ -n "$current_file" ]]; then
      if echo "$line" | grep -E 'sha3_256:' >/dev/null; then
        EXPECTED_SHA3["$current_file"]=$(echo "$line" | sed -E 's/^[[:space:]]*sha3_256:[[:space:]]*"(.*)".*$/\1/')
      elif echo "$line" | grep -E 'blake3:' >/dev/null; then
        EXPECTED_BLAKE3["$current_file"]=$(echo "$line" | sed -E 's/^[[:space:]]*blake3:[[:space:]]*"(.*)".*$/\1/')
      elif echo "$line" | grep -E 'sha256:' >/dev/null; then
        EXPECTED_SHA256["$current_file"]=$(echo "$line" | sed -E 's/^[[:space:]]*sha256:[[:space:]]*"(.*)".*$/\1/')
      fi
    fi
  done < "$VERIFY_FILE"
fi

# compute_hash: attempts multiple algorithms and returns YAML fragment to stdout
compute_hash() {
  local file="$1"
  local sha3hex="" blake3hex="" sha256hex="" algo_used=""
  # Prefer RAFAELIA_HASH if present (may produce one or more algos)
  if $HAS_RAFLIB; then
    if have_cmd RAFAELIA_HASH; then
      out=$(RAFAELIA_HASH "$file" 2>/dev/null || true)
    else
      out=$(rafhash "$file" 2>/dev/null || true)
    fi
    # normalize: try to detect hex tokens (pick last hex token)
    if [[ "$out" =~ ([0-9a-fA-F]{64,128})$ ]]; then
      candidate="${BASH_REMATCH[1]}"
      # length 64 -> sha3-256 or sha256 (both 64 hex), keep as raw; we assign to sha3 first
      sha3hex="$candidate"
      algo_used="RAFAELIA_HASH"
    else
      # if RAFLIB prints named pairs, attempt to extract known keys
      sha3hex=$(echo "$out" | awk '/sha3/{print $2; exit}')
      blake3hex=$(echo "$out" | awk '/blake3/{print $2; exit}')
      algo_used="RAFAELIA_HASH"
    fi
  fi

  # sha3sum
  if [[ -z "$sha3hex" && $HAS_SHA3SUM == true ]]; then
    sha3hex=$(sha3sum "$file" 2>/dev/null | awk '{print $1}' || true)
    algo_used="${algo_used:+$algo_used, }sha3sum"
  fi

  # openssl sha3-256
  if [[ -z "$sha3hex" && $HAS_OPENSSL == true ]]; then
    sha3hex=$(openssl dgst -sha3-256 -hex "$file" 2>/dev/null | sed -E 's/^.*= //')
    algo_used="${algo_used:+$algo_used, }openssl-sha3"
  fi

  # python fallback for sha3_256
  if [[ -z "$sha3hex" && $HAS_PYTHON == true ]]; then
    sha3hex=$(python3 - <<PY
import sys,hashlib
h=hashlib.new("sha3_256")
with open(sys.argv[1],"rb") as fh:
    for b in iter(lambda: fh.read(8192), b""):
        h.update(b)
print(h.hexdigest())
PY
"$file")
    algo_used="${algo_used:+$algo_used, }python-sha3_256"
  fi

  # blake3
  if [[ -z "$blake3hex" && $HAS_BLAKE3 == true ]]; then
    blake3hex=$(blake3 "$file" 2>/dev/null | awk '{print $1}' || true)
    algo_used="${algo_used:+$algo_used, }blake3"
  elif [[ -z "$blake3hex" && $HAS_PY_BLAKE3 == true ]]; then
    blake3hex=$(python3 - <<PY 2>/dev/null || true
import sys
from blake3 import blake3
h=blake3()
with open(sys.argv[1],"rb") as fh:
    for b in iter(lambda: fh.read(8192), b""):
        h.update(b)
print(h.hexdigest())
PY
"$file")
    algo_used="${algo_used:+$algo_used, }python-blake3"
  fi

  # sha256 fallback (if nothing else)
  if [[ -z "$sha3hex" && -z "$blake3hex" && $HAS_SHA256SUM == true ]]; then
    sha256hex=$(sha256sum "$file" 2>/dev/null | awk '{print $1}' || true)
    algo_used="${algo_used:+$algo_used, }sha256sum"
  fi

  # Build YAML fragment
  printf "  - file: \"%s\"\n" "$(basename "$file")"
  printf "    path: \"%s\"\n" "$file"
  printf "    timestamp: \"%s\"\n" "$TIMESTAMP"
  if [[ -n "$sha3hex" ]]; then
    printf "    sha3_256: \"%s\"\n" "$sha3hex"
  fi
  if [[ -n "$blake3hex" ]]; then
    printf "    blake3: \"%s\"\n" "$blake3hex"
  fi
  if [[ -n "$sha256hex" ]]; then
    printf "    sha256: \"%s\"\n" "$sha256hex"
  fi
  printf "    algo_used: \"%s\"\n" "${algo_used:-unknown}"
}

# verify_hashes: compare computed values with EXPECTED arrays and return status
verify_hashes_for_file() {
  local fname="$1"
  local status="OK"
  local reason=""
  if [[ -n "${EXPECTED_SHA3[$fname]:-}" ]]; then
    if [[ "${EXPECTED_SHA3[$fname]}" != "${COMPUTED_SHA3:-}" && -n "${COMPUTED_SHA3:-}" ]]; then
      status="MISMATCH"
      reason="sha3_256 mismatch"
    fi
  fi
  if [[ -n "${EXPECTED_BLAKE3[$fname]:-}" && -n "${COMPUTED_BLAKE3:-}" ]]; then
    if [[ "${EXPECTED_BLAKE3[$fname]}" != "${COMPUTED_BLAKE3:-}" ]]; then
      status="MISMATCH"
      reason="${reason:+$reason; }blake3 mismatch"
    fi
  fi
  if [[ -n "${EXPECTED_SHA256[$fname]:-}" && -n "${COMPUTED_SHA256:-}" ]]; then
    if [[ "${EXPECTED_SHA256[$fname]}" != "${COMPUTED_SHA256:-}" ]]; then
      status="MISMATCH"
      reason="${reason:+$reason; }sha256 mismatch"
    fi
  fi
  # Print suffix lines for status (single line)
  if [[ "$status" == "OK" ]]; then
    printf "    status: \"OK\"\n"
  else
    printf "    status: \"MISMATCH\"\n"
    printf "    note: \"%s\"\n" "$reason"
  fi
}

# Main run
echo -e "${C_INFO}[Σ_validate_bitraf] Iniciando validação BitRAF64-aware...${C_RESET}"
index_tmp="$TMPDIR/index.yaml"
{
  printf "index_generated: \"%s\"\n" "$TIMESTAMP"
  printf "root: \"%s\"\n" "$ROOT"
  printf "fiats:\n"
} > "$index_tmp"

all_ok=true
# iterate files (optionally parallelize by simple background jobs limited by CONCURRENCY)
pids=()
active_jobs=0
for f in "${FILES[@]}"; do
  # trim whitespace
  f=$(echo "$f" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
  [[ -z "$f" ]] && continue
  path="$ROOT/$f"
  if [[ ! -f "$path" ]]; then
    {
      printf "  - file: \"%s\"\n" "$f"
      printf "    path: \"%s\"\n" "$path"
      printf "    timestamp: \"%s\"\n" "$TIMESTAMP"
      printf "    status: \"MISSING\"\n"
    } >> "$index_tmp"
    echo -e "${C_WARN}[WARN] Arquivo ausente: $f${C_RESET}"
    all_ok=false
    continue
  fi

  # compute hashes inline (could be backgrounded)
  (
    # produce YAML fragment and write to a per-file temp then append
    frag_tmp="$TMPDIR/frag_$(basename "$f").yaml"
    compute_hash "$path" > "$frag_tmp"
    # For verification, export computed variables in a simple way by parsing frag_tmp
    COMPUTED_SHA3=$(grep -E 'sha3_256:' "$frag_tmp" 2>/dev/null | sed -E 's/^[[:space:]]*sha3_256:[[:space:]]*"(.*)".*$/\1/' || true)
    COMPUTED_BLAKE3=$(grep -E 'blake3:' "$frag_tmp" 2>/dev/null | sed -E 's/^[[:space:]]*blake3:[[:space:]]*"(.*)".*$/\1/' || true)
    COMPUTED_SHA256=$(grep -E 'sha256:' "$frag_tmp" 2>/dev/null | sed -E 's/^[[:space:]]*sha256:[[:space:]]*"(.*)".*$/\1/' || true)
    # Append verification result
    if [[ -n "$VERIFY_FILE" ]]; then
      verify_hashes_for_file "$(basename "$f")" >> "$frag_tmp"
    else
      # default: OK
      printf "    status: \"OK\"\n" >> "$frag_tmp"
    fi
    # append to main index (atomic append)
    cat "$frag_tmp" >> "$index_tmp"
  ) &

  pids+=($!)
  active_jobs=$((active_jobs+1))
  # throttle
  if [[ "$CONCURRENCY" -gt 0 && "$active_jobs" -ge "$CONCURRENCY" ]]; then
    wait "${pids[@]}"
    pids=()
    active_jobs=0
  fi
done

# wait for any remaining jobs
if [[ "${#pids[@]}" -gt 0 ]]; then
  wait "${pids[@]}" || true
fi

# Decide global state
if [[ "$VERIFY_FILE" != "" ]]; then
  # if verify provided, detect mismatches in file
  if grep -q 'status: "MISMATCH"' "$index_tmp" 2>/dev/null; then
    all_ok=false
  fi
fi

if $all_ok; then
  printf "estado: \"íntegro\"\n" >> "$index_tmp"
else
  printf "estado: \"parcial/aviso\"\n" >> "$index_tmp"
fi

# Write out (unless dry-run)
if $DRY_RUN; then
  echo -e "${C_INFO}[DRY-RUN] Index preview:${C_RESET}"
  cat "$index_tmp"
else
  mkdir -p "$(dirname "$OUT_INDEX")"
  mv "$index_tmp" "$OUT_INDEX"
  chmod 644 "$OUT_INDEX"
  echo -e "${C_OK}[Σ_validate_bitraf] Index gravado em: $OUT_INDEX${C_RESET}"
fi

# human summary
echo -e "${C_INFO}[Σ_validate_bitraf] Resultado resumido:${C_RESET}"
if [[ -f "$OUT_INDEX" ]]; then
  # show concise summary lines
  grep -E 'file:|status:|estado:' "$OUT_INDEX" || true
fi

if $all_ok; then
  echo -e "${C_OK}[Σ_validate_bitraf] FIAT_VOLUNTAS_DEI — integridade preservada ☀️${C_RESET}"
  exit 0
else
  echo -e "${C_WARN}[Σ_validate_bitraf] FIAT_VOLUNTAS_DEI — atenção: arquivos faltantes/erros ou divergências de hash${C_RESET}"
  exit 3
fi