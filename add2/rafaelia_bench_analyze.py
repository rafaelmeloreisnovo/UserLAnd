#!/usr/bin/env python3
import json, csv
from pathlib import Path

ROOT = Path("out")
J = ROOT/"bench.json"
C = ROOT/"bench.csv"
S = ROOT/"sysinfo.txt"
MB = ROOT/"mem_before.txt"
MA = ROOT/"mem_after.txt"

def load_json(p: Path):
  try: return json.loads(p.read_text(encoding="utf-8"))
  except: return {}

def load_csv_firstrow(p: Path):
  try:
    with p.open(newline="") as f:
      r = csv.DictReader(f)
      for row in r: return row
  except: pass
  return {}

def parse_mem(p: Path):
  d={}
  if not p.exists(): return d
  for line in p.read_text(errors="ignore").splitlines():
    if ":" in line and "kB" in line:
      k = line.split(":")[0].strip()
      v = line.split(":")[1].strip().split()[0]
      if v.isdigit(): d[k]=int(v)
  return d

def pick(lines, keys):
  out=[]
  for ln in lines.splitlines():
    for k in keys:
      if ln.startswith(k+":"): out.append(ln.strip())
  return out

def main():
  bj  = load_json(J)
  row = load_csv_firstrow(C)

  shellj = bj.get("shell", {}) or {}
  cj     = bj.get("c", {}) or {}

  # ---- SHELL ----
  # CSV não tem loops -> use archetypes_total ou JSON
  loops = int(row.get("loops", 0) or 0)
  if loops == 0:
    loops = int(row.get("archetypes_total", 0) or 0)
  if loops == 0:
    loops = int(shellj.get("loops", 0) or 0)

  dur_ms = float(row.get("duration_ms", 0) or 0)
  if dur_ms == 0:
    dur_ms = float(shellj.get("duration_ms", 0) or 0)

  chars_ps = int(row.get("chars_per_sec", shellj.get("chars_per_sec", 0)) or 0)
  arch_ps  = int(row.get("archetypes_per_sec", shellj.get("archetypes_per_sec", 0)) or 0)

  loops_ps = (loops*1000.0/dur_ms) if dur_ms>0 else 0.0

  # ---- C ----
  iters = int(cj.get("iters", 0) or 0)
  c_ms  = float(cj.get("duration_ms", 0) or 0)

  # alguns logs podem salvar duration_ns (ou wall/cpu separados)
  if c_ms == 0:
    c_ns = float(cj.get("duration_ns", 0) or 0)
    if c_ns > 0: c_ms = c_ns / 1e6

  itps  = (iters*1000.0/c_ms) if c_ms>0 else 0.0

  print("╔══════════════════════════════════════════════╗")
  print("║ RAFAELIA :: BENCH ANALYZE (Termux/aarch64)    ║")
  print("╚══════════════════════════════════════════════╝")
  print(f"[paths] csv={C} | json={J} | sysinfo={S}")

  if S.exists():
    print("\n== SYSINFO (recortes) ==")
    for ln in pick(S.read_text(errors="ignore"),
                   ["MemTotal","MemAvailable","Buffers","Cached","SwapCached","SwapTotal","SwapFree",
                    "Dirty","Writeback","KernelStack","PageTables"]):
      print(" ", ln)

  print("\n== BENCH: SHELL ==")
  print(f" loops              : {loops}")
  print(f" duration_ms        : {dur_ms:.3f}")
  print(f" loops_per_sec      : {loops_ps:.3f}")
  print(f" chars_per_sec      : {chars_ps}")
  print(f" archetypes_per_sec : {arch_ps}")

  print("\n== BENCH: C ==")
  print(f" iters              : {iters}")
  print(f" duration_ms        : {c_ms:.3f}")
  print(f" iters_per_sec      : {itps:.0f}")

  print("\n== DIAGNÓSTICO ==")
  if itps>0 and loops_ps>0:
    print(f" gap(C_vs_shell)    : ~{itps/loops_ps:,.0f}x (spawn/pipe/IO vs loop tight)")
  print(" shell ≈ orquestração/processos/IO; C ≈ throughput CPU puro.")
  print(" próximo: cache/TLB/stride + pagefaults + throttling.")

  if MB.exists() and MA.exists():
    b = parse_mem(MB); a = parse_mem(MA)
    if "SwapFree" in b and "SwapFree" in a:
      used_kb = b["SwapFree"] - a["SwapFree"]
      print("\n== MEM DELTA (TOROIDAL_SOC) ==")
      print(f" SwapFree delta     : -{used_kb} kB  (~{used_kb/1024:.1f} MB usados)")
      for k in ["MemAvailable","Cached","SwapCached","Dirty","Writeback"]:
        if k in b and k in a:
          print(f" {k:12s}       : {a[k]-b[k]:+d} kB")

if __name__ == "__main__":
  main()
