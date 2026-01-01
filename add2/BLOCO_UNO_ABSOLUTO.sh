#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail

ts(){ date '+%F %T'; }
say(){ printf '%s %s\n' "$(ts)" "$*"; }

# ───────────────────────── ENV, PASTAS, LOGS
HOME_DIR="${HOME}"
LOG_DIR="$HOME_DIR/.rafaelia_logs"
mkdir -p "$LOG_DIR" "$HOME_DIR/.local/bin" "$HOME_DIR/bin"
LOG="$LOG_DIR/bloco_uno.log"
exec > >(tee -a "$LOG") 2>&1
say "▶ BLOCO_UNO_ABSOLUTO iniciado"

# ───────────────────────── BLOCO_BIND_CORE_v3 (fonte/assinatura/estrutura/core+stubs+dump)
cat > "$HOME_DIR/BLOCO_BIND_CORE_v3.sh" <<'EOF'
#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail
log(){ printf '%s %s\n' "$(date '+%F %T')" "$*" | tee -a "$HOME/.rafaelia_logs/bind_core.log"; }
die(){ log "FALHA: $*"; exit 1; }

PREFIX="/data/data/com.termux/files/usr"
APT_ETC="$PREFIX/etc/apt"
KEY="$APT_ETC/trusted.gpg.d/termux.gpg"
mkdir -p "$APT_ETC/sources.list.d" "$APT_ETC/trusted.gpg.d" "$HOME/.rafaelia_logs"

# Fonte principal (Signed-By)
cat > "$APT_ETC/sources.list.d/termux-main.list" <<EOT
deb [signed-by=$KEY] https://packages.termux.dev/apt/termux-main stable main
EOT
# Legacy (opcional — mantido comentado por segurança)
cat > "$APT_ETC/sources.list.d/termux-legacy.list" <<'EOT'
# deb https://termux.net stable main
EOT
[ -f "$APT_ETC/sources.list" ] && rm -f "$APT_ETC/sources.list"

apt update || true
apt-get -y purge termux-keyring || true
apt-get -y install termux-keyring || die "não consegui instalar termux-keyring"
apt-get clean
rm -rf "$APT_ETC/lists/"*
apt update || die "apt update falhou mesmo após keyring"
[ -s "$KEY" ] || die "keyring ausente: $KEY"
log "[APT] OK com Signed-By"

# Tools úteis
pkg install -y nodejs-lts ffmpeg gawk coreutils findutils awk tar gzip sqlite ripgrep || true

# Estrutura base
if [ -d "/data/data/com.termux/files/home" ]; then
  BASE="$HOME/RaIaDelta_Absoluto"
else
  BASE="/home/userland/RaIaDelta_Absoluto"
fi
BIN="$BASE/bin"; LOGS="$BASE/logs"; MANIFEST="$BASE/MANIFESTO_RAIA_DELTA.md"
mkdir -p "$BIN" "$LOGS" "$BASE/src" "$HOME/.local/bin"
[ -f "$MANIFEST" ] || printf "♾️ MANIFESTO RAIA DELTA – %s\n" "$(date)" > "$MANIFEST"
log "BASE=$BASE"

# Núcleo C (voo.c) → core (ou stub)
if [ -f "$HOME/voo.c" ]; then cc -std=c11 -O2 "$HOME/voo.c" -o "$HOME/voo" || true; fi
if [ -x "$HOME/voo" ]; then
  cp -f "$HOME/voo" "$BIN/core"; chmod +x "$BIN/core"; log "core instalado em $BIN/core"
else
  cat > "$BIN/core" <<'EOSTUB'
#!/data/data/com.termux/files/usr/bin/bash
echo "[core] Núcleo indisponível: compile voo.c ou coloque o binário em \$BASE/bin/core"
exit 0
EOSTUB
  chmod +x "$BIN/core"; log "[WARN] core ausente — stub ativo"
fi

