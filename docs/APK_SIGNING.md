# Guia de Assinatura de APKs - UserLAnd

Este documento explica como configurar e usar o sistema de assinatura de APKs para o projeto UserLAnd.

## Visão Geral

O projeto UserLAnd inclui um script de assinatura de APKs (`scripts/sign_apk.sh`) que utiliza a ferramenta `apksigner` do Android SDK para assinar APKs de forma segura e reutilizável.

## Pré-requisitos

1. **Android SDK**: Instale o Android SDK com build-tools
2. **Keystore**: Um arquivo keystore (.jks) válido com suas chaves de assinatura
3. **Bash**: Shell Bash para executar o script

## Configuração

### Opção 1: Usando Arquivo de Propriedades (Recomendado)

1. Copie o arquivo template:
   ```bash
   cp signing.properties.template signing.properties
   ```

2. Edite `signing.properties` com suas credenciais:
   ```properties
   KEYSTORE_FILE=path/to/your/keystore.jks
   KEY_ALIAS=your-key-alias
   KEYSTORE_PASSWORD=your-keystore-password
   KEY_PASSWORD=your-key-password
   ```

3. **IMPORTANTE**: O arquivo `signing.properties` está no `.gitignore` e NÃO deve ser commitado no repositório por questões de segurança.

### Opção 2: Usando Variáveis de Ambiente

Defina as seguintes variáveis de ambiente antes de executar o script:

```bash
export KEYSTORE_FILE=/path/to/keystore.jks
export KEY_ALIAS=my-key-alias
export KEYSTORE_PASSWORD=my-store-password
export KEY_PASSWORD=my-key-password
```

## Variáveis de Assinatura

### KEYSTORE_FILE
- **Descrição**: Caminho para o arquivo keystore (.jks)
- **Tipo**: String (caminho absoluto ou relativo ao diretório raiz)
- **Exemplo**: `keystore.jks` ou `/home/user/keys/userland.jks`

### KEY_ALIAS
- **Descrição**: Alias da chave dentro do keystore
- **Tipo**: String
- **Exemplo**: `userland-release-key`

### KEYSTORE_PASSWORD
- **Descrição**: Senha para acessar o arquivo keystore
- **Tipo**: String
- **Exemplo**: `mySecurePassword123`

### KEY_PASSWORD
- **Descrição**: Senha da chave específica dentro do keystore
- **Tipo**: String
- **Exemplo**: `myKeyPassword456`

## Uso do Script de Assinatura

### Sintaxe Básica

```bash
./scripts/sign_apk.sh <apk-nao-assinado> [apk-assinado]
```

### Exemplos

#### 1. Assinar APK com nome automático
```bash
./scripts/sign_apk.sh app/build/outputs/apk/release/app-release-unsigned.apk
# Saída: app/build/outputs/apk/release/app-release-signed.apk
```

#### 2. Assinar APK com nome personalizado
```bash
./scripts/sign_apk.sh app/build/outputs/apk/release/app-release-unsigned.apk userland-v2.8.3-signed.apk
```

#### 3. Usando variáveis de ambiente (sem arquivo signing.properties)
```bash
KEYSTORE_FILE=my.jks \
KEY_ALIAS=mykey \
KEYSTORE_PASSWORD=pass1 \
KEY_PASSWORD=pass2 \
./scripts/sign_apk.sh app/build/outputs/apk/release/app-release-unsigned.apk
```

## Integração com Build

### Build e Assinatura em Um Único Comando

```bash
# 1. Construir APK não assinado
./scripts/build_unsigned_apk.sh

# 2. Assinar o APK
./scripts/sign_apk.sh app/build/outputs/apk/release/app-release-unsigned.apk
```

### Script Completo de Build e Assinatura

Você pode criar um script personalizado para automatizar todo o processo:

```bash
#!/usr/bin/env bash
set -euo pipefail

# Build
./scripts/build_unsigned_apk.sh

# Sign
./scripts/sign_apk.sh \
  app/build/outputs/apk/release/app-release-unsigned.apk \
  userland-release-signed.apk

echo "APK pronto: userland-release-signed.apk"
```

## Criando um Keystore

Se você ainda não tem um keystore, pode criar um usando o `keytool`:

```bash
keytool -genkey -v \
  -keystore userland.jks \
  -keyalg RSA \
  -keysize 2048 \
  -validity 10000 \
  -alias userland-key
```

