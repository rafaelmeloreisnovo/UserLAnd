# 🪉 ZIPRAF OMEGA - Sistema de Automação de Assinatura

Sistema automatizado de gerenciamento de assinatura e releases para o repositório UserLAnd, baseado na metodologia RAFAELIA OMEGA.

## 📋 Índice

- [Visão Geral](#visão-geral)
- [Características](#características)
- [Requisitos](#requisitos)
- [Instalação](#instalação)
- [Configuração](#configuração)
- [Uso](#uso)
- [Estrutura do Projeto](#estrutura-do-projeto)
- [Segurança](#segurança)
- [Integração](#integração)
- [Solução de Problemas](#solução-de-problemas)

## 🎯 Visão Geral

O **Zipraf Omega** é um sistema completo de automação que elimina a necessidade de configuração manual de assinatura de APKs. Basta fornecer seu GitHub Personal Access Token (PAT) e o sistema gerencia todo o processo de assinatura, armazenamento seguro de credenciais e integração com workflows.

### Metodologia ZIPRAF

Baseado no conceito do `raf_zipraf.c`, o sistema utiliza:
- **CRC-on-CRC**: Dupla validação de integridade
- **MMAP Database**: Banco de dados de alta performance
- **RAW IO**: Operações de I/O otimizadas
- **Criptografia AES-256**: Proteção de dados sensíveis

## ✨ Características

- ✅ **Configuração Única**: Setup inicial simples com assistente interativo
- 🔒 **Segurança Robusta**: Criptografia AES-256 para credenciais
- 🗄️ **Database Local**: SQLite para armazenar configurações e histórico
- 📊 **Logs Detalhados**: Rastreabilidade completa de operações
- 🔗 **Integração GitHub**: API nativa para releases e workflows
- 🚀 **Automação Completa**: Assinatura automática de APKs
- 📱 **Suporte Multiplataforma**: Linux, macOS, Windows (via WSL)

## 📦 Requisitos

### Sistema
- Python 3.8 ou superior
- pip (gerenciador de pacotes Python)
- Git configurado
- Bash (para scripts de automação)

### GitHub
Personal Access Token (PAT) com as seguintes permissões:
- `repo` - Acesso completo aos repositórios
- `workflow` - Gerenciar GitHub Actions workflows
- `write:packages` - Publicar releases

### Android (para assinatura)
- Android SDK instalado
- apksigner disponível no PATH
- Keystore (.jks) configurado

## 🚀 Instalação

### Método 1: Setup Automático (Recomendado)

```bash
cd zipraf_omega
./scripts/setup.sh
```

O script de setup irá:
1. ✅ Verificar Python e pip
2. ✅ Instalar dependências automaticamente
3. ✅ Criar estrutura de diretórios
4. ✅ Configurar permissões
5. ✅ Atualizar .gitignore

### Método 2: Instalação Manual

```bash
cd zipraf_omega

# Instalar dependências
pip3 install -r requirements.txt

# Criar diretórios
mkdir -p db

# Tornar scripts executáveis
chmod +x omega_manager.py scripts/*.sh
```

## ⚙️ Configuração

### Primeira Configuração

Execute o assistente de configuração:

```bash
cd zipraf_omega
python3 omega_manager.py --setup
```

O assistente irá solicitar:

1. **GitHub PAT**: Seu token de acesso pessoal
2. **Senha Mestra**: Para criptografar dados sensíveis (opcional mas recomendado)
3. **Keystore**: Caminho para o arquivo .jks
4. **Key Alias**: Alias da chave no keystore
5. **Senhas**: Senha do keystore e da chave (criptografadas)
6. **Repositório**: Owner e nome do repositório GitHub
7. **Opções**: Assinatura automática e outras preferências

### Obter GitHub PAT

1. Acesse: https://github.com/settings/tokens
2. Clique em "Generate new token" → "Generate new token (classic)"
3. Selecione as permissões:
   - ✅ `repo` (Full control of private repositories)
   - ✅ `workflow` (Update GitHub Action workflows)
   - ✅ `write:packages` (Upload packages)
4. Clique em "Generate token"
5. **Copie o token** (você não poderá vê-lo novamente!)

### Keystore Android

Se você ainda não tem um keystore:

```bash
keytool -genkey -v \
  -keystore my-release-key.jks \
  -alias my-key-alias \
  -keyalg RSA \
  -keysize 2048 \
  -validity 10000
```

## 🎮 Uso

### Comandos Principais

#### 1. Ver Status do Sistema

```bash
python3 omega_manager.py --status
```

Mostra:
- ✅ Configurações ativas
- 📊 Estatísticas de uso
- 💾 Informações do banco de dados

#### 2. Assinar APK

```bash
# Assinar APK (output automático)
python3 omega_manager.py --sign app/build/outputs/apk/release/app-release-unsigned.apk

# Especificar caminho de saída
python3 omega_manager.py --sign input.apk --output output-signed.apk
```

#### 3. Ver Logs

```bash
# Últimos 20 logs
python3 omega_manager.py --logs

# Últimos 50 logs
python3 omega_manager.py --logs --log-limit 50

# Apenas erros
python3 omega_manager.py --logs --log-level ERROR
```

#### 4. Reconfigurar Sistema

```bash
python3 omega_manager.py --setup
```

### Scripts de Conveniência

#### Ativação Rápida

```bash
./scripts/activate.sh
```

Detecta automaticamente se o sistema está configurado:
- Se SIM: Mostra o status
- Se NÃO: Inicia o assistente de configuração

## 📁 Estrutura do Projeto

```
zipraf_omega/
├── ativar.txt              # Documentação de orquestração
├── omega_manager.py        # Aplicação principal Python
├── requirements.txt        # Dependências Python
├── README.md              # Este arquivo
├── db/                    # Banco de dados (não versionado)
│   └── omega.db           # SQLite database (criado automaticamente)
└── scripts/
    ├── setup.sh           # Script de instalação
    └── activate.sh        # Script de ativação rápida
```

### Banco de Dados (SQLite)

O sistema utiliza um banco SQLite com as seguintes tabelas:

#### `config`
Armazena configurações do sistema (PAT, keystore, etc.)
- Suporte a criptografia por registro
- Timestamps de criação e atualização

#### `signatures`
Histórico de assinaturas realizadas
- Caminho do APK
- Alias usado
- Status e timestamp

#### `logs`
Logs de todas as operações
- Níveis: INFO, WARNING, ERROR
- Contexto e timestamp

#### `releases`
Releases criados no GitHub
- Tag, SHA do commit
- URL e status

## 🔒 Segurança

### Criptografia

- **Algoritmo**: AES-256 via Fernet (cryptography)
- **KDF**: PBKDF2 com SHA-256 e 100.000 iterações
- **Dados Protegidos**:
  - GitHub PAT
  - Senhas do keystore
  - Senha da chave

### Boas Práticas

✅ **O que fazer:**
- Use senhas mestras fortes
- Rotacione PATs regularmente
- Faça backup do banco de dados (criptografado)
- Revogue PATs não utilizados

❌ **O que NÃO fazer:**
- NUNCA commite `omega.db`
- NUNCA compartilhe seu PAT
- NUNCA desabilite criptografia em produção
- NUNCA use o mesmo PAT em múltiplos sistemas

### .gitignore

O sistema automaticamente adiciona ao `.gitignore`:

```gitignore
# Zipraf Omega - Banco de dados
zipraf_omega/db/omega.db
zipraf_omega/db/*.db
```

## 🔗 Integração

### Com Scripts Existentes

O Zipraf Omega integra-se perfeitamente com:

- **scripts/sign_apk.sh**: Usado internamente para assinatura
- **scripts/build_apk.sh**: Pode ser estendido para usar Omega
- **GitHub Actions**: Workflows podem invocar o Omega

### Exemplo de Integração em Workflow

```yaml
- name: Sign APK with Zipraf Omega
  env:
    GITHUB_PAT: ${{ secrets.GITHUB_PAT }}
  run: |
    cd zipraf_omega
    python3 omega_manager.py --sign ../app/build/outputs/apk/release/app-release-unsigned.apk
```

### API Python

Use o Omega como biblioteca:

```python
from omega_manager import ZipRafOmegaManager

manager = ZipRafOmegaManager()

# Assinar APK
success = manager.sign_apk('app-unsigned.apk', 'app-signed.apk')

# Obter configuração
pat = manager.get_config('github_pat', master_password)

# Registrar log
manager.log('INFO', 'Operação concluída', 'my_script')
```

## 🔧 Solução de Problemas

### Problema: "cryptography não instalado"

**Solução:**
```bash
pip3 install cryptography
```

### Problema: "Senha mestra incorreta"

**Solução:**
- Verifique se está usando a senha correta
- Se esqueceu a senha, execute `--setup` novamente
- **Nota**: Reconfigurar apagará as credenciais antigas

### Problema: "Script de assinatura não encontrado"

**Solução:**
```bash
# Verifique se o script existe
ls -la ../scripts/sign_apk.sh

# Torne executável se necessário
chmod +x ../scripts/sign_apk.sh
```

### Problema: "Permission denied" ao executar scripts

**Solução:**
```bash
chmod +x omega_manager.py scripts/*.sh
```

### Problema: Database corrompido

**Solução:**
```bash
# Backup do banco atual
mv db/omega.db db/omega.db.backup

# Recriar e reconfigurar
python3 omega_manager.py --setup
```

## 📚 Documentação Adicional

- **ativar.txt**: Documentação de orquestração e ativação
- **BUILD_ON_GITHUB.md**: Integração com GitHub Actions (repositório raiz)
- **scripts/sign_apk.sh**: Documentação do script de assinatura

## 🤝 Contribuindo

Este é um sistema interno do UserLAnd. Para sugestões ou problemas:

1. Abra uma issue no repositório
2. Descreva o problema ou sugestão
3. Inclua logs relevantes (sem expor credenciais!)

## 📄 Licença

Este sistema segue a licença do projeto UserLAnd.

## 🙏 Créditos

Desenvolvido com base na metodologia **RAFAELIA OMEGA**:
- CRC-on-CRC integrity verification
- MMAP high-performance database
- RAW IO optimizations
- Zipraf structure concepts

---

**ZIPRAF OMEGA** - Assinatura automatizada, gerenciamento simplificado 🪉
