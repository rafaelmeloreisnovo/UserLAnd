# Garantia de Compilação / Compilation Guarantee

Este documento descreve como garantir que o projeto UserLAnd compile corretamente em diferentes ambientes.

This document describes how to ensure the UserLAnd project compiles correctly in different environments.

## 📋 Requisitos Mínimos / Minimum Requirements

### Software Obrigatório / Required Software

1. **Java Development Kit (JDK) 17+**
   - Versão mínima: 17
   - Versão recomendada: 17 ou 21
   - Verificar: `java -version`

2. **Android SDK**
   - Platform API 35 (Android 15)
   - Build Tools 35.0.0
   - Será baixado automaticamente se não estiver presente

3. **Android NDK**
   - Versão especificada: 27.3.13750724 (configurada em `gradle.properties`)
   - Será baixado automaticamente durante o build

4. **Gradle**
   - Versão: 8.11.1 (via wrapper incluído no projeto)
   - Usar sempre `./gradlew` ao invés de instalação global

### Requisitos de Sistema / System Requirements

- **RAM**: Mínimo 8GB, recomendado 16GB
- **Espaço em disco**: Mínimo 10GB livres
- **Sistema Operacional**: Linux, macOS ou Windows

## 🔍 Script de Verificação / Verification Script

### Uso Básico / Basic Usage

Execute o script de garantia de compilação:

```bash
./scripts/ensure_compilation.sh
```

Este script verifica:
- ✅ Versão do Java
- ✅ Gradle wrapper e versão
- ✅ Android SDK e NDK
- ✅ Arquivos de configuração do build
- ✅ Arquivos fonte críticos
- ✅ Configuração de build nativo
- ✅ Resolução de dependências
- ⚠️  Problemas comuns conhecidos

### Teste Completo de Compilação / Full Compilation Test

Para executar um teste completo incluindo a compilação:

```bash
RUN_COMPILATION_TEST=true ./scripts/ensure_compilation.sh
```

Isso irá:
1. Executar todas as verificações básicas
2. Compilar o APK de debug
3. Verificar binários nativos
4. Validar o APK gerado

## 🚀 Como Garantir a Compilação / How to Ensure Compilation

### 1. Verificação Rápida / Quick Check

```bash
# Verificar requisitos
./scripts/ensure_compilation.sh

# Compilar APK de debug
./gradlew assembleDebug
```

### 2. Build Limpo / Clean Build

Para garantir um build limpo sem cache:

```bash
# Limpar todos os artefatos de build
./gradlew clean

# Limpar cache do Gradle (se necessário)
rm -rf ~/.gradle/caches/

# Recompilar
./gradlew assembleDebug --no-configuration-cache
```

### 3. Build com Scripts Auxiliares / Build with Helper Scripts

O projeto inclui scripts que automatizam o processo:

```bash
# Build completo com verificações
./scripts/build_apk.sh --task assembleDebug

# Build sem assinatura (para CI/CD)
./scripts/build_unsigned_apk.sh

# Build apenas para ARM64 (mais rápido)
./scripts/build_apk.sh --arch arm64-v8a --task assembleDebug
```

## 🔧 Solução de Problemas / Troubleshooting

### Problema: Java não encontrado ou versão incorreta

**Solução**:
```bash
# Linux
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
export PATH=$JAVA_HOME/bin:$PATH

# macOS
export JAVA_HOME=$(/usr/libexec/java_home -v 17)

# Verificar
java -version
```

### Problema: Android SDK não encontrado

**Solução 1 - Automática**:
```bash
./scripts/build_apk.sh --skip-build
# Isso configura o SDK em build/android-sdk
```

**Solução 2 - Manual**:
```bash
# Criar local.properties
echo "sdk.dir=/caminho/para/android/sdk" > local.properties

# Ou exportar variável de ambiente
export ANDROID_SDK_ROOT=/caminho/para/android/sdk
```

### Problema: Falha de memória durante compilação

**Solução**:
```bash
# Editar gradle.properties
echo "org.gradle.jvmargs=-Xmx4096m -XX:MaxMetaspaceSize=1024m" >> gradle.properties

# Ou usar variável de ambiente
export GRADLE_OPTS="-Xmx4096m"
```

### Problema: Dependências não resolvidas

**Solução**:
```bash
# Forçar refresh das dependências
./gradlew assembleDebug --refresh-dependencies

# Ou limpar e recompilar
./gradlew clean assembleDebug --refresh-dependencies
```

### Problema: Build nativo falha

