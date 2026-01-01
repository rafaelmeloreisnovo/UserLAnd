#!/bin/bash

# ==============================================================================
# PROJECT: RANOVO PERSONALITÉ 17 [HYBRID EDITION]
# ERA: 1987 (SCROLLING LOGS) + 2025 (INTERACTIVE MENU)
# TARGET: VAULT (CRYPT) -> RANOVO (DRIVE)
# ==============================================================================

# --- CONFIG ---
RCLONE_REMOTE="vault"
REMOTE_PATH="/DCIM_Backup_Cripto"
SOURCE_DIR="$HOME/storage/shared/DCIM/Camera"
LOG_FILE="personalite_17.log"

# --- CORES (MONOCHROME AMBER/GREEN) ---
GREEN='\033[0;32m'
AMBER='\033[0;33m' # A cor clássica de terminais 87
RED='\033[0;31m'
NC='\033[0m'

# --- INIT ---
files=()
selected=()
filenames=()

# Carregar arquivos
i=0
for f in "$SOURCE_DIR"/*; do
    if [ -f "$f" ]; then
        files+=("$f")
        filenames+=("$(basename "$f")")
        selected+=(0)
        ((i++))
    fi
done

total_files=${#files[@]}

if [ "$total_files" -eq 0 ]; then
    echo "Diretório vazio: $SOURCE_DIR"
    exit 1
fi

# ==============================================================================
# FASE 1: O "BROWSER" (CREEPER 5 STYLE)
# Seleção Interativa Estática
# ==============================================================================
msg=""
while true; do
    clear
    echo -e "${GREEN}:: RANOVO PERSONALITÉ 17 :: MENU DE SELEÇÃO ::${NC}"
    echo -e "--------------------------------------------------"
    
    for ((i=0; i<total_files; i++)); do
        if [ "${selected[i]}" -eq 1 ]; then
            mark="${AMBER}[X]${NC}"
            name="${AMBER}${filenames[i]}${NC}"
        else
            mark="[ ]"
            name="${filenames[i]}"
        fi
        echo -e "$i) $mark $name"
    done
    
    echo -e "--------------------------------------------------"
    echo -e "COMANDOS: Digite o ${AMBER}NÚMERO${NC} para marcar."
    echo -e "          'a' = Todos | 'n' = Nenhum | 'go' = ENVIAR"
    echo -e "${RED}$msg${NC}"
    
    read -p "RANOVO> " input
    
    if [[ "$input" == "go" ]]; then
        break
    elif [[ "$input" == "a" ]]; then
        for ((i=0; i<total_files; i++)); do selected[i]=1; done
    elif [[ "$input" == "n" ]]; then
        for ((i=0; i<total_files; i++)); do selected[i]=0; done
    elif [[ "$input" =~ ^[0-9]+$ ]] && [ "$input" -lt "$total_files" ]; then
        if [ "${selected[input]}" -eq 1 ]; then selected[input]=0; else selected[input]=1; fi
    fi
done

# ==============================================================================
# FASE 2: O SISTEMA DE 87 (SCROLLING EXECUTION)
# A tela NÃO limpa mais. O texto desce como log de sistema.
# ==============================================================================
clear
echo -e "${AMBER}"
echo "INITIATING UPLINK PROTOCOL..."
echo "CONNECTING TO NODE: $RCLONE_REMOTE"
echo "BAUD RATE: VARIABLE"
echo "ENCRYPTION: ON"
echo "--------------------------------------------------"
echo -e "${NC}"
sleep 1

# Pergunta cadência estilo terminal antigo
echo -n "ENTER BANDWIDTH LIMIT (Example: 500k) [Def: UNLIMITED]: "
read bwlimit
if [ -z "$bwlimit" ]; then bwlimit="0"; fi

echo -e "${GREEN}EXECUTING BATCH SEQUENCE...${NC}\n"

count=0
for ((i=0; i<total_files; i++)); do
    if [ "${selected[i]}" -eq 1 ]; then
        file="${files[i]}"
        name="${filenames[i]}"
        ((count++))
        
        # Estética 87: Imprime o log ANTES e DEPOIS, criando histórico
        echo -e "${AMBER}[$count] PROCESSING: $name${NC}"
        echo -ne "     > Hashing & Transport... "
        
        # Executa Rclone Move
        if rclone move "$file" "$RCLONE_REMOTE:$REMOTE_PATH" --bwlimit "$bwlimit" --checksum --stats-one-line; then
            echo -e "${GREEN}DONE (WIPED LOCAL)${NC}"
            echo "$(date) | OK | $name" >> "$LOG_FILE"
        else
            echo -e "${RED}ERROR (RETRYING NEXT CYCLE)${NC}"
            echo "$(date) | ERR | $name" >> "$LOG_FILE"
        fi
        
        # Pequena pausa para dar o efeito de processamento antigo
        # Se quiser mais rápido, remova a linha abaixo
        sleep 0.2
    fi
done

echo -e "\n${AMBER}--------------------------------------------------"
echo "END OF LINE."
echo "SYSTEM HALTED."
echo -e "--------------------------------------------------${NC}"
