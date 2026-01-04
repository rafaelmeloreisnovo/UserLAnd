#!/usr/bin/env bash
#
# ZIPRAF OMEGA - Script de Ativação Rápida
# ==========================================
# Ativa o sistema Zipraf Omega se já estiver configurado

set -euo pipefail

# Cores
CYAN='\033[0;36m'
NEON='\033[38;5;82m'
NC='\033[0m'

# Diretório do script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OMEGA_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${OMEGA_DIR}"

echo -e "${NEON}🪉 ZIPRAF OMEGA - Ativação Rápida${NC}"
echo ""

# Verificar se já está configurado
if [ -f "db/omega.db" ]; then
    echo -e "${CYAN}Sistema detectado. Mostrando status...${NC}"
    echo ""
    python3 omega_manager.py --status
else
    echo -e "${CYAN}Sistema não configurado. Iniciando assistente...${NC}"
    echo ""
    python3 omega_manager.py --setup
fi
