#!/usr/bin/env bash
#
# Script para assinar APKs do UserLAnd usando apksigner
#
# Uso:
#   ./scripts/sign_apk.sh <caminho-para-apk-nao-assinado> [caminho-para-apk-assinado]
#
# O script suporta duas formas de configuração:
#   1. Variáveis de ambiente: KEYSTORE_FILE, KEY_ALIAS, KEYSTORE_PASSWORD, KEY_PASSWORD
#   2. Arquivo signing.properties no diretório raiz do projeto
#
# Exemplo:
#   ./scripts/sign_apk.sh app/build/outputs/apk/release/app-release-unsigned.apk
#   ./scripts/sign_apk.sh app/build/outputs/apk/release/app-release-unsigned.apk app-signed.apk

set -euo pipefail

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Diretório raiz do projeto
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Arquivo de propriedades de assinatura
SIGNING_PROPS="${ROOT_DIR}/signing.properties"

# Função para exibir mensagens de erro
error() {
  echo -e "${RED}[ERRO]${NC} $1" >&2
  exit 1
}

# Função para exibir mensagens de sucesso
success() {
  echo -e "${GREEN}[SUCESSO]${NC} $1"
}

# Função para exibir mensagens de aviso
warning() {
  echo -e "${YELLOW}[AVISO]${NC} $1"
}

# Função para exibir mensagens informativas
info() {
  echo "[INFO] $1"
}