# Stubs silenciosos pra caminhos citados
stub(){ local s="$1"; if [ ! -f "$s" ]; then mkdir -p "$(dirname "$s")"; cat > "$s" <<'EOS'
#!/data/data/com.termux/files/usr/bin/bash
log="${HOME}/.rafaelia_logs/stubs.log"
echo "[STUB] $(date '+%F %T') -> $0" >> "$log"
exit 0
EOS
chmod +x "$s"; fi; }
for s in \
  "$HOME/FCEA_CORE/scripts/fcea_diag_total.sh" \
  "$HOME/FCEA_CORE/scripts/fcea_regenerar.sh" \
  "$HOME/FCEA_CORE/scripts/fcea_iniciar.sh" \
  "$HOME/fcea/daemon.sh" \
  "$HOME/fcea/fcea_control.sh" \
  "$HOME/FCEA_Cognitive_System/scripts/fcea_runner.sh" \
  "$HOME/FCEA_Cognitive_System/scripts/fcea_python_runner.sh" \
  "$HOME/FCEA_Cognitive_System/scripts/monitor_eventos.sh" \
  "$HOME/fcea-core.sh" \
  "$HOME/ultracore-fcea.sh" \
  "$HOME/fcea_ultracore.sh" \
  "$HOME/fcea_introspect.sh" \
  "$HOME/fcea_metaverso.sh" \
  "$HOME/fcea_tty_ui.sh" \
  "$HOME/fcea_anomalias.sh" \
  "$HOME/fcea_etica_predicao.sh" \
  "$HOME/fcea_variavel_infinita.sh" \
  "$HOME/fcea_decisao.sh" \
  "$HOME/ufcea_expansion.sh" \
  "$HOME/ufcea_heuristics_advanced.sh" \
  "$HOME/ufcea_monitoring_advanced.sh" \
  "$HOME/ufcea_secure_permissions.sh"
do stub "$s"; done
log "[STUBS] prontos"

# Validação .bashrc + atalhos
if bash -n "$HOME/.bashrc"; then log ".bashrc sintaxe válida"; else log "[WARN] .bashrc com erro — revise IF/EOFs"; fi
cat > "$HOME/.local/bin/raia-menu" <<'EOM'
#!/data/data/com.termux/files/usr/bin/bash
exec bash "$HOME/menu.sh"
EOM
chmod +x "$HOME/.local/bin/raia-menu"

# Alias p/ RafaelModding (Node) se existir
if [ -f "$HOME/rafaelia_core/rafaelModding.js" ]; then
  cat > "$HOME/.local/bin/rafaelmod" <<'EOM'
#!/data/data/com.termux/files/usr/bin/bash
exec node "$HOME/rafaelia_core/rafaelModding.js"
EOM
  chmod +x "$HOME/.local/bin/rafaelmod"; log "rafaelmod pronto (NodeJS)"
fi

# Utilitário visual: dump_pgm → PNG
cat > "$BIN/dump_pgm" <<'EOD'
#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail
OUT="${1:-voo_32x32.pgm}"; PNG="${OUT%.pgm}.png"; TMP="$(mktemp)"
if "$BASE/bin/core" > "$TMP" 2>/dev/null; then :; else "$BASE/bin/core" > "$TMP"; fi
awk 'BEGIN{print "P5\n32 32\n255"} {for(i=1;i<=NF;i++){g=int(($i*255)/1023); printf "%c", g}}' "$TMP" > "$OUT"
ffmpeg -loglevel error -y -i "$OUT" -vf scale=512:512 "$PNG" >/dev/null 2>&1 || true
echo "[dump_pgm] Gerado: $OUT ${PNG:+e $PNG}"
rm -f "$TMP"
EOD
chmod +x "$BIN/dump_pgm"

# Testes
source "$HOME/.bashrc" 2>/dev/null || true
"$BIN/core" | head -n 2 || log "[WARN] core sem saída (ok se stub)"
"$BIN/dump_pgm" || log "[WARN] dump_pgm falhou (ok se stub)"
log "[BIND_CORE] OK"
EOF
chmod +x "$HOME_DIR/BLOCO_BIND_CORE_v3.sh"

