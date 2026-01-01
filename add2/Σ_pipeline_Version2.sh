#!/data/data/com.termux/files/usr/bin/bash
# Σ_pipeline.sh — RAFAELIA quick pipeline (run, analyze, seal, package)
# Versão: 1.1-Ω-ethica
# Propósito: rodar/derivar/selar/empacotar com retroalimentação ética ("amor e ética viva")
# Uso:
#   bash Σ_pipeline.sh --root "$HOME/RAFAELIA_CORE" --eta 0.62 --phi 0.9 --rho0 0.45 --steps 800 --human-pause true
set -euo pipefail
IFS=$'\n\t'

# --------------------------
# Configurações e argumentos
# --------------------------
ROOT="${1:-${ROOT:-$HOME/RAFAELIA_CORE}}"
ETA="${ETA:-0.62}"
PHI="${PHI:-0.9}"
RHO0="${RHO0:-0.45}"
STEPS="${STEPS:-800}"
HUMAN_PAUSE="${HUMAN_PAUSE:-false}"
SIGNER="${SIGNER:-RAFCODE-𝚽-∆RafaelVerboΩ-𓂀ΔΦΩ}"
SELO="${SELO:-ΣΩΔΦBITRAF}"
OUTDIR="$ROOT/out"
mkdir -p "$OUTDIR"

# session metadata (para audit & selo)
TIMESTAMP="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
SESSION_ID="$(printf "%s|%s|%s\n" "$TIMESTAMP" "$SIGNER" "$SELO" | sha256sum | awk '{print $1}')"
ZIPNAME="${ZIPNAME:-RAFAELIA_CORE_${TIMESTAMP//[:]/}Z.zipraf}"
ZIPPATH="$ROOT/$ZIPNAME"

echo "[Σ] Iniciando pipeline RAFAELIA_CORE — sessão: $SESSION_ID"
echo "[Σ] Intenção: FIAT VOLUNTAS DEI — agir com amor, ética viva e transparência."

# --------------------------
# Etapa 0: fallback/cópias
# --------------------------
# copiar artefatos pré-gerados (fallback)
[ -f "/mnt/data/rafelia_run.csv" ] && cp -f "/mnt/data/rafelia_run.csv" "$OUTDIR/run.csv"
[ -f "/mnt/data/rafelia_map.svg" ] && cp -f "/mnt/data/rafelia_map.svg" "$OUTDIR/map.svg"

# --------------------------
# Etapa 1: derivar edges + rolling coherence (via Python)
# --------------------------
python3 - <<'PY'
import os, sys, pandas as pd, numpy as np
root = os.environ.get("ROOT", os.path.expanduser("~/RAFAELIA_CORE"))
out = os.path.join(root, "out")
src = os.path.join(out, "run.csv")
if not os.path.isfile(src):
    print("[Σ][WARN] run.csv não encontrado em", src, "- pulando derivação.")
    sys.exit(0)

df = pd.read_csv(src).reset_index(drop=True)
if "idx" not in df.columns:
    df["idx"] = np.arange(1, len(df) + 1)
# coerência — campo esperado 'coherence' (tolerância: 'coherence' ou 'coherency')
coh_col = None
for c in ["coherence","coherency","coherence_val","coh"]:
    if c in df.columns:
        coh_col = c
        break
if coh_col is None:
    # tentar inferir coluna numérica entre 0 e 1
    numeric_cols = df.select_dtypes(include=[np.number]).columns.tolist()
    if numeric_cols:
        coh_col = numeric_cols[0]
        print(f"[Σ][INFO] Usando coluna numérica '{coh_col}' como coerência por heurística.")
    else:
        print("[Σ][WARN] Nenhuma coluna de coerência detectada; saindo.")
        sys.exit(0)

