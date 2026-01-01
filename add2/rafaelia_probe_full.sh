#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ROOT="${ROOT:-$HOME}"
OUTDIR="${OUTDIR:-$ROOT/out}"
mkdir -p "$OUTDIR"

ts(){ date +%s; }
log(){ echo "[$(date '+%F %T')] $*"; }

# ---------- 0) paths ----------
SYSINFO="$OUTDIR/sysinfo.txt"
MEM_BEFORE="$OUTDIR/mem_before.txt"
MEM_AFTER="$OUTDIR/mem_after.txt"
CPUFREQ="$OUTDIR/cpufreq.txt"
CACHE_SYSFS="$OUTDIR/cache_sysfs.txt"
LSCPU="$OUTDIR/lscpu.txt"
TIMEV="$OUTDIR/timev_toroidal_soc.txt"
FAULTS="$OUTDIR/faults.txt"
RUNLOG="$OUTDIR/run_toroidal_soc.log"

# ---------- 1) coleta estática ----------
log "[1/6] SYSINFO + LSCPU"
{
  echo "== DATE =="; date
  echo
  echo "== UNAME =="; uname -a || true
  echo
  echo "== GETPROP (Android) ==";
  getprop ro.product.model 2>/dev/null || true
  getprop ro.board.platform 2>/dev/null || true
  getprop ro.hardware 2>/dev/null || true
  getprop ro.build.version.release 2>/dev/null || true
  getprop ro.build.version.sdk 2>/dev/null || true
  echo
  echo "== /proc/cpuinfo (head) =="; sed -n '1,120p' /proc/cpuinfo 2>/dev/null || true
  echo
  echo "== /proc/meminfo (recorte) =="; grep -E 'MemTotal|MemFree|MemAvailable|Buffers|Cached|SwapTotal|SwapFree|SwapCached|SReclaimable|Shmem|Dirty|Writeback|PageTables|KernelStack' /proc/meminfo 2>/dev/null || true
} > "$SYSINFO"

# ---------- 2) cache sysfs (com zsh/glob issues resolvidos) ----------
log "[2/6] CACHE (sysfs) + LSCPU cache"
{
  echo "== CACHE (sysfs) =="
  # evita falha se não existir
  shopt -s nullglob 2>/dev/null || true
  for i in /sys/devices/system/cpu/cpu0/cache/index*; do
    echo "--- $i ---"
    cat "$i/level" 2>/dev/null || true
    cat "$i/type"  2>/dev/null || true
    cat "$i/size"  2>/dev/null || true
    cat "$i/coherency_line_size" 2>/dev/null || true
    cat "$i/shared_cpu_list" 2>/dev/null || true
  done
  echo
  echo "== ALT: lscpu cache =="
  lscpu 2>/dev/null | grep -E -i 'cache|model name|architecture|cpu\(s\)|thread|core|socket' || true
} > "$CACHE_SYSFS"

lscpu 2>/dev/null | grep -E -i 'cache|model name|architecture|cpu\(s\)|thread|core|socket' > "$LSCPU" || true

# ---------- 3) cpufreq snapshot ----------
log "[3/6] CPUFREQ snapshot"
{
  echo "== CPUFREQ scaling_cur_freq =="
  for f in /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq; do
    [ -f "$f" ] || continue
    echo -n "$(basename "$(dirname "$f")") : "
    cat "$f" 2>/dev/null || true
  done
  echo
  echo "== CPUFREQ scaling_governor =="
  for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    [ -f "$g" ] || continue
    echo -n "$(basename "$(dirname "$g")") : "
    cat "$g" 2>/dev/null || true
  done
} > "$CPUFREQ" || true

# ---------- 4) BEFORE meminfo ----------
log "[4/6] MEMINFO BEFORE"
grep -E 'MemAvailable|Cached|SwapCached|SwapTotal|SwapFree|Dirty|Writeback|WritebackTmp' /proc/meminfo > "$MEM_BEFORE" || true

# ---------- 5) faults + time -v + execução ----------
# (a) faults baseline
log "[5/6] Faults baseline"
python3 - <<'PY' > "$FAULTS"
import resource, time
a = resource.getrusage(resource.RUSAGE_SELF)
print("baseline  minflt majflt:", a.ru_minflt, a.ru_majflt)
time.sleep(0.2)
b = resource.getrusage(resource.RUSAGE_SELF)
print("after200ms minflt majflt:", b.ru_minflt, b.ru_majflt)
PY

# (b) executar TOROIDAL_SOC: tenta medir completo; se travar, mata em 15s e ainda coleta AFTER
SCRIPT="RAFAELIA_TOROIDAL_SOC.py"
if [ ! -f "$SCRIPT" ]; then
  log "[ERRO] Não achei $SCRIPT no diretório atual: $(pwd)"
  log "      Dica: rode dentro da pasta onde estão os scripts do benchmark."
  exit 2
fi

log "[5/6] RUN TOROIDAL_SOC (time -v) — se não terminar, timeout 15s"
START_TS="$(ts)"

# tenta /usr/bin/time -v se existir, senão cai pro builtin time
TIMEBIN="/usr/bin/time"
if [ -x "$TIMEBIN" ]; then
  # timeout via background + sleep + kill (sem depender de coreutils timeout)
  ( "$TIMEBIN" -v python3 "$SCRIPT" >/dev/null ) >"$RUNLOG" 2>"$TIMEV" &
  PID=$!
  sleep 15
  if kill -0 "$PID" 2>/dev/null; then
    log "  [WARN] ainda rodando após 15s -> kill"
    kill "$PID" 2>/dev/null || true
    sleep 1
    kill -9 "$PID" 2>/dev/null || true
    echo "NOTE: killed after 15s (did not finish)" >> "$TIMEV"
  fi
  wait "$PID" 2>/dev/null || true
else
  # fallback
  ( time python3 "$SCRIPT" >/dev/null ) >"$RUNLOG" 2>"$TIMEV" &
  PID=$!
  sleep 15
  if kill -0 "$PID" 2>/dev/null; then
    log "  [WARN] ainda rodando após 15s -> kill"
    kill "$PID" 2>/dev/null || true
    sleep 1
    kill -9 "$PID" 2>/dev/null || true
    echo "NOTE: killed after 15s (did not finish)" >> "$TIMEV"
  fi
  wait "$PID" 2>/dev/null || true
fi

END_TS="$(ts)"
echo "wall_seconds=$((END_TS-START_TS))" >> "$TIMEV"

# (c) AFTER meminfo
log "[6/6] MEMINFO AFTER"
grep -E 'MemAvailable|Cached|SwapCached|SwapTotal|SwapFree|Dirty|Writeback|WritebackTmp' /proc/meminfo > "$MEM_AFTER" || true

# ---------- 6) resumo rápido no terminal ----------
log "[DONE] Artefatos em: $OUTDIR"
echo
echo "== QUICK SUMMARY =="
echo "-- BEFORE --"; cat "$MEM_BEFORE" || true
echo "-- AFTER  --"; cat "$MEM_AFTER" || true
echo
echo "Veja também:"
echo "  $SYSINFO"
echo "  $CACHE_SYSFS"
echo "  $CPUFREQ"
echo "  $TIMEV"
echo "  $RUNLOG"
echo "  $FAULTS"