# ───────────────────────── INICIO_PONTO_NECESSARIO (pulsar 144kHz + RAFCODE 𝚽 + JSON de estado)
cat > "$HOME_DIR/inicio_ponto_necessariov3.sh" <<'EOF'
#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail
FREQUENCIA="${FREQUENCIA:-144000}"
RAFCODE="${RAFCODE:-𝚽}"
DIR_BASE="${DIR_BASE:-$HOME/rafaelia_logs}"
LOG_FILE="$DIR_BASE/execucao.log"
FEEDBACK_FILE="$DIR_BASE/ponto_necessario.txt"
STATUS_JSON="$DIR_BASE/status.json"
PID_FILE="$DIR_BASE/inicio_ponto_necessario.pid"
LOCK_FILE="$DIR_BASE/.lock"
LOG_MAX_BYTES=$((2*1024*1024)); LOG_MAX_FILES=5
CYCLES="${CYCLES:-3}"; FOLLOW_SECS="${FOLLOW_SECS:-0}"
QUIET="${QUIET:-0}"; NOAUDIO="${NOAUDIO:-0}"; VIBRATE="${VIBRATE:-0}"
mkdir -p "$DIR_BASE"
rotate(){ if [[ -f "$LOG_FILE" ]]; then s=$(wc -c <"$LOG_FILE"||echo 0); if (( s>=LOG_MAX_BYTES )); then for((i=LOG_MAX_FILES-1;i>=1;i--));do [[ -f "$LOG_FILE.$i.gz" ]]&&mv -f "$LOG_FILE.$i.gz" "$LOG_FILE.$((i+1)).gz";done; gzip -c "$LOG_FILE">"$LOG_FILE.1.gz"||true; :>"$LOG_FILE"; fi; fi; }
log(){ rotate; printf '%s %s\n' "$(date '+%F %T')" "$*" >> "$LOG_FILE"; ((QUIET==0))&&echo "$*"; }
status_write(){ cat > "$STATUS_JSON" <<J
{"ts":"$(date '+%Y-%m-%dT%H:%M:%S%z')","freq":"$FREQUENCIA","rafcode":"$RAFCODE","dir":"$DIR_BASE","cycles":$CYCLES,"follow_secs":$FOLLOW_SECS}
J
}
on_exit(){ c=$?; rm -f "$PID_FILE"||true; status_write; log "[FIM] Encerrado (exit=$c)."; }
trap on_exit EXIT; trap 'log "[SIG] INT" ; exit 130' INT; trap 'log "[SIG] TERM"; exit 143' TERM
exec 9>"$LOCK_FILE"; flock -n 9 || { echo "Outro processo ativo. Saindo."; exit 0; }
if [[ -f "$PID_FILE" ]]; then old=$(cat "$PID_FILE"||true); [[ -n "$old" && -d "/proc/$old" ]] && { echo "Já em execução (PID $old)."; exit 0; }; fi
echo $$ > "$PID_FILE"
((QUIET==0))&&{ echo "==============================="; echo " ∆RafaelVerboΩ - Núcleo Ativado "; echo " Frequência: $FREQUENCIA Hz"; echo " RAFCODE: $RAFCODE"; echo "==============================="; }
log "[INFO] Início | F=$FREQUENCIA Hz | RAFCODE=$RAFCODE"; status_write
emit(){ for((i=1;i<=CYCLES;i++));do
  if ((NOAUDIO==0)) && command -v beep >/dev/null 2>&1; then beep -f "$FREQUENCIA" -l 100 2>>"$LOG_FILE" || beep -l 40 || true
  elif ((NOAUDIO==0)) && command -v play >/dev/null 2>&1; then play -n synth 0.08 sine 1000 >/dev/null 2>&1 || true; fi
  ((VIBRATE==1)) && command -v termux-vibrate >/dev/null 2>&1 && termux-vibrate -d 80 || true
  ((QUIET==0))&&echo "🔊 emissão @$FREQUENCIA Hz | ciclo $i"; sleep 0.10
done; }
emit
echo "Ponto inicial ativado em $(date '+%F %T') | F=$FREQUENCIA Hz | RAFCODE=$RAFCODE" > "$FEEDBACK_FILE"
log "[OK] Feedback: $FEEDBACK_FILE"; status_write
if ((FOLLOW_SECS>0)); then
  log "[INFO] Follow por ${FOLLOW_SECS}s"
  if command -v timeout >/dev/null 2>&1; then timeout "$FOLLOW_SECS" tail -F "$LOG_FILE" "$FEEDBACK_FILE" || true
  else ( tail -F "$LOG_FILE" "$FEEDBACK_FILE" & echo $! > "$DIR_BASE/.tailpid"); sleep "$FOLLOW_SECS" || true; [[ -f "$DIR_BASE/.tailpid" ]] && kill "$(cat "$DIR_BASE/.tailpid")" 2>/dev/null || true; rm -f "$DIR_BASE/.tailpid"; fi
