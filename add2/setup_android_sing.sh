#!/usr/bin/env bash
set -euo pipefail

echo "=== Setup Android Signing + GitHub Actions ==="
echo

if [ $# -lt 2 ]; then
  echo "Uso: $0 /caminho/para/repo /caminho/para/keystore.jks"
  echo "Exemplo: $0 ~/projetos/UserLAnd-mod ~/keys/userland-release-key.jks"
  exit 1
fi

REPO_DIR="$1"
KEYSTORE_PATH="$2"

if [ ! -d "$REPO_DIR" ]; then
  echo "ERRO: Diretório do repositório não existe: $REPO_DIR"
  exit 1
fi

if [ ! -f "$KEYSTORE_PATH" ]; then
  echo "ERRO: Keystore (.jks) não encontrado: $KEYSTORE_PATH"
  exit 1
fi

cd "$REPO_DIR"

echo "Repositório: $(pwd)"
echo "Keystore:    $KEYSTORE_PATH"
echo

# 1) Criar pasta de workflows
echo ">> Criando pasta .github/workflows (se não existir)..."
mkdir -p .github/workflows

# 2) Criar workflow de debug
echo ">> Criando .github/workflows/android-debug.yml..."
cat > .github/workflows/android-debug.yml << 'EOF'
name: Android Debug APK

on:
  push:
    branches: [ "master" ]
  pull_request:
    branches: [ "master" ]

jobs:
  build:
    name: Build debug APK
    runs-on: ubuntu-latest

    steps:
      - name: Checkout repository
        uses: actions/checkout@v4

      - name: Set up JDK 8
        uses: actions/setup-java@v4
        with:
          distribution: 'temurin'
          java-version: '8'
          cache: 'gradle'

      - name: Grant execute permission for gradlew
        run: chmod +x ./gradlew

      - name: Build debug APK
        run: ./gradlew clean assembleDebug --stacktrace --no-daemon

      - name: Upload debug APK artifact
        uses: actions/upload-artifact@v4
        with:
          name: userland-debug-apk
          path: app/build/outputs/apk/debug/*.apk
EOF

# 3) Criar workflow de release
echo ">> Criando .github/workflows/android-release.yml..."
cat > .github/workflows/android-release.yml << 'EOF'
name: Android Release APK

on:
  workflow_dispatch:

jobs:
  build:
    name: Build release APK
    runs-on: ubuntu-latest

    steps:
      - name: Checkout repository
        uses: actions/checkout@v4

      - name: Set up JDK 8
        uses: actions/setup-java@v4
        with:
          distribution: 'temurin'
          java-version: '8'
          cache: 'gradle'

      - name: Decode and prepare keystore
        env:
          ANDROID_KEYSTORE_BASE64: ${{ secrets.ANDROID_KEYSTORE_BASE64 }}
        run: |
          if [ -z "$ANDROID_KEYSTORE_BASE64" ]; then
            echo "ANDROID_KEYSTORE_BASE64 secret is not set."
            exit 1
          fi
          echo "$ANDROID_KEYSTORE_BASE64" | base64 --decode > userland-release-key.jks
          ls -l userland-release-key.jks

      - name: Grant execute permission for gradlew
        run: chmod +x ./gradlew

      - name: Build release APK
        env:
          ANDROID_KEY_ALIAS: ${{ secrets.ANDROID_KEY_ALIAS }}
          ANDROID_STORE_PASSWORD: ${{ secrets.ANDROID_STORE_PASSWORD }}
          ANDROID_KEY_PASSWORD: ${{ secrets.ANDROID_KEY_PASSWORD }}
        run: ./gradlew clean assembleRelease --stacktrace --no-daemon

      - name: Upload release APK artifact
        uses: actions/upload-artifact@v4
        with:
          name: userland-release-apk
          path: app/build/outputs/apk/release/*.apk
EOF

# 4) Criar CI geral (opcional, mas útil)
echo ">> Criando .github/workflows/userland-android-ci.yml..."
cat > .github/workflows/userland-android-ci.yml << 'EOF'
name: UserLAnd Android CI

on:
  push:
    branches: [ "master" ]
  pull_request:
    branches: [ "master" ]

jobs:
  build-android:
    runs-on: ubuntu-latest

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Set up JDK 8
        uses: actions/setup-java@v4
        with:
          distribution: 'temurin'
          java-version: '8'
          cache: 'gradle'

      - name: Grant execute permission for gradlew
        run: chmod +x ./gradlew

      - name: Run unit tests
        run: ./gradlew testDebugUnitTest --stacktrace --no-daemon

      - name: Build debug APK
        run: ./gradlew assembleDebug --stacktrace --no-daemon

      - name: Upload debug APK artifact
        uses: actions/upload-artifact@v4
        with:
          name: userland-debug-apk
          path: app/build/outputs/apk/debug/*.apk
EOF

# 5) Perguntar alias e senhas (sem ecoar)
echo
echo ">> Coletando dados de assinatura (isso fica APENAS na sua máquina)..."
read -rp "Alias da key (ANDROID_KEY_ALIAS): " ANDROID_KEY_ALIAS
echo
read -rsp "Senha do keystore (.jks) (ANDROID_STORE_PASSWORD): " ANDROID_STORE_PASSWORD
echo
read -rsp "Senha da key (ANDROID_KEY_PASSWORD): " ANDROID_KEY_PASSWORD
echo
echo

# 6) Criar arquivo local .env.android-signing
ENV_FILE=".env.android-signing"
echo ">> Gravando dados em $ENV_FILE (não faça commit desse arquivo)..."

cat > "$ENV_FILE" << EOF
# NÃO COMMITAR ESTE ARQUIVO
ANDROID_KEY_ALIAS="$ANDROID_KEY_ALIAS"
ANDROID_STORE_PASSWORD="$ANDROID_STORE_PASSWORD"
ANDROID_KEY_PASSWORD="$ANDROID_KEY_PASSWORD"
EOF

# 7) Garantir que .env.android-signing e o .jks estão no .gitignore
echo ">> Atualizando .gitignore para proteger dados sensíveis..."
if [ ! -f .gitignore ]; then
  touch .gitignore
fi

if ! grep -q "^.env.android-signing\$" .gitignore; then
  echo ".env.android-signing" >> .gitignore
fi

# Nome do jks que será reconstruído no Actions
JKS_BASENAME="userland-release-key.jks"

if ! grep -q "^$JKS_BASENAME\$" .gitignore; then
  echo "$JKS_BASENAME" >> .gitignore
fi

# 8) Gerar base64 do keystore
echo ">> Gerando base64 do keystore em keystore.b64..."
base64 "$KEYSTORE_PATH" | tr -d '\n' > keystore.b64

echo
echo "=== RESUMO DO QUE FOI FEITO ==="
echo "1) Criados workflows:"
echo "   - .github/workflows/android-debug.yml"
echo "   - .github/workflows/android-release.yml"
echo "   - .github/workflows/userland-android-ci.yml"
echo
echo "2) Criado arquivo local .env.android-signing com:"
echo "   ANDROID_KEY_ALIAS"
echo "   ANDROID_STORE_PASSWORD"
echo "   ANDROID_KEY_PASSWORD"
echo "   (arquivo já adicionado ao .gitignore)"
echo
echo "3) Gerado arquivo keystore.b64 na raiz do repo."
echo "   O conteúdo desse arquivo deve ser usado como valor do secret ANDROID_KEYSTORE_BASE64 no GitHub."
echo
echo "=== PRÓXIMOS PASSOS MANUAIS (GitHub UI) ==="
echo "A) Abra o arquivo keystore.b64 e copie TODO o conteúdo (uma linha gigante)."
echo "B) No GitHub, vá em: Settings -> Secrets and variables -> Actions -> New repository secret:"
echo "   - ANDROID_KEY_ALIAS        = $ANDROID_KEY_ALIAS"
echo "   - ANDROID_STORE_PASSWORD   = (mesma senha que você digitou aqui)"
echo "   - ANDROID_KEY_PASSWORD     = (mesma senha que você digitou aqui)"
echo "   - ANDROID_KEYSTORE_BASE64  = (conteúdo de keystore.b64)"
echo
echo "C) Depois vá em Actions -> workflow \"Android Release APK\" -> Run workflow."
echo
echo "IMPORTANTE:"
echo "- Ajuste seu app/build.gradle para usar:"
echo "    storeFile file(\"../$JKS_BASENAME\")"
echo "  e as variáveis de ambiente ANDROID_* para storePassword, keyAlias e keyPassword."
echo
echo "Feito. Agora é só configurar os secrets no GitHub e rodar o workflow. 🦉"
