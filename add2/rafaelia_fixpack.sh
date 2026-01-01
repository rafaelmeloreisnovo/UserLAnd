#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ROOT="${HOME}"
OUT="${ROOT}/out"
mkdir -p "$OUT"

echo "[1/3] Criando probe de cache robusto..."
cat <<'SH' > "${ROOT}/rafaelia_probe_cache.sh"
#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

# zsh compat (se rodar no zsh)
( setopt NULL_GLOB 2>/dev/null ) || true

echo "== CACHE (sysfs) =="
BASE="/sys/devices/system/cpu"
if ls "${BASE}/cpu0/cache/index"* >/dev/null 2>&1; then
  for i in "${BASE}/cpu0/cache/index"*; do
    echo "--- $i ---"
    cat "$i/level" 2>/dev/null || true
    cat "$i/type"   2>/dev/null || true
    cat "$i/size"   2>/dev/null || true
    cat "$i/coherency_line_size" 2>/dev/null || true
    cat "$i/shared_cpu_list" 2>/dev/null || true
  done
else
  echo "[WARN] sysfs cache padrão não exposto. Usando fallback via find."
  find "$BASE" -maxdepth 5 -type f \( -name level -o -name type -o -name size -o -name coherency_line_size -o -name shared_cpu_list \) 2>/dev/null \
    | sed 's#^#file: #' | head -n 200
fi

echo
echo "== LSCPU (cache/model/arch) =="
lscpu 2>/dev/null | grep -Ei 'cache|model|arch|cpu\(s\)|thread|core' || true
SH
chmod +x "${ROOT}/rafaelia_probe_cache.sh"

echo "[2/3] Criando analyzer (out/bench.json + sysinfo)..."
cat <<'PY' > "${ROOT}/rafaelia_bench_analyze.py"
#!/usr/bin/env python3
import json, os, re

ROOT=os.path.expanduser("~")
bench_path=os.path.join(ROOT,"out","bench.json")
sysinfo_path=os.path.join(ROOT,"out","sysinfo.txt")

def load_json(p):
    with open(p,"r",encoding="utf-8") as f: return json.load(f)

def pick_sysinfo(txt):
    keep=[]
    for line in txt.splitlines():
        if re.search(r'^(MemTotal|MemAvailable|Buffers|Cached|SwapTotal|SwapFree|SwapCached|Dirty|Writeback):', line):
            keep.append(line.strip())
        if re.search(r'^(Architecture:|CPU\(s\):|Model name:)', line):
            keep.append(line.strip())
    # dedup preservando ordem
    out=[]
    seen=set()
    for x in keep:
        if x not in seen:
            seen.add(x); out.append(x)
    return out[:30]

def main():
    print("╔══════════════════════════════════════════════╗")
    print("║ RAFAELIA :: BENCH ANALYZE (Termux/aarch64)    ║")
    print("╚══════════════════════════════════════════════╝")
    print(f"[paths] bench={os.path.relpath(bench_path, ROOT)} | sysinfo={os.path.relpath(sysinfo_path, ROOT)}")

    bench=load_json(bench_path)
    sh=bench.get("shell",{})
    c =bench.get("c",{})

    sysinfo=open(sysinfo_path,"r",encoding="utf-8",errors="ignore").read()
    print("\n== SYSINFO (recortes) ==")
    for line in pick_sysinfo(sysinfo):
        print(" ", line)

    # shell
    sh_loops = int(sh.get("loops",0) or 0)
    sh_ms    = int(sh.get("duration_ms",0) or 0)
    sh_dur_s = (sh_ms/1000.0) if sh_ms>0 else None
    loops_ps = (sh_loops/sh_dur_s) if (sh_dur_s and sh_dur_s>0) else None

    print("\n== BENCH: SHELL ==")
    print(f" loops              : {sh_loops}")
    print(f" duration_ms        : {sh_ms}  {'(BUG: 0 no JSON?)' if sh_ms==0 else ''}")
    print(f" loops_per_sec      : {0.0 if not loops_ps else loops_ps:.3f}")
    print(f" chars_per_sec      : {sh.get('chars_per_sec','NA')}")
    print(f" archetypes_per_sec : {sh.get('archetypes_per_sec','NA')}")

    # c
    iters = int(c.get("iters",0) or 0)
    dns   = int(c.get("duration_ns",0) or 0)
    c_dur_s = dns/1e9 if dns>0 else None
    itps = (iters/c_dur_s) if (c_dur_s and c_dur_s>0) else None

    print("\n== BENCH: C ==")
    print(f" iters              : {iters}")
    print(f" duration_s         : {0.0 if not c_dur_s else c_dur_s:.6f}")
    print(f" iters_per_sec      : {0 if not itps else int(itps)}")

    print("\n== DIAGNÓSTICO ==")
    print(" shell ≈ spawn/IO/utilitários; C ≈ CPU/loop tight.")
    if sh_ms==0:
        print(" ⚠️ JSON do shell está zerando métricas. Corrija export no suite (salvar duration_ms/chars/archetypes).")
    print(" próximo: medir memory-bound (stride) + faults/swap durante TOROIDAL_SOC.")
if __name__=="__main__":
    main()
PY
chmod +x "${ROOT}/rafaelia_bench_analyze.py"

echo "[3/3] Rodando cache probe e analyzer..."
"${ROOT}/rafaelia_probe_cache.sh" | tee "${OUT}/cache_probe_fixpack.txt" >/dev/null
python3 "${ROOT}/rafaelia_bench_analyze.py" | tee "${OUT}/bench_analyze_fixpack.txt" >/dev/null

echo
echo "[OK] Gerados:"
echo " - ${OUT}/cache_probe_fixpack.txt"
echo " - ${OUT}/bench_analyze_fixpack.txt"
echo
echo "Agora: revise seu rafaelia_bench_suite.sh -> garantir que escreve no bench.json:"
echo "  shell.duration_ms / chars_per_sec / archetypes_per_sec (nunca 0)."