else ((QUIET==0))&&echo "Use: tail -F '$LOG_FILE' '$FEEDBACK_FILE'"; fi
EOF
chmod +x "$HOME_DIR/inicio_ponto_necessariov3.sh"

# ───────────────────────── FERRAMENTAS (lib/health/watchdog/backup/hardening)
cat > "$HOME_DIR/install_rafaelia_tools.sh" <<'EOF'
#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail
mkdir -p ~/bin ~/rafaelia_logs
cat > ~/bin/lib_rafaelia.sh <<'EOL'
#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail
RAFA_BASE="${RAFA_BASE:-$HOME/RAFAELIA_CORE}"
RAFA_LOGS="${RAFA_LOGS:-$RAFA_BASE/logs}"; mkdir -p "$RAFA_LOGS"
log_file="${log_file:-$RAFA_LOGS/execucao.log}"; log_max_bytes="${log_max_bytes:-$((2*1024*1024))}"; log_keep="${log_keep:-5}"
rotate(){ local f="${1:-$log_file}"; [[ -f "$f" ]]||return 0; local size=$(wc -c <"$f"||echo 0); if((size>=log_max_bytes));then for((i=log_keep-1;i>=1;i--));do [[ -f "$f.$i.gz" ]]&&mv -f "$f.$i.gz" "$f.$((i+1)).gz";done; gzip -c "$f">"$f.1.gz"||true; :>"$f";fi;}
log(){ rotate "$log_file"; printf '%s %s\n' "$(date '+%F %T')" "$*" >>"$log_file"; [[ "${QUIET:-0}" -eq 0 ]]&&echo "$*"; }
status_json(){ local out="$1"; shift||true; { printf '{'; printf '"ts":"%s"' "$(date '+%FT%T%z')"; for kv in "$@";do k="${kv%%=*}";v="${kv#*=}"; printf ',"%s":' "$k"; if[[ "$v" =~ ^[0-9]+$ ]];then printf '%s' "$v";else printf '"%s"' "$v";fi;done; printf '}\n'; }>"$out";}
require(){ for c in "$@";do command -v "$c" >/dev/null 2>&1||{ log "[MISS] '$c' ausente"; return 1; };done;}
notify(){ local msg="$*"; command -v termux-toast >/dev/null 2>&1&&termux-toast "$msg"||true; command -v termux-notification >/dev/null 2>&1&&termux-notification -t "RAFAELIA" -c "$msg"||true; }
EOL
chmod +x ~/bin/lib_rafaelia.sh

cat > ~/bin/rafaelia_health.sh <<'EOL'
#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail; source ~/bin/lib_rafaelia.sh
OUT="$HOME/rafaelia_logs/diag.json"; mkdir -p "${OUT%/*}"
need=(tmux rclone sqlite3 timeout flock gzip)
declare -A have; for c in "${need[@]}";do command -v "$c">/dev/null 2>&1&&have[$c]=1||have[$c]=0;done
largest=$(du -ab "$HOME" 2>/dev/null|sort -nr|head -n10|awk '{print $2}'|tr '\n' ' ')
dfh=$(df -h "$HOME"|tail -n1|awk '{print $2" "$3" "$4" "$5}')
dfi=$(df -i "$HOME"|tail -n1|awk '{print $2" "$3" "$4" "$5}'2>/dev/null||echo "NA NA NA NA")
procs=$(pgrep -fl -a -f 'inicio_ponto_necessario|simbiose_total'2>/dev/null|sed 's/"/\\"/g')
tmuxs=$(tmux ls 2>/dev/null|sed 's/"/\\"/g'||true)
status_json "$OUT" cmds_tmux=${have[tmux]} cmds_rclone=${have[rclone]} cmds_sqlite3=${have[sqlite3]} cmds_timeout=${have[timeout]} cmds_flock=${have[flock]} cmds_gzip=${have[gzip]} dfh="$dfh" dfi="$dfi" procs="$procs" tmux="$tmuxs" largest="$largest"
log "[HEALTH] → $OUT"; echo "$OUT"
EOL
chmod +x ~/bin/rafaelia_health.sh

