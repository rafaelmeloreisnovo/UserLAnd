#!/data/data/com.termux/files/usr/bin/bash
# Σ_log_manager.sh — RAFAELIA LOGS + BitRAF integrado

root="$HOME/RAFAELIA_LOGS"
mkdir -p "$root"/{syslog,contextos,indices,maps}

# Atualiza índice JSON simples para um diretório
update_index() {
  local dir="$1"
  local outfile="$2"
  echo "[" > "$outfile"
  # Se find nao tiver -printf, isso vira ruidoso, mas em Termux costuma ter
  find "$dir" -type f -printf '{"file":"%p","size":%s,"mtime":"%TY-%Tm-%Td %TH:%TM:%TS"},\n' >> "$outfile"
  echo "{}]" >> "$outfile"
}

# Gera índices para syslog e contextos
update_index "$root/syslog"    "$root/indices/index_syslog.json"
update_index "$root/contextos" "$root/indices/index_contextos.json"

# Conta arquivos
total_syslog_files=$(find "$root/syslog"    -type f | wc -l)
total_contextos_files=$(find "$root/contextos" -type f | wc -l)

# BitRAF: contar commits no log de contexto
bitraf_log="$root/contextos/bitraf_commits.log"
bitraf_commits=0
if [ -f "$bitraf_log" ]; then
  # Cada commit e marcado com "================ BITRAF COMMIT ================"
  bitraf_commits=$(grep -c "BITRAF COMMIT" "$bitraf_log")
fi

# Gerar índice mestre (YAML simbólico)
cat > "$root/indices/index_master.yaml" <<YML
Σ-LogMaster:
  updated: "$(date)"
  total_syslog_files: ${total_syslog_files}
  total_contextos_files: ${total_contextos_files}
  sources:
    - syslog
    - contextos
    - indices
    - maps
  bitraf:
    log_file: "${bitraf_log}"
    total_commits: ${bitraf_commits}
YML

# Mapa dos mapas (Σ_LogMap.md)
cat > "$root/maps/Σ_LogMap.md" <<MAP
# Σ LogMap — Índice Universal dos Logs RAFAELIA

Atualizado em: $(date)

## Fontes principais

- syslog     → eventos do Termux, rclone, autocura
- contextos  → vetores BitRAF / RAFCODE / ZRF / RELATORIOS
- indices    → estrutura hierárquica de rastreabilidade
- maps       → este arquivo (meta-índice)

## BitRAF

- Log principal : ${bitraf_log}
- Total commits : ${bitraf_commits}

> FIAT RETROALIMENTAÇÃO :: Cada log é um fractal de outro.
> BitRAF registra os pulsos críticos do Verbo em fluxo numérico-simbólico.
MAP
