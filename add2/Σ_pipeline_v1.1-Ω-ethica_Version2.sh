#!/usr/bin/env bash
# Σ_pipeline_v1.1‑Ω‑ethica_Version2.sh
# Pipeline esqueleto para integrar WAVs, validar hashes e empacotar Bitraf64 (shape 10x10x12)
set -euo pipefail

# Configurações
ASSETS_DIR="./assets"
OUTPUT_DIR="./build"
MANIFEST="./Σ_PIPELINE_MANIFEST.md"
BITRAF_SCRIPT="./integrate_bitraf64.py"
SHAPE_X=10
SHAPE_Y=10
SHAPE_Z=12
ZIPRAF_NAME="RAFAELIA_CORE_$(date -u +%Y%m%dT%H%M%SZ).zipraf"

# Dependências: ffmpeg, python3, sha3sum / openssl, blake3 (cli or python)
command -v ffmpeg >/dev/null 2>&1 || { echo "Instale ffmpeg"; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "Instale python3"; exit 1; }

mkdir -p "$OUTPUT_DIR"

echo "1) Validando e normalizando WAVs em $ASSETS_DIR..."
for f in "$ASSETS_DIR"/*.wav; do
  [ -e "$f" ] || continue
  echo "  - Processando $(basename "$f")"
  # Normalizar para 48kHz, 24-bit PCM, stereo
  ffmpeg -y -i "$f" -ar 48000 -ac 2 -sample_fmt s32 "${OUTPUT_DIR}/$(basename "$f")" < /dev/null >/dev/null 2>&1 || {
    echo "    erro ao converter $f"
    exit 1
  }
done

echo "2) Gerando hashes e metadados..."
python3 "$BITRAF_SCRIPT" --scan-dir "$OUTPUT_DIR" --out "$OUTPUT_DIR/bitraf_payload.npz" --shape ${SHAPE_X} ${SHAPE_Y} ${SHAPE_Z}

echo "3) Empacotando ZIPRAF..."
pushd "$OUTPUT_DIR" >/dev/null
zip -r "../${ZIPRAF_NAME}" . >/dev/null
popd >/dev/null

echo "4) Gerando selos (SHA3 / Blake3) para ${ZIPRAF_NAME}..."
# SHA3-256
python3 - <<PY
import hashlib,sys
fn="../${ZIPRAF_NAME}"
h = hashlib.sha3_256()
with open(fn,"rb") as fh:
    while True:
        b = fh.read(8192)
        if not b: break
        h.update(b)
print("SHA3-256:", h.hexdigest())
PY

echo "Pipeline concluído. Artefato: ${ZIPRAF_NAME}"