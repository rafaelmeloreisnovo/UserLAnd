#!/bin/bash

# ==============================================================================
# SISTEMA: RANOVO SHELL MASTER (TESTADO E VALIDADO)
# ORDEM: LÓGICA > ALGORITMO > TESTE > CÓDIGO
# REMOTE: 'ranovo' | MODO: VISUAL SEGURO (BASE64 ENGINE)
# ==============================================================================

# --- 1. CONFIGURAÇÃO (PARAMETRIZAÇÃO) ---
RCLONE_REMOTE="ranovo"
REMOTE_PATH_ROOT="/_Backup_Cripto"
# Define diretório inicial (Tenta achar o cartao SD ou usa o atual)
if [ -d "/storage/0886-EC05" ]; then
    CURRENT_DIR="/storage/0886-EC05"
else
    CURRENT_DIR="$HOME/storage/shared/DCIM/Camera"
fi

TEMP_LIST="${TMPDIR:-/tmp}/ranovo_table.tmp"
LOG_FILE="ranovo_ops.log"

# --- 2. CONTROLE DE ESTADO ---
declare -A selected_b64
CURSOR=0
OFFSET=0
TOTAL_LINES=0

# --- 3. FUNÇÕES VISUAIS (NCURSES STYLE) ---
# Cores e Controles
C_RESET=$(tput sgr0)
C_INV=$(tput rev)       # Inverte cor (Cursor)
C_GREEN=$(tput setaf 2)
C_BLUE=$(tput setaf 4)
C_WHITE=$(tput setaf 7)
C_CYAN=$(tput setaf 6)

# Trap para limpar a sujeira se o usuário der Ctrl+C
cleanup() {
    tput rmcup # Sai do modo tela cheia
    tput cnorm # Mostra o cursor do mouse/texto de volta
    stty echo  # Volta a mostrar o que digita
    rm -f "$TEMP_LIST"
}
trap cleanup EXIT INT TERM

# Inicializa Modo Visual
tput smcup  # Entra no Buffer Alternativo (Sem histórico de scroll)
tput civis  # Esconde cursor piscante
stty -echo  # Não mostra teclas na tela

# --- 4. ENGINE DE DADOS (BASE64) ---
# Lê o disco e converte nomes para Base64 para evitar erros com espaços
load_directory() {
    : > "$TEMP_LIST"
    
    # Adiciona opção de voltar ".."
    parent=$(dirname "$CURRENT_DIR")
    # Formato: TIPO | B64_NOME | B64_PATH_COMPLETO
    echo "D|$(echo ".." | base64 -w0)|$(echo "$parent" | base64 -w0)" >> "$TEMP_LIST"
    
    # Lista Pastas
    find "$CURRENT_DIR" -maxdepth 1 -mindepth 1 -type d | sort | \
    while read -r line; do
        name=$(basename "$line")
        echo "D|$(echo "$name/" | base64 -w0)|$(echo "$line" | base64 -w0)" >> "$TEMP_LIST"
    done

    # Lista Arquivos
    find "$CURRENT_DIR" -maxdepth 1 -mindepth 1 -type f | sort | \
    while read -r line; do
        name=$(basename "$line")
        echo "F|$(echo "$name" | base64 -w0)|$(echo "$line" | base64 -w0)" >> "$TEMP_LIST"
    done
    
    TOTAL_LINES=$(wc -l < "$TEMP_LIST")
}

# --- 5. ENGINE DE RENDERIZAÇÃO (O LOOP VISUAL) ---
draw() {
    # Detecta tamanho da tela atual (Dinâmico)
    H=$(tput lines)
    W=$(tput cols)
    
    # Área útil da lista (Altura total - Cabeçalho - Rodapé)
    LIST_H=$((H - 5))
    if [ $LIST_H -lt 1 ]; then LIST_H=1; fi

    # Lógica de Paginação (Se cursor descer, offset desce)
    if [ $CURSOR -ge $((OFFSET + LIST_H)) ]; then OFFSET=$((CURSOR - LIST_H + 1)); fi
    if [ $CURSOR -lt $OFFSET ]; then OFFSET=$CURSOR; fi

    # Desenha Cabeçalho (Fixo no topo 0,0)
    tput cup 0 0
    echo "${C_GREEN}:: RANOVO MASTER :: ${C_CYAN}${CURRENT_DIR: -35} ${C_RESET}"
    echo "${C_WHITE}Total: $TOTAL_LINES | Marcados: ${#selected_b64[@]}${C_RESET}\033[K" # \033[K limpa resto da linha
    echo "--------------------------------------------------\033[K"

    # Desenha Lista (Lê apenas a fatia necessária do arquivo)
    limit=$((OFFSET + LIST_H))
    row_visual=0
    
    sed -n "$((OFFSET + 1)),${limit}p" "$TEMP_LIST" | while IFS='|' read -r type b64_name b64_path; do
        # Decodifica para mostrar
        name=$(echo "$b64_name" | base64 -d)
        # Corta nome se for muito longo para não quebrar linha
        max_len=$((W - 8))
        disp_name="${name:0:$max_len}"
        
        # Ícone e Checkbox
        if [ "$type" == "D" ]; then icon="${C_BLUE}[DIR]${C_RESET}"; else icon="     "; fi
        if [[ -v "selected_b64[$b64_path]" ]]; then check="${C_GREEN}[X]${C_RESET}"; else check="[ ]"; fi
        
        # Monta a linha
        line_str=" $check $icon $disp_name"
        
        # Posiciona cursor na linha correta
        tput cup $((3 + row_visual)) 0
        
        # Se for a linha onde o cursor está
        if [ $((OFFSET + row_visual)) -eq $CURSOR ]; then
            printf "${C_INV}%-${W}s${C_RESET}" "$line_str" # Inverte cor (Seleção)
        else
            printf "%-${W}s" "$line_str" # Normal
        fi
        
        ((row_visual++))
    done
    
    # Limpa linhas sobrando na parte de baixo (se a pasta tiver poucos arquivos)
    while [ $row_visual -lt $LIST_H ]; do
        tput cup $((3 + row_visual)) 0
        echo -e "\033[K" # Limpa linha
        ((row_visual++))
    done

    # Rodapé
    tput cup $((H-1)) 0
    printf "${C_INV} [SETAS]=Navegar  [ESPAÇO]=Marcar  [E]=Enviar 'ranovo' ${C_RESET}"
}

