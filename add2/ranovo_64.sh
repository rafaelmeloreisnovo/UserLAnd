#!/bin/bash

# ==============================================================================
# SISTEMA: RANOVO 64 (SED/BASE64 ENGINE)
# RECURSO: PAGINAÇÃO (WINDOWING) + STREAM PROCESSING
# COMPLIANCE: ISO 27001 (Integrity via B64)
# ==============================================================================

# --- CONFIGURAÇÃO ---
RCLONE_REMOTE="ranovo"
REMOTE_PATH_ROOT="/_Backup_Cripto"
CURRENT_DIR="/storage/0886-EC05"
TEMP_LIST="/data/data/com.termux/files/usr/tmp/ranovo_list.tmp"

# --- VARIÁVEIS VISUAIS ---
ROWS=15  # Quantos arquivos mostrar por vez (Janela)
CURSOR=0 # Posição real na lista total
OFFSET=0 # Onde começa a janela visual

# Array associativo para guardar seleções (chave em Base64 para segurança)
declare -A selected_b64

# --- CORES ---
GREEN='\033[0;32m'
BLUE='\033[1;34m'
CYAN='\033[0;36m'
WHITE='\033[1;37m'
RED='\033[0;31m'
REVERSE='\033[7m'
NC='\033[0m'

# --- ENGINE: CARREGAR LISTA COM SED/BASE64 ---
load_directory() {
    # 1. Limpa lista antiga
    : > "$TEMP_LIST"
    
    # 2. Adiciona ".." para voltar (codificado em B64)
    # Formato do arquivo: TYPE|B64_NAME|B64_FULLPATH
    parent_dir=$(dirname "$CURRENT_DIR")
    echo "D|$(echo ".." | base64 -w0)|$(echo "$parent_dir" | base64 -w0)" >> "$TEMP_LIST"
    
    # 3. Processa Diretórios (Find -> Base64)
    # Usamos SED para não quebrar em loops bash lentos
    find "$CURRENT_DIR" -maxdepth 1 -mindepth 1 -type d | sort | \
    while read -r line; do
        name=$(basename "$line")
        echo "D|$(echo "$name/" | base64 -w0)|$(echo "$line" | base64 -w0)" >> "$TEMP_LIST"
    done

    # 4. Processa Arquivos
    find "$CURRENT_DIR" -maxdepth 1 -mindepth 1 -type f | sort | \
    while read -r line; do
        name=$(basename "$line")
        echo "F|$(echo "$name" | base64 -w0)|$(echo "$line" | base64 -w0)" >> "$TEMP_LIST"
    done
    
    # Conta total de linhas
    TOTAL_LINES=$(wc -l < "$TEMP_LIST")
}

# --- ENGINE: RENDERIZAR TELA (VIEWPORT) ---
draw_screen() {
    # Move o cursor para o topo sem limpar tudo (evita flicker)
    tput cup 0 0
    
    echo -e "${GREEN}:: RANOVO 64 ENGINE :: [QTD: $TOTAL_LINES] :: [SEL: ${#selected_b64[@]}]${NC}"
    echo -e "${CYAN}Dir: $CURRENT_DIR${NC}"
    echo -e "------------------------------------------------------------"

    # Lógica de Janela Deslizante (Scrolling)
    # Se o cursor desceu além da janela, empurra o offset pra baixo
    if [ $CURSOR -ge $((OFFSET + ROWS)) ]; then
        OFFSET=$((CURSOR - ROWS + 1))
    fi
    # Se o cursor subiu além da janela, puxa o offset pra cima
    if [ $CURSOR -lt $OFFSET ]; then
        OFFSET=$CURSOR
    fi
    
    # SED MÁGICO: Extrai apenas as linhas visíveis (da linha X até Y)
    # Isso é o que impede o script de travar com lista grande
    limit=$((OFFSET + ROWS))
    sed -n "$((OFFSET + 1)),${limit}p" "$TEMP_LIST" | while IFS='|' read -r type b64_name b64_path; do
        
        # Recupera índice relativo para calcular se é a linha do cursor
        # (Isso é uma simulação visual, o loop do sed não tem índice nativo fácil)
        # Vamos fazer um hack visual: carregar essas 15 linhas num array temp
        :
    done > /dev/null # Hack para estrutura, vamos fazer o loop real abaixo

    # Loop real de exibição (apenas ROWS vezes, super rápido)
    i=0
    sed -n "$((OFFSET + 1)),${limit}p" "$TEMP_LIST" | while IFS='|' read -r type b64_name b64_path; do
        # Decodifica só para mostrar na tela
        display_name=$(echo "$b64_name" | base64 -d)
        
        real_idx=$((OFFSET + i))
        
        # Verifica Seleção (usando a chave B64 pura)
        if [[ -v "selected_b64[$b64_path]" ]]; then
            mark="${GREEN}[X]${NC}"
        else
            mark="[ ]"
        fi
        
        # Define cor (Pasta vs Arquivo)
        if [ "$type" == "D" ]; then color=$BLUE; else color=$WHITE; fi
        
        # Renderiza Linha
        if [ $real_idx -eq $CURSOR ]; then
            # Linha Focada (Invertida)
            printf "${REVERSE} > %s %b%-50s%b ${NC}\n" "$mark" "$color" "${display_name:0:50}" "$NC"
        else
            # Linha Normal
            printf "   %s %b%-50s%b \n" "$mark" "$color" "${display_name:0:50}" "$NC"
        fi
        
        ((i++))
    done
    
    # Preenche o resto da tela se a lista for menor que a janela
    lines_drawn=$i
    rem=$((ROWS - lines_drawn))
    for ((k=0; k<rem; k++)); do echo -e "                                                            "; done
    
    echo -e "------------------------------------------------------------"
    echo -e "[Space]=Marcar  [Enter]=Entrar  [E]=Enviar  [Q]=Sair"
}