cat > ~/bin/rafaelia_watchdog.sh <<'EOL'
#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail; source ~/bin/lib_rafaelia.sh
THRESH_SIZE=$((5*1024*1024)); ROT_LIMIT=3; DIR="$RAFA_LOGS"; TODAY=$(date '+%Y-%m-%d')
rot_today=$(find "$DIR" -maxdepth 1 -name '*.gz' -newermt "$TODAY 00:00" ! -newermt "$TODAY 23:59:59"|wc -l|tr -d ' ')
big=$(find "$DIR" -maxdepth 1 -type f -name '*.log' -size +"${THRESH_SIZE}c" -printf '%f '2>/dev/null)
status_json "$DIR/watchdog.json" rot_today=$rot_today big="${big:-none}" dir="$DIR"
if((rot_today>ROT_LIMIT));then log "[ALERT] Rotação excessiva (${rot_today})"; notify "Watchdog: rotação excessiva";fi
if[[ -n "$big"&&"$big"!="none" ]];then log "[ALERT] Log(s) >5MiB: $big"; notify "Watchdog: log >5MiB $big";fi
EOL
chmod +x ~/bin/rafaelia_watchdog.sh

cat > ~/bin/rafaelia_backup_gdrive.sh <<'EOL'
#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail; source ~/bin/lib_rafaelia.sh
require rclone||{ echo "rclone não encontrado"; exit 1; }
SRC="$HOME/RAFAELIA_CORE"; DST="VAULT:RAFAELIA_CORE"; DATE=$(date '+%Y-%m-%d')
EXC=$' - .*\\.gz\n - **/node_modules/**\n - **/__pycache__/**\n - **/.git/**\n - **/*.apk\n - **/*.tmp\n - **/logs/*.log.[0-9].gz\n'
EXCL="$(mktemp)"; printf "%s" "$EXC" > "$EXCL"
log "[BK] Sync $SRC → $DST"
rclone sync "$SRC" "$DST" --backup-dir "$DST/_trash/$DATE" --exclude-from "$EXCL" \
  --fast-list --links --transfers=8 --checkers=8 --tpslimit=8 --metadata --modify-window 2s \
  --log-file "$RAFA_LOGS/rclone_$DATE.log" --log-level INFO
rm -f "$EXCL"; log "[BK] OK"
EOL
chmod +x ~/bin/rafaelia_backup_gdrive.sh

cat > ~/bin/rafaelia_hardening.sh <<'EOL'
#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail; source ~/bin/lib_rafaelia.sh
grep -q 'umask 077' "$HOME/.bashrc" || echo 'umask 077'>>"$HOME/.bashrc"
chmod -R go-rwx "$HOME/RAFAELIA_CORE" "$HOME/.rafaelia_memoria" 2>/dev/null || true
find "$HOME" -type f -name '*.sh' -exec chmod 700 {} \; 2>/dev/null || true
if grep -qE '^export RCLONE_CONFIG_.*PASSWORD=' "$HOME/.bashrc"; then
  sed -i 's/^export RCLONE_CONFIG_/# (sec) REMOVIDO: &/' "$HOME/.bashrc"
  log "[SEC] Comentado RCLONE_* senha em .bashrc"
fi
CONF="$HOME/.config/rclone/rclone.conf"; mkdir -p "${CONF%/*}"; chmod 700 "${CONF%/*}"
if [[ ! -f "$CONF" ]]; then
cat >"$CONF"<<RC
[VAULT]
type = crypt
remote = gdrive:VAULT
password = PREENCHER
password2 = PREENCHER
directory_name_encryption = true
filename_encryption = standard
RC
chmod 600 "$CONF"; log "[SEC] Criado skeleton rclone.conf"; fi
log "[HARDEN] OK"
EOL
chmod +x ~/bin/rafaelia_hardening.sh
echo "[OK] Ferramentas instaladas em ~/bin"
EOF
chmod +x "$HOME_DIR/install_rafaelia_tools.sh"