# Função de uso
usage() {
  cat <<'USAGE'
Uso: scripts/sign_apk.sh <caminho-para-apk-nao-assinado> [caminho-para-apk-assinado]

Este script assina APKs usando o apksigner do Android SDK.

Argumentos:
  <caminho-para-apk-nao-assinado>  Caminho para o APK não assinado (obrigatório)
  [caminho-para-apk-assinado]      Caminho para o APK assinado (opcional)
                                    Se não fornecido, será gerado automaticamente
                                    removendo '-unsigned' ou adicionando '-signed'

Configuração:
  O script pode ser configurado de duas formas (em ordem de prioridade):

  1. Variáveis de ambiente:
     KEYSTORE_FILE       - Caminho para o arquivo keystore (.jks)
     KEY_ALIAS           - Alias da chave no keystore
     KEYSTORE_PASSWORD   - Senha do keystore
     KEY_PASSWORD        - Senha da chave

  2. Arquivo signing.properties na raiz do projeto:
     Copie signing.properties.template para signing.properties
     e preencha com suas credenciais

Exemplos:
  # Usando arquivo de propriedades
  ./scripts/sign_apk.sh app/build/outputs/apk/release/app-release-unsigned.apk

  # Usando variáveis de ambiente
  KEYSTORE_FILE=my.jks KEY_ALIAS=mykey KEYSTORE_PASSWORD=pass1 KEY_PASSWORD=pass2 \
    ./scripts/sign_apk.sh app/build/outputs/apk/release/app-release-unsigned.apk

  # Especificando o arquivo de saída
  ./scripts/sign_apk.sh input-unsigned.apk output-signed.apk

Requisitos:
  - Android SDK instalado com build-tools
  - apksigner disponível no PATH ou em $ANDROID_SDK_ROOT/build-tools/*/

USAGE
}

# Verificar argumentos
if [[ $# -lt 1 ]]; then
  usage
  exit 1
fi

if [[ "$1" == "-h" || "$1" == "--help" ]]; then
  usage
  exit 0
fi

INPUT_APK="$1"
OUTPUT_APK="${2:-}"

# Verificar se o APK de entrada existe
if [[ ! -f "$INPUT_APK" ]]; then
  error "APK não encontrado: $INPUT_APK"
fi

# Se o APK de saída não foi especificado, gerar automaticamente
if [[ -z "$OUTPUT_APK" ]]; then
  if [[ "$INPUT_APK" == *"-unsigned.apk" ]]; then
    OUTPUT_APK="${INPUT_APK/-unsigned.apk/-signed.apk}"
  else
    OUTPUT_APK="${INPUT_APK%.apk}-signed.apk"
  fi
  info "APK de saída será: $OUTPUT_APK"
fi

# Carregar configuração do arquivo de propriedades se existir
if [[ -f "$SIGNING_PROPS" ]]; then
  info "Carregando configuração de: $SIGNING_PROPS"
  # Ler propriedades ignorando comentários e linhas vazias
  while IFS='=' read -r key value; do
    # Ignorar linhas vazias e comentários
    if [[ -z "$key" || "$key" =~ ^[[:space:]]*# ]]; then
      continue
    fi
    # Remover espaços em branco
    key=$(echo "$key" | xargs)
    value=$(echo "$value" | xargs)
    # Exportar variável se não estiver definida
    # Usando expansão indireta ${!key} para verificar se a variável já existe
    if [[ -z "${!key:-}" ]]; then
      export "$key=$value"
    fi
  done < "$SIGNING_PROPS"
fi

# Verificar se as variáveis necessárias estão definidas
REQUIRED_VARS=("KEYSTORE_FILE" "KEY_ALIAS" "KEYSTORE_PASSWORD" "KEY_PASSWORD")
MISSING_VARS=()

for var in "${REQUIRED_VARS[@]}"; do
  if [[ -z "${!var:-}" ]]; then
    MISSING_VARS+=("$var")
  fi
done

if [[ ${#MISSING_VARS[@]} -gt 0 ]]; then
  error "Variáveis de assinatura não definidas: ${MISSING_VARS[*]}
Configure através de variáveis de ambiente ou arquivo signing.properties
Execute './scripts/sign_apk.sh --help' para mais informações"
fi

# Resolver caminho do keystore (pode ser relativo ao ROOT_DIR)
if [[ ! "$KEYSTORE_FILE" = /* ]]; then
  KEYSTORE_FILE="${ROOT_DIR}/${KEYSTORE_FILE}"
fi

# Verificar se o keystore existe
if [[ ! -f "$KEYSTORE_FILE" ]]; then
  error "Arquivo keystore não encontrado: $KEYSTORE_FILE"
fi


unescape_properties_value() {
  local value="$1"
  value="${value//\\\\/\\}"
  value="${value//\\ / }"
  value="${value//\\:/:}"
  echo "$value"
}


# Encontrar apksigner
find_apksigner() {
  # Tentar encontrar no PATH primeiro
  if command -v apksigner &> /dev/null; then
    command -v apksigner
    return 0
  fi

  # Função auxiliar para encontrar a versão mais recente do build-tools
  find_latest_build_tools_apksigner() {
    local build_tools_dir="$1"
    if [[ -d "$build_tools_dir" ]]; then
      local latest_version=$(ls -1 "$build_tools_dir" | sort -V | tail -n 1)
      if [[ -n "$latest_version" && -f "${build_tools_dir}/${latest_version}/apksigner" ]]; then
        echo "${build_tools_dir}/${latest_version}/apksigner"
        return 0
      fi
    fi
    return 1
  }

  # Tentar encontrar no Android SDK
  if [[ -n "${ANDROID_SDK_ROOT:-}" ]]; then
    local apksigner_path=$(find_latest_build_tools_apksigner "${ANDROID_SDK_ROOT}/build-tools")
    if [[ -n "$apksigner_path" ]]; then
      echo "$apksigner_path"
      return 0
    fi
  fi

  # Tentar local.properties
  if [[ -f "${ROOT_DIR}/local.properties" ]]; then
    local sdk_dir=$(grep '^sdk.dir=' "${ROOT_DIR}/local.properties" | cut -d'=' -f2-)
    sdk_dir=$(unescape_properties_value "$sdk_dir")
    if [[ -n "$sdk_dir" ]]; then
      local apksigner_path=$(find_latest_build_tools_apksigner "${sdk_dir}/build-tools")
      if [[ -n "$apksigner_path" ]]; then
        echo "$apksigner_path"
        return 0
      fi
    fi
  fi

  return 1
}

APKSIGNER=$(find_apksigner || true)
if [[ -z "$APKSIGNER" ]]; then
  cat >&2 <<'EOF_ERROR'
apksigner não encontrado. 
Instale o Android SDK e configure ANDROID_SDK_ROOT ou adicione apksigner ao PATH.
Consulte: https://developer.android.com/studio/command-line/apksigner
EOF_ERROR
  exit 1
fi

info "Usando apksigner: $APKSIGNER"

# Criar diretório de saída se não existir
OUTPUT_DIR=$(dirname "$OUTPUT_APK")
if [[ ! -d "$OUTPUT_DIR" ]]; then
  mkdir -p "$OUTPUT_DIR"
  info "Diretório de saída criado: $OUTPUT_DIR"
fi

# Copiar APK de entrada para saída (apksigner modifica in-place)
info "Copiando APK..."
cp "$INPUT_APK" "$OUTPUT_APK"

# Assinar o APK
info "Assinando APK..."
"$APKSIGNER" sign \
  --ks "$KEYSTORE_FILE" \
  --ks-key-alias "$KEY_ALIAS" \
  --ks-pass "pass:$KEYSTORE_PASSWORD" \
  --key-pass "pass:$KEY_PASSWORD" \
  "$OUTPUT_APK" || error "Falha ao assinar o APK"

# Verificar a assinatura
info "Verificando assinatura..."
"$APKSIGNER" verify "$OUTPUT_APK" || error "Falha na verificação da assinatura do APK"

success "APK assinado com sucesso: $OUTPUT_APK"

# Exibir informações do APK assinado
if [[ -f "$OUTPUT_APK" ]]; then
  APK_SIZE=$(du -h "$OUTPUT_APK" | cut -f1)
  info "Tamanho do APK: $APK_SIZE"
fi

exit 0
