#!/bin/bash

# ==============================================================================
# SISTEMA: RANOVO EXPLORER (BASH NAVIGATOR)
# FUNÇÃO: NAVEGAR ENTRE PASTAS -> SELECIONAR -> ENVIAR LOTE MISTO
# COMPLIANCE: ISO 27001 (Integrity Check)
# ==============================================================================

# --- CONFIGURAÇÃO ---
RCLONE_REMOTE="rafanovo"
REMOTE_PATH_ROOT="/_Backup_Cripto"
# Começa onde você quiser (ex: armazenamento interno)
CURRENT_DIR="/storage/0886-EC05" 

# Lista de itens selecionados (caminhos completos)
declare -A selected_items

# --- CORES ---
GREEN='\033[0;32m'
BLUE='\033[1;34m'
CYAN='\033[0;36m'
WHITE='\033[1;37m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
REVERSE='\033[7m'
NC='\033[0m'

# --- CONFIGURAÇÃO DE UPLOAD ---
BWLIMIT="0" # Ilimitado por padrão
TRANSFERS="4"

# --- FUNÇÃO: DESENHAR TELA ---
draw_browser() {
    clear
    echo -e "${GREEN}:: RANOVO EXPLORER :: [Setas]=Mover [Enter]=Entrar [Espaço]=Marcar [e]=ENVIAR${NC}"
    echo -e "${CYAN}Dir Atual: $CURRENT_DIR${NC}"
    echo -e "${YELLOW}Itens Marcados: ${#selected_items[@]}${NC}"
    echo -e "----------------------------------------------------------------"

    # Listar diretórios primeiro, depois arquivos
    # Array 'entries' vai guardar o nome do arquivo para exibição
    # Array 'paths' vai guardar o caminho completo
    entries=()
    paths=()
    types=() # d = dir, f = file

    # Adicionar opção de voltar se não for a raiz absoluta (opcional)
    entries+=(".. (Voltar)")
    paths+=("..")
    types+=("d")

    # Ler conteúdo
    while read -r line; do
        entries+=("$(basename "$line")/")
        paths+=("$line")
        types+=("d")
    done < <(find "$CURRENT_DIR" -maxdepth 1 -mindepth 1 -type d | sort)

    while read -r line; do
        entries+=("$(basename "$line")")
        paths+=("$line")
        types+=("f")
    done < <(find "$CURRENT_DIR" -maxdepth 1 -mindepth 1 -type f | sort)

    # Renderizar janela (limite visual simples)
    total_entries=${#entries[@]}
    
    start_idx=0
    # Lógica simples de scroll se quiser, mas aqui vamos simplificar mostrando tudo
    # Se a pasta for gigante, o terminal rola.
    
    for ((i=0; i<total_entries; i++)); do
        path="${paths[i]}"
        name="${entries[i]}"
        type="${types[i]}"
        
        # Verifica se está marcado
        if [[ -v "selected_items[$path]" ]]; then
            mark="${GREEN}[X]${NC}"
        else
            mark="[ ]"
        fi

        # Cor do item (Azul para pasta, Branco para arquivo)
        if [ "$type" == "d" ]; then
            display_name="${BLUE}$name${NC}"
        else
            display_name="${WHITE}$name${NC}"
        fi

        # Renderiza cursor
        if [ "$i" -eq "$cursor_pos" ]; then
            echo -e "${REVERSE} > $mark $display_name ${NC}"
        else
            echo -e "   $mark $display_name"
        fi
    done
    echo -e "----------------------------------------------------------------"
}

# --- LOOP DE NAVEGAÇÃO ---
cursor_pos=0

while true; do
    draw_browser
    
    # Ler tecla (compatibilidade total termux)
    read -rsn1 key
    if [[ "$key" == $'\x1b' ]]; then
        read -rsn2 key # Ler o resto da sequencia de escape
        case "$key" in
            '[A') # Cima
                ((cursor_pos--))
                if [ $cursor_pos -lt 0 ]; then cursor_pos=$((total_entries-1)); fi
                ;;
            '[B') # Baixo
                ((cursor_pos++))
                if [ $cursor_pos -ge $total_entries ]; then cursor_pos=0; fi
                ;;
        esac
    else
        case "$key" in
            "") # Enter (Entrar na pasta)
                target_type="${types[cursor_pos]}"
                target_path="${paths[cursor_pos]}"
                
                if [ "$target_path" == ".." ]; then
                    CURRENT_DIR=$(dirname "$CURRENT_DIR")
                    cursor_pos=0
                elif [ "$target_type" == "d" ]; then
                    if [ -r "$target_path" ]; then
                        CURRENT_DIR="$target_path"
                        cursor_pos=0
                    fi
                fi
                ;;
            " ") # Espaço (Marcar)
                target_path="${paths[cursor_pos]}"
                if [ "$target_path" != ".." ]; then
                    if [[ -v "selected_items[$target_path]" ]]; then
                        unset "selected_items[$target_path]"
                    else
                        selected_items["$target_path"]=1
                    fi
                fi
                ;;
            "e"|"E") # Executar Upload
                if [ ${#selected_items[@]} -eq 0 ]; then
                    echo -e "\n${RED}Nada selecionado!${NC}"
                    sleep 1
                else
                    break # Sai do loop e vai pro upload
                fi
                ;;
            "q") # Sair
                exit 0
                ;;
        esac
    fi
done

# --- FASE DE UPLOAD (LOTE) ---
clear
echo -e "${CYAN}:: CONFIGURANDO LOTE DE UPLOAD ::${NC}"
echo "Itens para processar: ${#selected_items[@]}"

# Config rápida de cadência
read -p "Limite de Banda (ex: 1M) ou Enter [Max]: " input_bw
BWLIMIT=${input_bw:-0}
read -p "Arquivos simultâneos (ex: 4) ou Enter [4]: " input_trans
TRANSFERS=${input_trans:-4}

echo -e "\n${GREEN}INICIANDO PROTOCOLO MOVE (INTEGRITY CHECK ON)...${NC}"
sleep 1

for item in "${!selected_items[@]}"; do
    base_name=$(basename "$item")
    echo -e "\n${YELLOW}>>> Processando: $base_name${NC}"
    
    # Se for diretório, usa estrutura recursiva
    # Se for arquivo, move direto para raiz do backup ou mantém estrutura?
    # Vamos mover mantendo o nome
    
    # Rclone Move com Checksum
    rclone move "$item" "$RCLONE_REMOTE:$REMOTE_PATH_ROOT/$base_name" \
        --transfers "$TRANSFERS" \
        --bwlimit "$BWLIMIT" \
        --checksum \
        --stats-one-line \
        -P
        
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}[SUCESSO] Item Seguro e Removido do Local.${NC}"
    else
        echo -e "${RED}[ERRO] Falha na integridade. Item mantido.${NC}"
    fi
done

echo -e "\n${GREEN}:: OPERAÇÃO FINALIZADA ::${NC}"