**Solução**:
```bash
# Verificar NDK
echo $ANDROID_NDK_HOME

# Se não estiver definido, será baixado automaticamente
./gradlew assembleDebug

# Verificar compilação de binários nativos
./scripts/compile_check.sh
```

## 📝 Checklist de Compilação / Compilation Checklist

Antes de abrir um pull request ou fazer deploy:

- [ ] Executar `./scripts/ensure_compilation.sh` sem erros
- [ ] Executar `./gradlew assembleDebug` com sucesso
- [ ] Verificar que o APK foi gerado em `app/build/outputs/apk/debug/`
- [ ] Executar `./gradlew ktlint` para verificar estilo de código
- [ ] Executar testes (se aplicável): `./gradlew test`
- [ ] Verificar warnings no log de compilação
- [ ] Testar instalação do APK em dispositivo/emulador

## 🤖 CI/CD

### GitHub Actions

O projeto usa GitHub Actions para garantir compilação contínua. Os workflows incluem:

1. **userland-android-ci.yml** - Build automático para push/PR
2. **android-debug.yml** - Build manual de debug
3. **build-arm64-guaranteed.yml** - Build ARM64 com validação
4. **build-all-architectures.yml** - Build paralelo de todas arquiteturas

### Workflow de Exemplo

```yaml
- name: Verificar compilação
  run: |
    ./scripts/ensure_compilation.sh
    
- name: Compilar APK
  run: |
    ./gradlew assembleDebug --no-daemon --stacktrace
```

## 🔐 Compilação com Assinatura / Signed Build

Para compilação de release com assinatura:

1. **Criar keystore** (uma vez):
```bash
keytool -genkey -v -keystore keystore.jks \
  -keyalg RSA -keysize 2048 -validity 10000 \
  -alias release-key
```

2. **Configurar variáveis de ambiente**:
```bash
export ANDROID_KEY_ALIAS=release-key
export ANDROID_STORE_PASSWORD=sua_senha
export ANDROID_KEY_PASSWORD=sua_senha
```

3. **Compilar release assinado**:
```bash
./gradlew assembleRelease
```

**⚠️ Importante**: Nunca faça commit da keystore ou senhas!

## 📊 Métricas de Compilação / Compilation Metrics

### Tempos Esperados / Expected Times

- **Primeira compilação**: 5-10 minutos (download de dependências)
- **Compilações subsequentes**: 1-3 minutos (com cache)
- **Build limpo**: 3-5 minutos

### Tamanhos de Artefatos / Artifact Sizes

- **APK Debug**: ~15-20 MB (todas arquiteturas)
- **APK Debug (single arch)**: ~8-12 MB
- **APK Release**: ~12-18 MB (minified/optimized)

## 🔍 Diagnóstico Avançado / Advanced Diagnostics

### Verificar configuração do Gradle

```bash
./gradlew -v
./gradlew properties | grep -E "sdk|ndk|java"
```

### Listar dependências

```bash
./gradlew :app:dependencies
```

### Verificar configuração do Android

```bash
./gradlew :app:androidDependencies
```

### Build com logs detalhados

```bash
./gradlew assembleDebug --info --stacktrace
```

### Build com diagnóstico completo

```bash
./gradlew assembleDebug --debug > build.log 2>&1
```

## 📚 Recursos Adicionais / Additional Resources

- [README.md](../README.md) - Visão geral do projeto
- [BUILD_ON_GITHUB.md](../BUILD_ON_GITHUB.md) - Build no GitHub Actions
- [docs/DEVELOPMENT_SETUP.md](DEVELOPMENT_SETUP.md) - Guia de setup
- [docs/BUILD_TROUBLESHOOTING.md](BUILD_TROUBLESHOOTING.md) - Solução de problemas
- [docs/COMMON_TASKS.md](COMMON_TASKS.md) - Tarefas comuns

## 🆘 Suporte / Support

Se você encontrar problemas de compilação que não estão cobertos aqui:

1. Verifique [issues abertas](https://github.com/CypherpunkArmory/UserLAnd/issues)
2. Execute `./scripts/ensure_compilation.sh` e compartilhe a saída
3. Execute `./gradlew assembleDebug --stacktrace` e compartilhe o log
4. Abra uma nova issue com todas as informações relevantes

## 🔄 Atualização da Documentação / Documentation Updates

Este documento deve ser atualizado quando:
- Versões de dependências mudarem
- Novos requisitos forem adicionados
- Problemas comuns forem descobertos
- Scripts de build forem modificados

---

**Última atualização**: 2026-01-04  
**Versão do documento**: 1.0.0