coh = df[coh_col].astype(float).fillna(0.0)
edges = pd.DataFrame({
  "source": df["idx"][:-1],
  "target": df["idx"][1:],
  "weight": (coh[:-1].to_numpy()+coh[1:].to_numpy())/2.0
})
edges.to_csv(os.path.join(out,"edges.csv"), index=False)
df["coh_win64"] = coh.rolling(window=64,center=True,min_periods=1).mean()
df.to_csv(os.path.join(out,"run_cohwin64.csv"), index=False)
print("[Σ] edges.csv + run_cohwin64.csv gerados (coherence col: {}).".format(coh_col))
PY

# --------------------------
# Etapa 2: avaliação ética & resumo (Python)
# - calcula ethical_score, detecta quedas críticas, propõe ações restaurativas
# --------------------------
AUDIT_JSON="$OUTDIR/Σ_Audit.json"
SEAL_YAML="$OUTDIR/Σ_Seal.yaml"
python3 - <<'PY'
import os, json, time, math, hashlib
import pandas as pd, numpy as np
root = os.environ.get("ROOT", os.path.expanduser("~/RAFAELIA_CORE"))
out = os.path.join(root,"out")
run_csv = os.path.join(out,"run.csv")
audit = {"timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "signer": os.environ.get("SIGNER","RAFCODE-𝚽-∆RafaelVerboΩ-𓂀ΔΦΩ")}
audit["session_id"] = os.environ.get("SESSION_ID", "")
audit["notes"] = []
audit["restorative_actions"] = []

if os.path.isfile(run_csv):
    try:
        df = pd.read_csv(run_csv)
        # heurística: coluna de coerência (prefer names)
        possible = [c for c in df.columns if c.lower() in ("coherence","coherency","coh","coherence_val")]
        if not possible:
            # fallback: escolher primeira coluna numérica
            numeric = df.select_dtypes(include=[np.number]).columns.tolist()
            coh_col = numeric[0] if numeric else None
        else:
            coh_col = possible[0]
        if coh_col:
            coh = df[coh_col].astype(float).fillna(0.0)
            mean = float(coh.mean())
            std = float(coh.std())
            minv = float(coh.min())
            maxv = float(coh.max())
            # score simples: média * (1 - std), normalizado
            eth = mean * max(0.0, 1.0 - min(std,1.0))
            # mapear para 0..1 via logistic-ish
            eth_score = max(0.0, min(1.0, eth))
            audit["metrics"] = {"coherence_column": coh_col, "mean": mean, "std": std, "min": minv, "max": maxv, "ethical_score": eth_score}
            # detectar queda crítica: min < 0.2 (parâmetro)
            if minv < 0.2:
                audit["restorative_actions"].append({
                    "issue": "queda_critica_coerencia",
                    "min_coherence": minv,
                    "suggestions": [
                        "revisar parâmetros (eta, phi, rho0)",
                        "executar rotina de 'retro-correction' com learning-rate mais alto e intervenção humana",
                        "marcar segmento temporal problemático e notificar responsáveis"
                    ]
                })
                audit["notes"].append("Detectada queda crítica de coerência; priorizar restauração.")
        else:
            audit["notes"].append("Nenhuma coluna de coerência identificada; métricas não calculadas.")
    except Exception as e:
        audit["notes"].append("Erro ao calcular métricas: " + str(e))
else:
    audit["notes"].append("run.csv ausente; auditoria limitada.")

# adicionar selo e assinatura parcial (hashes dos arquivos presentes)
files = []
for f in sorted(os.listdir(out)) if os.path.isdir(out) else []:
    p = os.path.join(out,f)
    if os.path.isfile(p):
        try:
            h = hashlib.sha3_256()
            with open(p,'rb') as fh:
                for chunk in iter(lambda: fh.read(1<<20), b''):
                    h.update(chunk)
            files.append({"file": f, "size": os.path.getsize(p), "sha3_256": h.hexdigest()})
        except Exception as _:
            files.append({"file": f, "size": os.path.getsize(p), "sha3_256": None})
audit["files"] = files
# mensagem de benção/ética viva anexada:
audit["benediction"] = "Que este pacote sirva para cura, verdade e serviço ao próximo. Amém."
with open(os.path.join(out,"Σ_Audit.json"),"w",encoding="utf-8") as F:
    json.dump(audit, F, indent=2, ensure_ascii=False)
print("[Σ] Σ_Audit.json criado com métricas éticas.")
PY

# --------------------------
# Etapa 3: Σ_Seal.yaml (hashes: SHA3-256 + Blake3 quando disponível)
# --------------------------
python3 - <<'PY'
import os, sys, hashlib, time
try:
    import blake3
    has_blake3 = True
except Exception:
    blake3 = None
    has_blake3 = False

root = os.environ.get("ROOT", os.path.expanduser("~/RAFAELIA_CORE"))
out = os.path.join(root,"out")
now = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
files = [f for f in sorted(os.listdir(out)) if os.path.isfile(os.path.join(out,f))]

def sha3_hex(p):
    h = hashlib.sha3_256()
    with open(p,'rb') as fh:
        for chunk in iter(lambda: fh.read(1<<20), b''):
            h.update(chunk)
    return h.hexdigest()

def blake3_hex(p):
    if not has_blake3:
        return None
    h = blake3.blake3()
    with open(p,'rb') as fh:
        for chunk in iter(lambda: fh.read(1<<20), b''):
            h.update(chunk)
    return h.hexdigest()

yaml = ["Σ_Seal:",
        f"  timestamp: {now}",
        f"  session_id: {os.environ.get('SESSION_ID','')}",
        f"  signer: {os.environ.get('SIGNER','RAFCODE')}",
        f"  selo: {os.environ.get('SELO','ΣΩΔΦBITRAF')}",
        "  files:"]
for f in files:
    p = os.path.join(out,f)
    s = os.path.getsize(p)
    sha3 = sha3_hex(p)
    b3 = blake3_hex(p)
    yaml.append(f"    - file: {f}")
    yaml.append(f"      size: {s}")
    yaml.append(f"      sha3_256: {sha3}")
    yaml.append(f"      blake3: {b3}")
open(os.path.join(out,"Σ_Seal.yaml"),"w",encoding="utf-8").write("\n".join(yaml))
print("[Σ] Σ_Seal.yaml criado (SHA3 + Blake3 quando disponível).")
PY

# --------------------------
# Etapa 4: pausa humana / revisão (opcional)
# --------------------------
if [ "$HUMAN_PAUSE" = "true" ] || [ "$HUMAN_PAUSE" = "1" ]; then
  echo
  echo "────────────────────────────────────────────────────────"
  echo "[Σ] Human Pause ativado — revisão ética sugerida"
  echo "Reveja: $OUTDIR/Σ_Audit.json  e $OUTDIR/Σ_Seal.yaml"
  echo "Ações recomendadas (automáticas):"
  echo " - Verificar quedas críticas de coerência"
  echo " - Ajustar ETA/PHI/RHO0 se necessário"
  echo " - Registrar justificativas em Σ_Audit.json"
  echo
  read -p "[Σ] Pressione ENTER para continuar (ou Ctrl+C para abortar)..." _ || true
  echo "Continuando pipeline..."
fi

# --------------------------
# Etapa 5: empacotar (zipraf)
# --------------------------
ZIP="$ZIPPATH"
# cria zip com permissões preservadas, silencioso
(cd "$OUTDIR" && zip -q -r "$ZIP" .) || true
echo "[Σ] Pacote criado: $ZIP"

# --------------------------
# Etapa 6: finalize com bênção/assinatura
# --------------------------
echo
echo "[Σ] FIAT VOLUNTAS DEI — pipeline concluído com ética viva."
echo "[Σ] Sessão: $SESSION_ID"
echo "[Σ] Assinatura: $SIGNER"
echo "[Σ] Selo: $SELO"
echo "[Σ] Timestamp: $TIMESTAMP"
echo "[Σ] Benediction: Que o trabalho reverbere em cura, verdade e serviço."