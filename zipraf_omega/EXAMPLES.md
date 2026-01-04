# ZIPRAF OMEGA - Exemplos de Uso
# ================================

Este documento fornece exemplos práticos de como usar o Zipraf Omega.

## 📚 Índice

1. [Configuração Inicial](#configuração-inicial)
2. [Assinatura de APKs](#assinatura-de-apks)
3. [Gerenciamento de Configurações](#gerenciamento-de-configurações)
4. [Logs e Monitoramento](#logs-e-monitoramento)
5. [Integração com Scripts](#integração-com-scripts)

## 🚀 Configuração Inicial

### Primeira vez - Setup completo

```bash
cd zipraf_omega

# Executar instalação
./scripts/setup.sh

# Executar assistente de configuração
python3 omega_manager.py --setup
```

O assistente irá perguntar:
1. **GitHub PAT**: Seu token de acesso
2. **Senha Mestra**: Para criptografar dados (recomendado)
3. **Keystore**: Caminho para o arquivo .jks
4. **Key Alias**: Nome da chave
5. **Senhas**: Senhas do keystore e da chave
6. **Repositório**: Owner e nome do repo

### Ativação Rápida

Se você já configurou anteriormente:

```bash
cd zipraf_omega
./scripts/activate.sh
```

## 🔒 Assinatura de APKs

### Assinar um APK (básico)

```bash
python3 omega_manager.py --sign ../app/build/outputs/apk/release/app-release-unsigned.apk
```

O sistema irá:
1. Solicitar a senha mestra (se criptografia estiver ativa)
2. Carregar as configurações do banco de dados
3. Chamar o script `sign_apk.sh` com as credenciais
4. Registrar a assinatura no banco de dados
5. Salvar o APK assinado no mesmo diretório

### Assinar com caminho de saída customizado

```bash
python3 omega_manager.py --sign input.apk --output /tmp/output-signed.apk
```

### Assinar múltiplos APKs

```bash
# Usando loop bash
for apk in app/build/outputs/apk/*/app-*.apk; do
    echo "Assinando: $apk"
    python3 omega_manager.py --sign "$apk"
done
```

### Assinar em um script automatizado

```bash
#!/bin/bash
set -euo pipefail

cd zipraf_omega

# Exportar senha mestra (use com cuidado!)
export MASTER_PASSWORD="sua-senha-aqui"

# Assinar APK
python3 omega_manager.py --sign ../app/build/outputs/apk/release/app-release-unsigned.apk

# Verificar logs
python3 omega_manager.py --logs --log-limit 5
```

## ⚙️ Gerenciamento de Configurações

### Ver status do sistema

```bash
python3 omega_manager.py --status
```

Mostra:
- Configurações ativas
- Estatísticas de uso
- Informações do banco de dados
- Status da criptografia

### Reconfigurar o sistema

```bash
# Executar setup novamente
python3 omega_manager.py --setup
```

⚠️ **Atenção**: Isto irá sobrescrever as configurações atuais!

### Backup do banco de dados

```bash
# Backup simples
cp db/omega.db db/omega.db.backup-$(date +%Y%m%d)

# Backup criptografado (recomendado)
tar czf - db/omega.db | gpg -c > omega-backup-$(date +%Y%m%d).tar.gz.gpg
```

### Restaurar backup

```bash
# Backup simples
cp db/omega.db.backup-20240104 db/omega.db

# Backup criptografado
gpg -d omega-backup-20240104.tar.gz.gpg | tar xz
```

## 📋 Logs e Monitoramento

### Ver últimos logs

```bash
# Últimos 20 logs (padrão)
python3 omega_manager.py --logs

# Últimos 50 logs
python3 omega_manager.py --logs --log-limit 50

# Últimos 100 logs
python3 omega_manager.py --logs --log-limit 100
```

### Filtrar logs por nível

```bash
# Apenas erros
python3 omega_manager.py --logs --log-level ERROR

# Apenas avisos
python3 omega_manager.py --logs --log-level WARNING

# Apenas informações
python3 omega_manager.py --logs --log-level INFO
```

### Logs em tempo real (simulado)

```bash
# Monitorar banco de dados
watch -n 5 'python3 omega_manager.py --logs --log-limit 10'
```

### Exportar logs para arquivo

```bash
python3 omega_manager.py --logs --log-limit 1000 > logs-export-$(date +%Y%m%d).txt
```

## 🔗 Integração com Scripts

### Uso em script de build

```bash
#!/bin/bash
# build-and-sign.sh

set -euo pipefail

echo "🏗️  Building APK..."
./gradlew clean assembleRelease

echo "🔒 Signing APK with Zipraf Omega..."
cd zipraf_omega
python3 omega_manager.py --sign ../app/build/outputs/apk/release/app-release-unsigned.apk

echo "✅ Done! Signed APK ready."
python3 omega_manager.py --logs --log-limit 5
```

### Uso como biblioteca Python

```python
#!/usr/bin/env python3
# custom-signer.py

from omega_manager import ZipRafOmegaManager
import sys

def main():
    manager = ZipRafOmegaManager()
    
    # Verificar configuração
    if not manager.get_config('github_pat'):
        print("❌ Sistema não configurado. Execute --setup primeiro.")
        sys.exit(1)
    
    # Assinar APK
    apk_path = sys.argv[1] if len(sys.argv) > 1 else 'app-unsigned.apk'
    
    print(f"🔒 Assinando: {apk_path}")
    success = manager.sign_apk(apk_path)
    
    if success:
        print("✅ Assinatura concluída!")
        manager.log('INFO', f'Script custom-signer.py assinou {apk_path}', 'custom')
    else:
        print("❌ Falha na assinatura!")
        sys.exit(1)

if __name__ == '__main__':
    main()
```

### Integração com Makefile

```makefile
# Makefile

.PHONY: build sign status

build:
	./gradlew assembleRelease

sign: build
	cd zipraf_omega && \
	python3 omega_manager.py --sign ../app/build/outputs/apk/release/app-release-unsigned.apk

status:
	cd zipraf_omega && python3 omega_manager.py --status

logs:
	cd zipraf_omega && python3 omega_manager.py --logs

all: sign status
```

Uso:
```bash
make sign
make status
make logs
```

### Integração com Docker

```dockerfile
# Dockerfile para build e assinatura

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    openjdk-17-jdk \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

COPY . .

RUN cd zipraf_omega && pip3 install -r requirements.txt

# Configurar via variáveis de ambiente
ENV OMEGA_GITHUB_PAT=""
ENV OMEGA_KEYSTORE_PASSWORD=""
ENV OMEGA_KEY_PASSWORD=""
ENV OMEGA_KEY_ALIAS=""

CMD ["bash", "-c", "cd zipraf_omega && python3 omega_manager.py --sign ../app-unsigned.apk"]
```

## 🎯 Casos de Uso Avançados

### 1. Assinatura Condicional

```bash
#!/bin/bash
# Assinar apenas se o build for bem-sucedido

if ./gradlew assembleRelease; then
    echo "✅ Build OK. Assinando..."
    cd zipraf_omega
    python3 omega_manager.py --sign ../app/build/outputs/apk/release/app-release-unsigned.apk
else
    echo "❌ Build falhou. Abortando assinatura."
    exit 1
fi
```

### 2. Assinatura com Validação

```bash
#!/bin/bash
# Assinar e validar APK

cd zipraf_omega
python3 omega_manager.py --sign ../app/build/outputs/apk/release/app-release-unsigned.apk

# Verificar assinatura
apksigner verify --verbose ../app/build/outputs/apk/release/app-release-signed.apk

if [ $? -eq 0 ]; then
    echo "✅ APK assinado e verificado com sucesso!"
else
    echo "❌ Falha na verificação da assinatura!"
    exit 1
fi
```

### 3. Assinatura com Notificação

```bash
#!/bin/bash
# Assinar e enviar notificação

cd zipraf_omega

if python3 omega_manager.py --sign ../app/build/outputs/apk/release/app-release-unsigned.apk; then
    # Notificar sucesso
    curl -X POST https://api.telegram.org/botTOKEN/sendMessage \
        -d chat_id=CHAT_ID \
        -d text="✅ APK assinado com sucesso pelo Zipraf Omega!"
else
    # Notificar falha
    curl -X POST https://api.telegram.org/botTOKEN/sendMessage \
        -d chat_id=CHAT_ID \
        -d text="❌ Falha ao assinar APK com Zipraf Omega!"
fi
```

## 🛠️ Troubleshooting

### Erro: "Senha mestra incorreta"

```bash
# Reconfigurar o sistema
python3 omega_manager.py --setup
```

### Erro: "Keystore não encontrado"

```bash
# Verificar caminho
python3 << EOF
from omega_manager import ZipRafOmegaManager
manager = ZipRafOmegaManager()
print("Keystore:", manager.get_config('keystore_file'))
EOF

# Atualizar caminho se necessário
python3 omega_manager.py --setup
```

### Banco de dados corrompido

```bash
# Backup do banco atual
mv db/omega.db db/omega.db.corrupted

# Recriar
python3 omega_manager.py --setup
```

## 📚 Referências

- **README.md**: Documentação completa
- **ativar.txt**: Documentação de orquestração
- **scripts/sign_apk.sh**: Script de assinatura subjacente
- **workflow-integration.example.yml**: Exemplo de integração CI/CD

---

**ZIPRAF OMEGA** - Assinatura simplificada, automação completa 🪉
