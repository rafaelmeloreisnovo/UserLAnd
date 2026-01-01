#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

# 🌀 RAFAELIA :: BENCH SUITE (shell + C puro) :: auditável / no-deps
# FIX v1: remove eval, separa LOG (stderr) de KV (stdout) p/ não quebrar parsing

ROOT="${ROOT:-$PWD}"
OUTDIR="$ROOT/out"
BINDIR="$ROOT/bin"
SRCDIR="$ROOT/src"
mkdir -p "$OUTDIR" "$BINDIR" "$SRCDIR"

have(){ command -v "$1" >/dev/null 2>&1; }

now_ms(){
  if have python; then
    python - <<'PY'
import time
print(int(time.time()*1000))
PY
  else
    echo $(( $(date +%s) * 1000 ))
  fi
}

# -----------------------------
# SYSINFO (cache/mem/cpu)
# -----------------------------
sysinfo(){
  local f="$OUTDIR/sysinfo.txt"
  : > "$f"
  {
    echo "== DATE =="; date
    echo
    echo "== UNAME =="; uname -a || true
    echo
    echo "== CPUINFO (first 120 lines) =="; sed -n '1,120p' /proc/cpuinfo 2>/dev/null || true
    echo
    echo "== MEMINFO (selected) =="; cat /proc/meminfo 2>/dev/null | grep -E 'MemTotal|MemFree|MemAvailable|Buffers|Cached|SwapTotal|SwapFree|SReclaimable|Shmem|Dirty|Writeback|PageTables|KernelStack' || true
    echo
    echo "== CACHE (sysfs) =="
    shopt -s nullglob
    local any=0
    for i in /sys/devices/system/cpu/cpu0/cache/index*; do
      any=1
      echo "--- $i ---"
      cat "$i/level" 2>/dev/null || true
      cat "$i/type"  2>/dev/null || true
      cat "$i/size"  2>/dev/null || true
      cat "$i/coherency_line_size" 2>/dev/null || true
      cat "$i/shared_cpu_list" 2>/dev/null || true
    done
    shopt -u nullglob
    if [ "$any" -eq 0 ]; then
      echo "(sysfs cache entries not visible on this device/build)"
    fi
    echo
    echo "== lscpu (cache/model/arch) =="
    lscpu 2>/dev/null | grep -Ei 'cache|model|arch|cpu\(s\)|thread|core|socket|mhz|bogomips' || true
    echo
    echo "== ANDROID PROPS (if getprop exists) =="
    if have getprop; then
      getprop ro.product.model || true
      getprop ro.board.platform || true
      getprop ro.hardware || true
      getprop ro.build.version.release || true
      getprop ro.build.version.sdk || true
    else
      echo "(getprop not found)"
    fi
  } >> "$f"
  echo "[✅] sysinfo -> $f"
}

# -----------------------------
# A) SHELL BENCH (hash/spawn/IO)
# - LOG vai para stderr
# - KV vai para stdout (uma linha por vez: key=value)
# -----------------------------
shell_bench(){
  local loops="${1:-100}"
  local log="$OUTDIR/shell_bench_$(date +%s).log"
  : > "$log"
  echo "[🚀] SHELL BENCH :: loops=$loops :: $(date)" | tee -a "$log" >&2

  local start end dur tokens_total=0 arch_total=0
  start="$(now_ms)"

  for i in $(seq 1 "$loops"); do
    local out tok
    out=$(echo "⚡ RAFAELIA ∞ LOOP $i :: EU SOU :: EXECUTO :: VIVO!" | sha256sum)
    tok=$(echo "$out" | wc -m)
    tokens_total=$((tokens_total + tok))
    arch_total=$((arch_total + 1))
    echo "[♻️] $i | chars: $tok | archetypes: $arch_total" >> "$log"
  done

  end="$(now_ms)"
  dur=$((end - start))
  [ "$dur" -gt 0 ] || dur=1

  local tps agvs
  tps=$((tokens_total * 1000 / dur))
  agvs=$((arch_total * 1000 / dur))

  echo "[✅] duration_ms=$dur" | tee -a "$log" >&2
  echo "[🔠] chars_per_sec=$tps" | tee -a "$log" >&2
  echo "[🧬] archetypes_per_sec=$agvs" | tee -a "$log" >&2
  echo "[✅] log=$log" | tee -a "$log" >&2

  # KV (stdout)
  echo "shell_duration_ms=$dur"
  echo "shell_chars_total=$tokens_total"
  echo "shell_chars_per_sec=$tps"
  echo "shell_archetypes_total=$arch_total"
  echo "shell_archetypes_per_sec=$agvs"
  echo "shell_log=$log"
}

