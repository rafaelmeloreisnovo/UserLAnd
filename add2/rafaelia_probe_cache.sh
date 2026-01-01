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
