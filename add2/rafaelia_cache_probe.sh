#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

mkdir -p out

echo "== CACHE PROBE =="

echo "-- sysfs --"
# zsh nullglob não vale aqui; em bash usamos nullglob
shopt -s nullglob || true

for cpu in /sys/devices/system/cpu/cpu*; do
  [ -d "$cpu/cache" ] || continue
  echo "## ${cpu##*/}"
  for i in "$cpu"/cache/index*; do
    [ -d "$i" ] || continue
    lvl=$(cat "$i/level" 2>/dev/null || echo "?")
    typ=$(cat "$i/type"  2>/dev/null || echo "?")
    siz=$(cat "$i/size"  2>/dev/null || echo "?")
    cls=$(cat "$i/coherency_line_size" 2>/dev/null || echo "?")
    shr=$(cat "$i/shared_cpu_list" 2>/dev/null || echo "?")
    echo " L$lvl $typ size=$siz line=$cls shared=$shr"
  done
done | tee out/cache_sysfs.txt

echo
echo "-- lscpu (se disponível) --"
if command -v lscpu >/dev/null; then
  lscpu | grep -E 'Architecture|CPU\(s\)|On-line|Model name|Thread|Core|Socket|cache' | tee out/lscpu.txt
else
  echo "lscpu não disponível" | tee out/lscpu.txt
fi

echo
echo "[OK] out/cache_sysfs.txt out/lscpu.txt"
