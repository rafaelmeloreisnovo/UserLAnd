#!/data/data/com.termux/files/usr/bin/bash
# Patch: garantir loops no bench.json

OUTDIR="$HOME/out"
BENCH="$OUTDIR/bench.json"
mkdir -p "$OUTDIR"

# supondo que você tenha:
# duration_ms, chars_per_sec, archetypes_per_sec, loops
# GARANTA que loops seja o número real (ex.: LOOPS)

LOOPS="${LOOPS:-100}"           # <- aqui entra teu valor real do run
DURATION_MS="${DURATION_MS:-0}"
CHARS_PER_SEC="${CHARS_PER_SEC:-0}"
ARCH_PER_SEC="${ARCH_PER_SEC:-0}"

# preserva bloco C se já existir
C_BLOCK="$(python3 - <<PY 2>/dev/null
import json,sys,os
p="$BENCH"
if os.path.exists(p):
  j=json.load(open(p))
  print(json.dumps(j.get("c",{})))
else:
  print("{}")
PY
)"

python3 - <<PY
import json, os
bench_path="$BENCH"
c=json.loads('''$C_BLOCK''') if '''$C_BLOCK'''.strip() else {}
j={
  "shell":{
    "loops": int("$LOOPS"),
    "duration_ms": int("$DURATION_MS"),
    "chars_per_sec": int("$CHARS_PER_SEC"),
    "archetypes_per_sec": int("$ARCH_PER_SEC")
  },
  "c": c
}
os.makedirs(os.path.dirname(bench_path), exist_ok=True)
with open(bench_path,"w",encoding="utf-8") as f:
  json.dump(j,f,indent=2,ensure_ascii=False)
print("[OK] wrote", bench_path)
PY
