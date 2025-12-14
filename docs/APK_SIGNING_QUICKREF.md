# Referência Rápida - Assinatura de APKs

## 🔑 Variáveis de Assinatura

| Variável | Descrição | Exemplo |
|----------|-----------|---------|
| `KEYSTORE_FILE` | Caminho para o arquivo keystore (.jks) | `keystore.jks` ou `/path/to/keystore.jks` |
| `KEY_ALIAS` | Alias da chave no keystore | `userland-release-key` |
| `KEYSTORE_PASSWORD` | Senha do arquivo keystore | `myStorePass123` |
| `KEY_PASSWORD` | Senha da chave específica | `myKeyPass456` |

## 📋 Configuração Rápida

### 1. Copie o template
```bash
cp signing.properties.template signing.properties
```

### 2. Edite as variáveis
```bash
nano signing.properties  # ou seu editor preferido
```

### 3. Preencha com seus valores
```properties
KEYSTORE_FILE=meu-keystore.jks
KEY_ALIAS=meu-alias
KEYSTORE_PASSWORD=senha-do-keystore
KEY_PASSWORD=senha-da-chave
```

## 🚀 Uso Rápido

### Assinar um APK
```bash
./scripts/sign_apk.sh app/build/outputs/apk/release/app-release-unsigned.apk
```

### Assinar com nome customizado
```bash
./scripts/sign_apk.sh input.apk output-signed.apk
```

### Usar variáveis de ambiente (sem arquivo)
```bash
KEYSTORE_FILE=my.jks KEY_ALIAS=key KEYSTORE_PASSWORD=pass1 KEY_PASSWORD=pass2 \
  ./scripts/sign_apk.sh input.apk
```

## 🛠️ Criar Novo Keystore

```bash
keytool -genkey -v \
  -keystore userland.jks \
  -keyalg RSA \
  -keysize 2048 \
  -validity 10000 \
  -alias userland-key
```

## ✅ Verificar Assinatura

```bash
apksigner verify meu-app-signed.apk
apksigner verify --verbose meu-app-signed.apk  # detalhado
```

## 🔒 Segurança

⚠️ **NUNCA** commite:
- `signing.properties` (suas senhas)
- `*.jks` (seu keystore)

✅ Estes arquivos já estão no `.gitignore`

## 📚 Documentação Completa

Para mais detalhes, consulte: [docs/APK_SIGNING.md](docs/APK_SIGNING.md)

## 🆘 Ajuda

```bash
./scripts/sign_apk.sh --help
```

## 📦 Exemplo Completo: Build + Sign

```bash
# 1. Build unsigned APK
./scripts/build_unsigned_apk.sh

# 2. Sign APK
./scripts/sign_apk.sh \
  app/build/outputs/apk/release/app-release-unsigned.apk \
  userland-signed.apk

# 3. Verify
apksigner verify userland-signed.apk

echo "✅ APK pronto: userland-signed.apk"
```