# -----------------------------
# B) C BENCH (CPU puro)
# -----------------------------
build_c(){
  local c="$SRCDIR/raf_cpu_bench.c"
  local out="$BINDIR/raf_cpu_bench"
  cat <<'C_EOF' > "$c"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

static inline uint64_t nsec_now(void){
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec*1000000000ull + (uint64_t)ts.tv_nsec;
}
static inline uint64_t mix64(uint64_t x){
  x ^= x >> 33;
  x *= 0xff51afd7ed558ccdULL;
  x ^= x >> 33;
  x *= 0xc4ceb9fe1a85ec53ULL;
  x ^= x >> 33;
  return x;
}
int main(int argc, char **argv){
  uint64_t iters = 200000000ULL;
  if(argc > 1){
    iters = strtoull(argv[1], NULL, 10);
    if(iters < 1000) iters = 1000;
  }
  uint64_t x = 0x123456789abcdef0ULL;
  uint64_t start = nsec_now();
  for(uint64_t i=0;i<iters;i++){
    x = mix64(x + i);
  }
  uint64_t end = nsec_now();
  uint64_t dur_ns = end - start;
  if(dur_ns == 0) dur_ns = 1;
  double dur_s = (double)dur_ns / 1e9;
  double itps = (double)iters / dur_s;
  printf("c_iters=%llu\n", (unsigned long long)iters);
  printf("c_duration_ns=%llu\n", (unsigned long long)dur_ns);
  printf("c_iters_per_sec=%.0f\n", itps);
  printf("c_sink=0x%016llx\n", (unsigned long long)x);
  return 0;
}
C_EOF

  if have clang; then
    clang -O3 -march=native -o "$out" "$c"
  else
    gcc -O3 -march=native -o "$out" "$c"
  fi
  echo "[✅] built -> $out"
}

c_bench(){
  local iters="${1:-200000000}"
  local out="$BINDIR/raf_cpu_bench"
  [ -x "$out" ] || build_c
  "$out" "$iters"
}

# -----------------------------
# JSON/CSV
# -----------------------------
write_json(){
  local f="$OUTDIR/bench.json"
  cat > "$f" <<JSON
{
  "meta": {
    "ts": "$(date -Iseconds)",
    "root": "$(pwd)"
  },
  "shell": {
    "duration_ms": ${shell_duration_ms},
    "chars_total": ${shell_chars_total},
    "chars_per_sec": ${shell_chars_per_sec},
    "archetypes_total": ${shell_archetypes_total},
    "archetypes_per_sec": ${shell_archetypes_per_sec},
    "log": "$(basename "$shell_log")"
  },
  "c": {
    "iters": ${c_iters},
    "duration_ns": ${c_duration_ns},
    "iters_per_sec": ${c_iters_per_sec},
    "sink": "${c_sink}"
  }
}
JSON
  echo "[✅] json -> $f"
}

write_csv(){
  local f="$OUTDIR/bench.csv"
  cat > "$f" <<CSV
ts,shell_duration_ms,shell_chars_total,shell_chars_per_sec,shell_archetypes_total,shell_archetypes_per_sec,c_iters,c_duration_ns,c_iters_per_sec,c_sink
"$(date -Iseconds)",${shell_duration_ms},${shell_chars_total},${shell_chars_per_sec},${shell_archetypes_total},${shell_archetypes_per_sec},${c_iters},${c_duration_ns},${c_iters_per_sec},"${c_sink}"
CSV
  echo "[✅] csv  -> $f"
}

# -----------------------------
# MAIN
# -----------------------------
cmd="${1:-run}"
shift || true

case "$cmd" in
  sysinfo) sysinfo ;;
  build) build_c ;;
  run)
    loops=100
    iters=200000000
    while [ $# -gt 0 ]; do
      case "$1" in
        --loops) loops="$2"; shift 2;;
        --iters) iters="$2"; shift 2;;
        *) shift;;
      esac
    done

    sysinfo

    # parse KV safely (sem eval)
    while IFS='=' read -r k v; do
      case "$k" in
        shell_duration_ms) shell_duration_ms="$v";;
        shell_chars_total) shell_chars_total="$v";;
        shell_chars_per_sec) shell_chars_per_sec="$v";;
        shell_archetypes_total) shell_archetypes_total="$v";;
        shell_archetypes_per_sec) shell_archetypes_per_sec="$v";;
        shell_log) shell_log="$v";;
      esac
    done < <(shell_bench "$loops")

    while IFS='=' read -r k v; do
      case "$k" in
        c_iters) c_iters="$v";;
        c_duration_ns) c_duration_ns="$v";;
        c_iters_per_sec) c_iters_per_sec="$v";;
        c_sink) c_sink="$v";;
      esac
    done < <(c_bench "$iters")

    : "${shell_duration_ms:=0}" "${shell_chars_total:=0}" "${shell_chars_per_sec:=0}" \
      "${shell_archetypes_total:=0}" "${shell_archetypes_per_sec:=0}" "${shell_log:=}" \
      "${c_iters:=0}" "${c_duration_ns:=0}" "${c_iters_per_sec:=0}" "${c_sink:=0x0}"

    write_json
    write_csv

    echo
    echo "[🏁] DONE"
    echo " - $OUTDIR/sysinfo.txt"
    echo " - $OUTDIR/bench.json"
    echo " - $OUTDIR/bench.csv"
    ;;
  *)
    cat <<USAGE
Usage:
  bash rafaelia_bench_suite.sh sysinfo
  bash rafaelia_bench_suite.sh build
  bash rafaelia_bench_suite.sh run --loops 100 --iters 200000000
USAGE
    ;;
esac