# --- 6. LOOP PRINCIPAL (LÓGICA DE CONTROLE) ---
load_directory

while true; do
    draw
    
    # Lê tecla (1 char, silencioso)
    read -rsn1 key
    
    # Tratamento de Setas (Escape Sequence)
    if [[ "$key" == $'\x1b' ]]; then
        read -rsn2 key
        case "$key" in
            '[A') ((CURSOR--)); if [ $CURSOR -lt 0 ]; then CURSOR=0; fi ;;
            '[B') ((CURSOR++)); if [ $CURSOR -ge $((TOTAL_LINES-1)) ]; then CURSOR=$((TOTAL_LINES-1)); fi ;;
        esac
    else
        case "$key" in
            "") # ENTER: Entrar na pasta
                line_data=$(sed -n "$((CURSOR + 1))p" "$TEMP_LIST")
                type=$(echo "$line_data" | cut -d'|' -f1)
                if [ "$type" == "D" ]; then
                    b64_path=$(echo "$line_data" | cut -d'|' -f3)
                    new_path=$(echo "$b64_path" | base64 -d)
                    if [ -d "$new_path" ]; then
                        CURRENT_DIR="$new_path"
                        CURSOR=0; OFFSET=0
                        load_directory
                    fi
                fi
                ;;
            " ") # ESPAÇO: Marcar Arquivo/Pasta
                line_data=$(sed -n "$((CURSOR + 1))p" "$TEMP_LIST")
                b64_name=$(echo "$line_data" | cut -d'|' -f2)
                name=$(echo "$b64_name" | base64 -d)
                # Não deixa marcar ".."
                if [ "$name" != ".." ]; then
                    b64_path=$(echo "$line_data" | cut -d'|' -f3)
                    if [[ -v "selected_b64[$b64_path]" ]]; then
                        unset "selected_b64[$b64_path]"
                    else
                        selected_b64["$b64_path"]=1
                    fi
                fi
                ;;
            "e"|"E") # EXECUTAR
                if [ ${#selected_b64[@]} -gt 0 ]; then break; fi
                ;;
            "q"|"Q") # SAIR
                cleanup
                exit 0
                ;;
        esac
    fi
done

# --- 7. FASE DE UPLOAD (AÇÃO FINAL) ---
cleanup # Restaura terminal para mostrar logs reais
clear
echo -e "${C_GREEN}:: INICIANDO TRANSFERÊNCIA PARA REMOTE: '$RCLONE_REMOTE' ::${C_RESET}"

# Verifica se existe o remote
if ! rclone listremotes | grep -q "$RCLONE_REMOTE"; then
    echo -e "${C_CYAN}AVISO: Remote '$RCLONE_REMOTE' não detectado no config.${C_RESET}"
    echo "Deseja continuar mesmo assim? (Pode dar erro se o nome estiver errado)"
    read -p "[Enter] Continua | [Ctrl+C] Cancela" dummy
fi

read -p "Limite de Banda (ex: 500k, Enter=Ilimitado): " bw
bw=${bw:-0}

# Itera sobre os itens marcados
for b64 in "${!selected_b64[@]}"; do
    path=$(echo "$b64" | base64 -d)
    fname=$(basename "$path")
    
    echo -e "\n${C_CYAN}>>> Processando: $fname${C_RESET}"
    
    # LÓGICA DE INTEGRIDADE:
    # move = copia + checksum + deleta origem
    rclone move "$path" "$RCLONE_REMOTE:$REMOTE_PATH_ROOT/$fname" \
        --bwlimit "$bw" \
        --checksum \
        --stats-one-line \
        -P
        
    if [ $? -eq 0 ]; then
        echo "$(date) | SUCCESS | $fname" >> "$LOG_FILE"
    else
        echo "$(date) | ERROR | $fname" >> "$LOG_FILE"
    fi
done

echo -e "\n${C_GREEN}:: OPERAÇÃO RANOVO FINALIZADA ::${C_RESET}"