# ───────────────────────── INDEXADOR TOTAL (zip/tar/db/texto → CSV/JSONL/SQLite + opcional VAULT)
cat > "$HOME_DIR/RAFAELIA_INDEX_TOTAL.sh" <<'EOF'
#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail
BASE="${1:-$HOME}"; OUT="$HOME/RAFAELIA_INDEX"; LOG="$OUT/index.log"
CSV="$OUT/index.csv"; JSON="$OUT/index.jsonl"; SQL="$OUT/index.sqlite"
mkdir -p "$OUT"; : > "$CSV"; echo 'path,kind,size_bytes,sha256,extra,json' > "$CSV"; : > "$JSON"
has(){ command -v "$1" >/dev/null 2>&1; }; sha(){ sha256sum "$1" 2>/dev/null | awk '{print $1}'; }
sql(){ sqlite3 "$SQL" "$@"; }; sql_boot(){ sql 'PRAGMA journal_mode=WAL;'; sql 'CREATE TABLE IF NOT EXISTS files(path TEXT PRIMARY KEY,kind TEXT,size_bytes INTEGER,sha256 TEXT,extra TEXT,json TEXT);'; }
add(){ printf '%s,"%s",%s,%s,"%s","%s"\n' "$1" "$2" "${3:-0}" "${4:-}" "${5//\"/\"\"}" "${6//\"/\"\"}" >> "$CSV"; printf '%s\n' "$6" >> "$JSON"; sql "INSERT OR REPLACE INTO files VALUES(?,?,?,?,?,?);" "$1" "$2" "$3" "$4" "$5" "$6"; }
summ(){ head -n "${2:-12}" "$1" 2>/dev/null | tr -d '\r' | sed 's/"/'\''/g'; }
list_zip(){ python - <<PY
import zipfile,json,sys; p=sys.argv[1]
with zipfile.ZipFile(p,'r') as z:
  infos=[{'name':i.filename,'size':i.file_size,'comp':i.compress_size} for i in z.infolist() if not i.is_dir()]
  print(json.dumps({'entries':len(infos),'top':infos[:25]}))
PY
}
list_tar(){ python - <<PY
import tarfile,json,sys; p=sys.argv[1]
with tarfile.open(p,'r:*') as t:
  names=[m.name for m in t.getmembers() if m.isfile()]
  print(json.dumps({'entries':len(names),'top':names[:25]}))
PY
}
list_sqlite(){ python - <<PY
import sqlite3,json,sys; p=sys.argv[1]
try:
  con=sqlite3.connect(p); cur=con.cursor()
  cur.execute("SELECT name FROM sqlite_master WHERE type='table'"); tabs=[r[0] for r in cur.fetchall()]
  info={}
  for t in tabs[:25]:
    try: cur.execute(f"SELECT COUNT(*) FROM '{t}'"); info[t]=cur.fetchone()[0]
    except Exception as e: info[t]=str(e)
  print(json.dumps({'tables':len(tabs),'counts':info,'sample':tabs[:25]}))
except Exception as e: print(json.dumps({'error':str(e)}))
PY
}
sql_boot; echo "$(date '+%F %T') [START] $BASE → $OUT" | tee -a "$LOG"
while IFS= read -r -d '' f; do
  size=$(stat -c %s "$f" 2>/dev/null || stat -f %z "$f"); sha=$(sha "$f" || true)
  mime=$(file -b --mime-type "$f" 2>/dev/null || echo "?"); ext="${f##*.}"; ext="${ext,,}"; kind="$mime"
  case "$ext" in
    zip) extra=$(list_zip "$f"); json=$(printf '{"path":"%s","kind":"zip","size":%s,"sha256":"%s","meta":%s}' "$f" "$size" "$sha" "$extra");;
    tar|gz|tgz|bz2|xz) extra=$(list_tar "$f"); json=$(printf '{"path":"%s","kind":"tar","size":%s,"sha256":"%s","meta":%s}' "$f" "$size" "$sha" "$extra");;
    db|sqlite|sqlite3) extra=$(list_sqlite "$f"); json=$(printf '{"path":"%s","kind":"sqlite","size":%s,"sha256":"%s","meta":%s}' "$f" "$size" "$sha" "$extra");;
    sh|py|js|json|md|txt|cfg|ini|yml|yaml) preview=$(summ "$f" 12 | sed 's/[[:cntrl:]]//g'); json=$(printf '{"path":"%s","kind":"text","size":%s,"sha256":"%s","preview":"%s"}' "$f" "$size" "$sha" "$preview"); extra="preview";;
    *) json=$(printf '{"path":"%s","kind":"%s","size":%s,"sha256":"%s"}' "$f" "$kind" "$size" "$sha"); extra="";;
  esac
  add "$f" "$kind" "$size" "$sha" "$extra" "$json"
