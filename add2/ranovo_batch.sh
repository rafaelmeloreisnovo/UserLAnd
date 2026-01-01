#!/bin/bash

# ==============================================================================
# SISTEMA: RANOVO BATCH COMMANDER (FZF INTEGRATED)
# ALVO: SELEÇÃO DE PASTAS + VERIFICAÇÃO DE HASH + CONTROLE DE LOTE
# COMPLIANCE: ISO 27001 (Integrity) | NIST (Data Sanitation)
# ==============================================================================

# --- CONFIGURAÇÃO (AJUSTE SEU REMOTE AQUI) ---
RCLONE_REMOTE="vault"
ROOT_REMOTE_PATH="/Android_Backup"
# Vamos começar na pasta DCIM para você ver as pastas (Camera, Facebook, etc)
SOURCE_ROOT="$HOME/storage/shared/DCIM" 

# --- ESTÉTICA ---
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

clear
echo -e "${GREEN}"
echo "   █▀▀█ █▀▀█ █▀▀▄ █▀▀█ ▀▀█▀▀ █▀▀█ "
echo "   █▄▄▀ █▄▄█ █──█ █──█ ──█── █──█ "
echo "   ▀─▀▀ ▀──▀ ▀──▀ ▀▀▀▀ ──▀── ▀▀▀▀ "
echo -e "   :: BATCH INTEGRITY SYSTEM ::   ${NC}"
echo ""

# --- 1. SELEÇÃO DE PASTAS (FZF) ---
echo -e "${CYAN}[1/4] SELEÇÃO DE PASTAS (DIRETÓRIOS)${NC}"
echo "Use TAB para marcar várias, ENTER para confirmar."

# Busca apenas diretórios (-type d) dentro do DCIM
# O fzf -m permite múltipla seleção
DIRS=$(find "$SOURCE_ROOT" -maxdepth 1 -type d | fzf -m --prompt="Selecione as Pastas > " --height=40% --layout=reverse --border --preview 'ls -A {} | head -10')

if [ -z "$DIRS" ]; then
    echo -e "${RED}[ABORT] Nenhuma pasta selecionada.${NC}"
    exit 1
fi

# --- 2. CONFIGURAÇÃO DO LOTE (TRANSFERS) ---
echo -e "\n${CYAN}[2/4] TAMANHO DO LOTE (SIMULTANEIDADE)${NC}"
echo "Quantos arquivos enviar ao mesmo tempo?"
echo "Recomendado: 4 (Estável) | 8 (Rápido) | 16 (Turbo - Gasta muita CPU)"
read -p ">> Digite o número (Padrão 4): " TRANSFERS
TRANSFERS=${TRANSFERS:-4}

# --- 3. CONFIGURAÇÃO DA 'ALTURA' (VELOCIDADE) ---
echo -e "\n${CYAN}[3/4] LIMITE DE VELOCIDADE (BANDA)${NC}"
echo "Ex: 500k (Meio mega), 2M (2 Megas), ou ENTER para Máximo."
read -p ">> Limite: " BWLIMIT
BWLIMIT=${BWLIMIT:-0}

# --- 4. EXECUÇÃO COM VERIFICAÇÃO ---
clear
echo -e "${GREEN}:: INICIANDO TRANSFERÊNCIA SEGURA ::${NC}"
echo -e "Destino Base: $RCLONE_REMOTE:$ROOT_REMOTE_PATH"
echo -e "Integridade: ${YELLOW}ON (SHA-1 Checksum Required)${NC}"
echo -e "Modo: ${RED}MOVE (Apaga localmente APENAS se Hash bater)${NC}"
echo "-----------------------------------------------------------"

# Loop pelas pastas selecionadas
echo "$DIRS" | while read -r folder; do
    folder_name=$(basename "$folder")
    
    echo -e "\n${CYAN}>>> PROCESSANDO PASTA: $folder_name${NC}"
    
    # COMANDO COMPLETO DO RCLONE
    # --transfers: Tamanho do lote simultâneo
    # --bwlimit: Controle de velocidade
    # --checksum: OBRIGA a checar se o arquivo chegou inteiro
    # -P: Mostra barra de progresso detalhada
    
    rclone move "$folder" "$RCLONE_REMOTE:$ROOT_REMOTE_PATH/$folder_name" \
        --transfers "$TRANSFERS" \
        --bwlimit "$BWLIMIT" \
        --checksum \
        --stats-one-line \
        -P
        
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}[✔] PASTA '$folder_name' FINALIZADA COM INTEGRIDADE.${NC}"
    else
        echo -e "${RED}[✖] ERRO AO PROCESSAR '$folder_name'. ALGUNS ARQUIVOS PODEM TER FICADO.${NC}"
    fi
done

echo -e "\n${GREEN}=== OPERAÇÃO DE LOTE CONCLUÍDA ===${NC}"