Você será solicitado a fornecer:
- Senha do keystore
- Senha da chave
- Informações do certificado (nome, organização, etc.)

**IMPORTANTE**: Guarde o keystore e as senhas em um local seguro! Se você perder o keystore, não poderá publicar atualizações do seu aplicativo.

## Verificando a Assinatura

O script automaticamente verifica a assinatura após assinar o APK. Para verificar manualmente:

```bash
# Usando apksigner
apksigner verify app-signed.apk

# Para informações detalhadas
apksigner verify --verbose app-signed.apk
```

## Estrutura de Arquivos

```
UserLAnd/
├── signing.properties.template    # Template de configuração
├── signing.properties            # Suas credenciais (não commitado)
├── keystore.jks                  # Seu keystore (não commitado)
├── scripts/
│   ├── sign_apk.sh              # Script principal de assinatura
│   ├── build_apk.sh             # Build APK normal
│   └── build_unsigned_apk.sh    # Build APK não assinado
└── app/
    └── build.gradle             # Configuração de build existente
```

## Segurança

### Boas Práticas

1. **NUNCA** commite o arquivo `signing.properties` no repositório
2. **NUNCA** commite arquivos `.jks` (keystore) no repositório
3. Use senhas fortes e únicas para keystore e chaves
4. Mantenha backup do keystore em local seguro
5. Considere usar gestores de senhas para armazenar credenciais
6. Em ambientes de CI/CD, use secrets ou variáveis de ambiente criptografadas

### Arquivos no .gitignore

Os seguintes arquivos estão automaticamente ignorados:
- `*.jks` (arquivos keystore)
- `signing.properties` (credenciais de assinatura)

## Solução de Problemas

### apksigner não encontrado

**Problema**: `apksigner não encontrado`

**Solução**: 
1. Instale o Android SDK
2. Configure `ANDROID_SDK_ROOT`:
   ```bash
   export ANDROID_SDK_ROOT=/path/to/android/sdk
   ```
3. Ou adicione apksigner ao PATH:
   ```bash
   export PATH=$PATH:$ANDROID_SDK_ROOT/build-tools/<version>
   ```

### Erro de verificação de assinatura

**Problema**: `Falha na verificação da assinatura do APK`

**Solução**:
- Verifique se o keystore está correto
- Confirme se o alias está correto
- Verifique as senhas
- Certifique-se de que o APK não estava já assinado

### Keystore não encontrado

**Problema**: `Arquivo keystore não encontrado`

**Solução**:
- Verifique o caminho em `KEYSTORE_FILE`
- Use caminho absoluto se caminho relativo não funcionar
- Confirme que o arquivo existe e tem permissões de leitura

## Compatibilidade

### Assinatura pelo Gradle vs Script

O projeto UserLAnd já possui suporte para assinatura via Gradle (veja `app/build.gradle`). O script `sign_apk.sh` oferece uma alternativa mais flexível:

**Vantagens do script:**
- Pode assinar qualquer APK, não apenas os gerados pelo projeto
- Não requer rebuild completo
- Mais fácil de usar em pipelines personalizados
- Configuração independente do Gradle

**Quando usar Gradle:**
- Build e assinatura em um único comando
- Integração com Android Studio
- Para builds oficiais de release/beta

**Quando usar o script:**
- Assinar APKs já construídos
- Testes rápidos de assinatura
- Workflows customizados
- Re-assinatura de APKs

### Diferenças nas Variáveis de Ambiente

**Gradle** (em `app/build.gradle`):
- `ANDROID_KEY_ALIAS` - Alias da chave
- `ANDROID_STORE_PASSWORD` - Senha do keystore
- `ANDROID_KEY_PASSWORD` - Senha da chave
- Keystore fixo: `../keystore.jks`

**Script sign_apk.sh**:
- `KEY_ALIAS` - Alias da chave
- `KEYSTORE_PASSWORD` - Senha do keystore
- `KEY_PASSWORD` - Senha da chave
- `KEYSTORE_FILE` - Caminho configurável do keystore

Ambos os métodos são independentes e podem ser usados conforme a necessidade.

## Suporte

Para problemas ou dúvidas:
1. Verifique a documentação do Android: https://developer.android.com/studio/command-line/apksigner
2. Execute `./scripts/sign_apk.sh --help` para ver opções disponíveis
3. Abra uma issue no repositório do projeto

## Licença

Este sistema de assinatura faz parte do projeto UserLAnd e está sujeito à mesma licença do projeto principal.
