#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ROOT="${ROOT:-$HOME}"
OUT="$ROOT/out"
BENCH_JSON="$OUT/bench.json"
SYSINFO="$OUT/sysinfo.txt"
MBEF="$OUT/mem_before.txt"
MAFT="$OUT/mem_after.txt"
STRIDE_CSV="$OUT/cache_stride.csv"

mkdir -p "$OUT"

echo "[*] paths:"
echo "  bench=$BENCH_JSON"
echo "  sysinfo=$SYSINFO"
echo "  mem_before=$MBEF"
echo "  mem_after=$MAFT"
echo "  stride=$STRIDE_CSV"
echo

python3 - <<'PY'
import json, re, os, math, statistics
from pathlib import Path

OUT = Path(os.environ.get("OUT", str(Path.home()/ "out")))
bench_p = OUT/"bench.json"
sysinfo_p = OUT/"sysinfo.txt"
mbef_p = OUT/"mem_before.txt"
maft_p = OUT/"mem_after.txt"
stride_p = OUT/"cache_stride.csv"

def parse_mem(text:str):
    d={}
    for line in text.splitlines():
        m=re.match(r'^(\w+):\s+(\d+)\s*kB', line)
        if m: d[m.group(1)] = int(m.group(2))
    return d

def pick_sysinfo(text:str):
    want = ["MemTotal","MemAvailable","Buffers","Cached","SwapCached","SwapTotal","SwapFree","Dirty","Writeback","KernelStack","PageTables"]
    d=parse_mem(text)
    out=[]
    for k in want:
        if k in d: out.append(f"{k}: {d[k]} kB")
    # lscpu recortes
    for line in text.splitlines():
        if any(x in line for x in ["Architecture:", "CPU(s):", "On-line CPU(s) list:", "Model name:"]):
            out.append(line.strip())
    return out

print("╔══════════════════════════════════════════════╗")
print("║ RAFAELIA :: BENCH REPORT (Termux/aarch64)     ║")
print("╚══════════════════════════════════════════════╝")

# --- BENCH.JSON ---
if not bench_p.exists():
    raise SystemExit(f"[ERR] faltou {bench_p}")

bench = json.loads(bench_p.read_text(errors="ignore"))
sh = bench.get("shell", {})
c  = bench.get("c", {})

# shell: em vez de loops, use archetypes_total (100) como “loops efetivos”
sh_dur_ms = float(sh.get("duration_ms", 0.0))
sh_dur_s  = sh_dur_ms/1000.0 if sh_dur_ms>0 else 0.0
ar_total  = int(sh.get("archetypes_total", 0))
chars_total = int(sh.get("chars_total", 0))

ar_ps = float(sh.get("archetypes_per_sec", 0.0))
ch_ps = float(sh.get("chars_per_sec", 0.0))

loops_eff = ar_total
loops_ps = (loops_eff/sh_dur_s) if sh_dur_s>0 else 0.0

# C bench
iters = int(c.get("iters", 0))
c_dur_ns = int(c.get("duration_ns", 0))
c_dur_s = c_dur_ns/1e9 if c_dur_ns>0 else 0.0
itps = float(c.get("iters_per_sec", 0.0))

print("\n== SYSINFO (recortes) ==")
if sysinfo_p.exists():
    for line in pick_sysinfo(sysinfo_p.read_text(errors="ignore")):
        print(" ", line)
else:
    print(" [WARN] sysinfo.txt não encontrado")

print("\n== BENCH: SHELL ==")
print(f" loops_eff          : {loops_eff}")
print(f" duration_s         : {sh_dur_s:.6f}")
print(f" loops_eff_per_sec  : {loops_ps:.3f}")
print(f" chars_total        : {chars_total}")
print(f" chars_per_sec      : {ch_ps}")
print(f" archetypes_total   : {ar_total}")
print(f" archetypes_per_sec : {ar_ps}")

print("\n== BENCH: C ==")
print(f" iters              : {iters}")
print(f" duration_s         : {c_dur_s:.6f}")
print(f" iters_per_sec      : {itps:.0f}")

print("\n== DIAGNÓSTICO ==")
if itps>0 and loops_ps>0:
    gap = itps/loops_ps
    print(f" gap(C_vs_shell)    : ~{gap:,.0f}x  (spawn/pipe/IO vs loop tight)")
print(" shell ≈ orquestração/processos/IO; C ≈ throughput CPU puro.")
print(" próximo: medir cache/TLB/stride + pagefaults + throttling.")

# --- MEM DELTA (TOROIDAL_SOC) ---
if mbef_p.exists() and maft_p.exists():
    b=parse_mem(mbef_p.read_text(errors="ignore"))
    a=parse_mem(maft_p.read_text(errors="ignore"))
    if "SwapFree" in b and "SwapFree" in a:
        used_kb = b["SwapFree"] - a["SwapFree"]
        print("\n== MEM DELTA (TOROIDAL_SOC) ==")
        print(f" SwapFree delta     : -{used_kb} kB  (~{used_kb/1024:.1f} MB usados)")
        for k in ["MemAvailable","Cached","SwapCached","Dirty","Writeback"]:
            if k in b and k in a:
                print(f" {k:12s}       : {a[k]-b[k]:+d} kB")

# --- STRIDE CSV (cache line / page) ---
if stride_p.exists():
    rows=[]
    for line in stride_p.read_text(errors="ignore").splitlines():
        line=line.strip()
        if not line or line.startswith("#"): continue
        if "," in line:
            s,ns=line.split(",",1)
            try:
                rows.append((int(s.strip()), float(ns.strip())))
            except: pass
    rows.sort()
    if rows:
        # maiores saltos
        deltas=[]
        prev=None
        for s,ns in rows:
            if prev:
                deltas.append((ns-prev[1], s, ns, prev[0], prev[1]))
            prev=(s,ns)
        deltas.sort(reverse=True, key=lambda x:x[0])
        print("\n== CACHE STRIDE (inferência) ==")
        print(" sinais típicos:")
        print("  • salto perto de 64B  => cache line")
        print("  • salto perto de 4096 => página / TLB")
        print("\n top saltos (Δns, stride_atual, ns_atual, stride_prev, ns_prev):")
        for d in deltas[:6]:
            print(f"  Δ{d[0]:6.3f}  | {d[1]:7d}B {d[2]:7.3f}ns  <= {d[3]:7d}B {d[4]:7.3f}ns")

PY

echo
echo "[OK] relatório acima (corrigido: loops_eff != 0)."