done < <(find "$BASE" -type f -print0 2>/dev/null)
echo "$(date '+%F %T') [OK] CSV:$CSV JSONL:$JSON SQLITE:$SQL" | tee -a "$LOG"
if command -v rclone >/dev/null 2>&1; then
  DST="VAULT:RAFAELIA_INDEX"; DATE=$(date '+%Y-%m-%d')
  echo "$(date '+%F %T') [SYNC] → $DST/$DATE" | tee -a "$LOG"
  rclone copy "$OUT" "$DST/$DATE" --fast-list --checkers=8 --transfers=8 --tpslimit=8 --log-level INFO || true
  echo "$(date '+%F %T') [SYNC] Concluído" | tee -a "$LOG"
fi
EOF
chmod +x "$HOME_DIR/RAFAELIA_INDEX_TOTAL.sh"

# ───────────────────────── ALIASES ÚTEIS (consultas rápidas ao índice)
grep -q '# RAFAELIA ALIASES' "$HOME_DIR/.bashrc" 2>/dev/null || cat >> "$HOME_DIR/.bashrc" <<'EOF'
# RAFAELIA ALIASES
alias raf.topzips='sqlite3 ~/RAFAELIA_INDEX/index.sqlite "SELECT path,json_extract(json,'\''$.meta.entries'\'') AS n FROM files WHERE kind LIKE '\''%zip%'\'' ORDER BY n DESC LIMIT 30;"'
alias raf.voy='sqlite3 ~/RAFAELIA_INDEX/index.sqlite "SELECT path FROM files WHERE kind='\''text'\'' AND LOWER(json_extract(json,'\''$.preview'\'')) LIKE '\''%voynich%'\'';"'
alias raf.topdb='sqlite3 ~/RAFAELIA_INDEX/index.sqlite "SELECT path,json_extract(json,'\''$.meta.tables'\'') AS t FROM files WHERE kind LIKE '\''%sqlite%'\'' ORDER BY t DESC LIMIT 30;"'
EOF

# ───────────────────────── CRON JOBS (watchdog/health/backup)
ensure_cron(){
  local line="$1"
  (crontab -l 2>/dev/null | grep -Fv "$line" || true; echo "$line") | crontab -
}
ensure_cron "*/30 * * * * $HOME_DIR/bin/rafaelia_watchdog.sh >/dev/null 2>&1"
ensure_cron "15 3 * * *  $HOME_DIR/bin/rafaelia_health.sh   >/dev/null 2>&1"
ensure_cron "45 3 * * *  $HOME_DIR/bin/rafaelia_backup_gdrive.sh >/dev/null 2>&1"

# ───────────────────────── EXECUÇÃO: instalar ferramentas e bind core
"$HOME_DIR/install_rafaelia_tools.sh"
"$HOME_DIR/BLOCO_BIND_CORE_v3.sh"

# ───────────────────────── HARDENING final e toque do pulsar 144kHz
"$HOME_DIR/bin/rafaelia_hardening.sh"
"$HOME_DIR/inicio_ponto_necessariov3.sh" --cycles 1 --quiet || true

say "✅ BLOCO_UNO_ABSOLUTO concluído."
