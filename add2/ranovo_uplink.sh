#!/bin/bash

# ==============================================================================
# PROJECT: RESTAURATIO GAIA // MODULE: RANOVO_UPLINK_V3 (NO-FZF FALLBACK)
# TARGET: VAULT (CRYPT) -> RANOVO (DRIVE)
# COMPLIANCE: ISO 27001 (Encryption), NIST SP 800-88 (Sanitization/Wipe)
# ==============================================================================

# --- CONFIGURAÇÃO ---
RCLONE_REMOTE="vault"   # Aponta para o Crypt (Segurança Máxima)
REMOTE_PATH="/DCIM_Backup_Cripto"
SOURCE_DIR="$HOME/storage/shared/DCIM/Camera"
LOG_FILE="uplink_session.log"

# --- CORES ---
GREEN='\033[0;32m'
CYAN='\033[0;36m'
RED='\033[0;31m'
NC='\033[0m'

clear
echo -e "${GREEN}"
echo "██████╗  █████╗ ███╗   ██╗ ██████╗ ██╗   ██╗ ██████╗"
echo "██╔══██╗██╔══██╗████╗  ██╗██╔═══██╗██║   ██║██╔═══██╗"
echo "██████╔╝███████║██╔██╗ ██║██║   ██║██║   ██║██║   ██║"
echo "██╔══██╗██╔══██║██║╚██╗██║██║   ██║╚██╗ ██╔╝██║   ██║"
echo "██║  ██║██║  ██║██║ ╚████║╚██████╔╝ ╚████╔╝ ╚██████╔╝"
echo "╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝ ╚═════╝   ╚═══╝   ╚═════╝ "
echo -e "   :: STORAGE: CRITICAL :: MODE: SELECTIVE WIPE ::${NC}"
echo ""

# --- PASSO 1: SELEÇÃO DE ALVOS (COM FALLBACK) ---
echo -e "${CYAN}[1/3] ESCANEANDO SETOR: $SOURCE_DIR${NC}"
rm -f selection_list.tmp

if command -v fzf &> /dev/null; then
    # MODO FZF (Se estiver instalado)
    find "$SOURCE_DIR" -maxdepth 1 -type f -printf "%s\t%p\n" | \
    fzf --multi --with-nth=2 --preview 'echo "Size: {1} bytes"' > selection_list.tmp
else
    # MODO NATIVO (Se FZF falhar por falta de espaço)
    echo -e "${RED}[AVISO] FZF não detectado (Erro de Espaço?). Usando modo manual.${NC}"
    echo "Arquivos encontrados:"
    i=0
    files=()
    # Lista arquivos com índice
    for file in "$SOURCE_DIR"/*; do
        if [ -f "$file" ]; then
            files+=("$file")
            size=$(du -h "$file" | cut -f1)
            echo "[$i] $(basename "$file") ($size)"
            ((i++))
        fi
    done
    
    echo -e "\n${CYAN}Digite os números dos arquivos para subir (ex: 0 2 5) ou 'a' para TODOS:${NC}"
    read -r selection
    
    if [ "$selection" == "a" ]; then
        for file in "${files[@]}"; do echo -e "0\t$file" >> selection_list.tmp; done
    else
        for index in $selection; do
            if [ -n "${files[$index]}" ]; then
                echo -e "0\t${files[$index]}" >> selection_list.tmp
            fi
        done
    fi
fi

# Validação
if [ ! -s selection_list.tmp ]; then
    echo -e "${RED}[ABORT] Nada selecionado.${NC}"
    exit 0
fi

# --- PASSO 2: CONTROLE DE CADÊNCIA ---
echo -e "\n${CYAN}[2/3] CADÊNCIA DE REDE (Traffic Shaping)${NC}"
echo "Digite o limite (ex: 500k, 2M) ou ENTER para ILIMITADO:"
read -p ">> " BW_INPUT
BW_LIMIT=${BW_INPUT:-0}

# --- PASSO 3: EXECUÇÃO (WIPE SEGURO) ---
echo -e "\n${GREEN}[3/3] INICIANDO UPLINK PARA '$RCLONE_REMOTE'${NC}"
echo -e "${RED}MODO MOVE ATIVO: Arquivos serão apagados do local APÓS verificação.${NC}"
sleep 2

while IFS=$'\t' read -r _ file_path; do
    filename=$(basename "$file_path")
    echo -ne "${CYAN}Processando: $filename ... ${NC}"
    
    # COMANDO MOVE: Sobe -> Checa Hash -> Apaga Local
    if rclone move "$file_path" "$RCLONE_REMOTE:$REMOTE_PATH" --bwlimit "$BW_LIMIT" --checksum --stats-one-line --tries 3; then
        echo -e "${GREEN}[SUCESSO/WIPED]${NC}"
        echo "$(date) | OK | $filename" >> "$LOG_FILE"
    else
        echo -e "${RED}[FALHA/MANTIDO]${NC}"
        echo "$(date) | ERR | $filename" >> "$LOG_FILE"
    fi
done < selection_list.tmp

rm selection_list.tmp
echo -e "\n${GREEN}:: OPERAÇÃO FINALIZADA ::${NC}"