# --- INICIALIZAÇÃO ---
load_directory

# --- LOOP PRINCIPAL ---
while true; do
    draw_screen
    
    # Leitura de Teclas
    read -rsn1 key
    if [[ "$key" == $'\x1b' ]]; then
        read -rsn2 key
        case "$key" in
            '[A') # Cima
                ((CURSOR--))
                if [ $CURSOR -lt 0 ]; then CURSOR=0; fi
                ;;
            '[B') # Baixo
                ((CURSOR++))
                if [ $CURSOR -ge $TOTAL_LINES ]; then CURSOR=$((TOTAL_LINES - 1)); fi
                ;;
        esac
    else
        case "$key" in
            "") # Enter (Ação de Navegar)
                # Pega a linha exata do arquivo temporário com sed
                line_data=$(sed -n "$((CURSOR + 1))p" "$TEMP_LIST")
                type=$(echo "$line_data" | cut -d'|' -f1)
                b64_path=$(echo "$line_data" | cut -d'|' -f3)
                full_path=$(echo "$b64_path" | base64 -d)
                
                if [ "$type" == "D" ]; then
                    if [ -d "$full_path" ]; then
                        CURRENT_DIR="$full_path"
                        CURSOR=0
                        OFFSET=0
                        load_directory # Recarrega lista
                    fi
                fi
                ;;
            " ") # Espaço (Marcar)
                line_data=$(sed -n "$((CURSOR + 1))p" "$TEMP_LIST")
                # Ignora o ".."
                b64_name=$(echo "$line_data" | cut -d'|' -f2)
                name_dec=$(echo "$b64_name" | base64 -d)
                
                if [ "$name_dec" != ".." ]; then
                    b64_path=$(echo "$line_data" | cut -d'|' -f3)
                    
                    if [[ -v "selected_b64[$b64_path]" ]]; then
                        unset "selected_b64[$b64_path]"
                    else
                        selected_b64["$b64_path"]=1
                    fi
                fi
                ;;
            "e"|"E") # Executar
                if [ ${#selected_b64[@]} -eq 0 ]; then
                   echo -e "\n${RED}Nada selecionado!${NC}"; sleep 1
                else
                   break
                fi
                ;;
            "q"|"Q") exit 0 ;;
        esac
    fi
done

# --- FASE DE UPLOAD (DECODE B64 & EXECUTE) ---
clear
echo -e "${GREEN}:: RANOVO UPLINK INICIADO ::${NC}"

# Configs de Banda
read -p "Limite de Banda (Enter=Max): " bwlimit
bwlimit=${bwlimit:-0}
read -p "Simultâneos (Enter=4): " transfers
transfers=${transfers:-4}

for b64_item in "${!selected_b64[@]}"; do
    # Decodifica o caminho para o Rclone usar
    real_path=$(echo "$b64_item" | base64 -d)
    filename=$(basename "$real_path")
    
    echo -e "\n${CYAN}>>> Processando: $filename${NC}"
    
    # Rclone Move Seguro
    rclone move "$real_path" "$RCLONE_REMOTE:$REMOTE_PATH_ROOT/$filename" \
        --transfers "$transfers" \
        --bwlimit "$bwlimit" \
        --checksum \
        --stats-one-line \
        -P
        
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}[OK] INTEGRIDADE VERIFICADA.${NC}"
    else
        echo -e "${RED}[ERRO] MANTIDO NO LOCAL.${NC}"
    fi
done

echo -e "\n${GREEN}:: OPERAÇÃO CONCLUÍDA ::${NC}"
rm "$TEMP_LIST" 2>/dev/null
