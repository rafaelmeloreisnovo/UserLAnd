#!/usr/bin/env bash
#
# ZIPRAF OMEGA - Script de Instalação
# ====================================
# Configura o ambiente para o sistema Zipraf Omega

set -euo pipefail

# Cores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NEON='\033[38;5;82m'
NC='\033[0m'

echo -e "${NEON}========================================${NC}"
echo -e "${NEON}🪉 ZIPRAF OMEGA - Instalação 🪉${NC}"
echo -e "${NEON}========================================${NC}"
echo ""

# Diretório do script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OMEGA_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="$(cd "${OMEGA_DIR}/.." && pwd)"

echo -e "${CYAN}[INFO]${NC} Diretório Omega: ${OMEGA_DIR}"
echo -e "${CYAN}[INFO]${NC} Diretório Root: ${ROOT_DIR}"
echo ""

# Verificar Python
echo -e "${CYAN}[1/5] Verificando Python...${NC}"
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}[ERRO]${NC} Python 3 não encontrado!"
    echo -e "${YELLOW}       Instale Python 3.8 ou superior${NC}"
    exit 1
fi

PYTHON_VERSION=$(python3 --version | cut -d' ' -f2)
echo -e "${GREEN}[OK]${NC} Python ${PYTHON_VERSION} encontrado"
echo ""

# Verificar pip
echo -e "${CYAN}[2/5] Verificando pip...${NC}"
if ! command -v pip3 &> /dev/null; then
    echo -e "${RED}[ERRO]${NC} pip3 não encontrado!"
    echo -e "${YELLOW}       Instale pip: python3 -m ensurepip${NC}"
    exit 1
fi

echo -e "${GREEN}[OK]${NC} pip encontrado"
echo ""

# Instalar dependências Python
echo -e "${CYAN}[3/5] Instalando dependências Python...${NC}"
cd "${OMEGA_DIR}"

if [ -f "requirements.txt" ]; then
    pip3 install -r requirements.txt --user --quiet
    echo -e "${GREEN}[OK]${NC} Dependências instaladas"
else
    echo -e "${YELLOW}[AVISO]${NC} requirements.txt não encontrado"
fi
echo ""

# Criar diretório de banco de dados
echo -e "${CYAN}[4/5] Criando estrutura de diretórios...${NC}"
mkdir -p "${OMEGA_DIR}/db"
echo -e "${GREEN}[OK]${NC} Diretórios criados"
echo ""

# Tornar scripts executáveis
echo -e "${CYAN}[5/5] Configurando permissões...${NC}"
chmod +x "${OMEGA_DIR}/omega_manager.py" 2>/dev/null || true
chmod +x "${SCRIPT_DIR}/setup.sh" 2>/dev/null || true
chmod +x "${SCRIPT_DIR}/activate.sh" 2>/dev/null || true
echo -e "${GREEN}[OK]${NC} Permissões configuradas"
echo ""

# Verificar .gitignore
echo -e "${CYAN}[INFO] Verificando .gitignore...${NC}"
GITIGNORE="${ROOT_DIR}/.gitignore"

if [ -f "${GITIGNORE}" ]; then
    if ! grep -q "zipraf_omega/db/omega.db" "${GITIGNORE}"; then
        echo "# Zipraf Omega - Banco de dados" >> "${GITIGNORE}"
        echo "zipraf_omega/db/omega.db" >> "${GITIGNORE}"
        echo "zipraf_omega/db/*.db" >> "${GITIGNORE}"
        echo -e "${GREEN}[OK]${NC} Entradas adicionadas ao .gitignore"
    else
        echo -e "${GREEN}[OK]${NC} .gitignore já configurado"
    fi
else
    echo -e "${YELLOW}[AVISO]${NC} .gitignore não encontrado"
fi
echo ""

# Resumo
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}✓ Instalação concluída com sucesso!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "${CYAN}Próximos passos:${NC}"
echo -e "  1. Execute o assistente de configuração:"
echo -e "     ${NEON}cd ${OMEGA_DIR}${NC}"
echo -e "     ${NEON}python3 omega_manager.py --setup${NC}"
echo ""
echo -e "  2. Ou use o script de ativação rápida:"
echo -e "     ${NEON}./scripts/activate.sh${NC}"
echo ""
echo -e "${CYAN}Comandos disponíveis:${NC}"
echo -e "  ${NEON}python3 omega_manager.py --status${NC}  - Ver status do sistema"
echo -e "  ${NEON}python3 omega_manager.py --sign <apk>${NC} - Assinar APK"
echo -e "  ${NEON}python3 omega_manager.py --logs${NC}    - Ver logs"
echo ""
echo -e "${YELLOW}⚠️  IMPORTANTE:${NC}"
echo -e "  - Configure seu GitHub PAT com as permissões corretas"
echo -e "  - NUNCA commite o arquivo omega.db"
echo -e "  - Mantenha suas credenciais seguras"
echo ""
