set -eu

F="${1:-}"
if [ -z "$F" ]; then
  echo "uso: sh raf_prove.sh <arquivo>"
  exit 2
fi
if [ ! -f "$F" ]; then
  echo "arquivo nao existe: $F"
  exit 2
fi

mkdir -p raf_provas
OUT="raf_provas/$(basename "$F").report.txt"

echo "== RAFAELIA PROVA ==" | tee "$OUT"
echo "file: $F" | tee -a "$OUT"
echo "size: $(wc -c < "$F") bytes" | tee -a "$OUT"
echo | tee -a "$OUT"

# hashes (integridade)
if command -v sha3sum >/dev/null 2>&1; then
  echo "[SHA3-256]" | tee -a "$OUT"
  sha3sum -a 256 "$F" | tee -a "$OUT"
else
  echo "[SHA3-256] sha3sum nao instalado (pkg install coreutils + libdigest?)" | tee -a "$OUT"
fi

if command -v b3sum >/dev/null 2>&1; then
  echo "[BLAKE3]" | tee -a "$OUT"
  b3sum "$F" | tee -a "$OUT"
else
  echo "[BLAKE3] b3sum nao instalado (pkg install b3sum se tiver, ou compilar blake3)" | tee -a "$OUT"
fi

echo | tee -a "$OUT"

# repetição via k-grams simples (python3)
python3 - <<'PY' "$F" | tee -a "$OUT"
import sys
from collections import Counter
path=sys.argv[1]
data=open(path,'rb').read()
print("[REPETICAO k-grams]")
for k in (4,6,8,12,16):
    if len(data)<k:
        print("k=",k,"(arquivo pequeno)")
        continue
    total=len(data)-k+1
    c=Counter(data[i:i+k] for i in range(0,total,1))
    uniq=len(c)
    top=c.most_common(5)
    rep_ratio=1.0-(uniq/total)
    print(f"k={k} total={total} uniq={uniq} rep_ratio={rep_ratio:.6f} top5_counts={[x[1] for x in top]}")
PY

echo | tee -a "$OUT"

# se parece ZIP, testa sem extrair
if command -v zipinfo >/dev/null 2>&1; then
  echo "[ZIPINFO]" | tee -a "$OUT"
  zipinfo -h "$F" 2>/dev/null | head -n 30 | tee -a "$OUT" || echo "zipinfo nao reconheceu" | tee -a "$OUT"
fi

if command -v 7z >/dev/null 2>&1; then
  echo "[7Z TEST]" | tee -a "$OUT"
  7z t "$F" 2>/dev/null | head -n 60 | tee -a "$OUT" || echo "7z nao testou (ou nao e arquivo suportado)" | tee -a "$OUT"
fi

echo
echo "OK -> $OUT"
