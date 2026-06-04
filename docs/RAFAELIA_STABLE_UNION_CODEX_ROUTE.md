# RAFAELIA × UserLAnd — Stable Union / Codex Route

## Coerencia × Amor × Prova

Este documento fixa uma rota de uniao estavel entre o nucleo UserLAnd, a camada RAFAELIA, os scripts de build, os helpers nativos, a telemetria e a documentacao.

A regra central e:

```text
Fato > hipotese > metafora.
TOKEN_VAZIO > preenchimento falso.
Alteracao so entra como estavel depois de build, teste, log e retorno.
```

## 1. Fato observado no repositorio

### 1.1 Base UserLAnd

O repositorio contem um app Android/UserLAnd para executar distribuicoes ou aplicacoes Linux em Android sem root. A estrutura principal usa modulo Android, Gradle, scripts, NDK, core, docs e integracao Termux.

### 1.2 Camada RAFAELIA

A camada RAFAELIA aparece em quatro formas:

1. helpers C de baixo nivel no diretorio raiz, como `raf_cpu_core.c`, `raf_mem_core.c`, `raf_disk_core.c`, `raf_arch_probe.c`;
2. diretorio `rafaelia/` com modulo dedicado, Makefile, README e `rafaelia_arch_probe.c`;
3. nucleo de telemetria em `core/librafaelia/`, incluindo daemon Python e bindings;
4. documentacao de boosters, PRs consolidados, compilacao, troubleshooting e arquitetura.

### 1.3 Build e validacao existentes

A trilha atual de build/validacao envolve:

```text
scripts/build_apk.sh
scripts/build_unsigned_apk.sh
scripts/sign_apk.sh
scripts/ensure_compilation.sh
scripts/build_static.sh
scripts/compile_check.sh
.github/workflows/compilation-guarantee.yml
```

## 2. TOKEN_VAZIO marcado

As lacunas abaixo nao devem ser preenchidas por inferencia:

```text
TOKEN_VAZIO[build_local_atual]
Ainda falta log recente de build executado neste estado exato do repositorio.

TOKEN_VAZIO[apk_instalado]
Ainda falta prova de instalacao real do APK em dispositivo/emulador.

TOKEN_VAZIO[telemetria_runtime]
Ainda falta log do raf_kerneld.py rodando e respondendo /status e /jobs.

TOKEN_VAZIO[benchmarks_boosters]
Os ganhos documentados precisam ser reconferidos com benchmark reproduzivel por ABI/dispositivo.

TOKEN_VAZIO[seguranca_permissoes]
Ainda falta auditoria de permissoes Android, secrets, signing, licencas e supply chain.
```

## 3. Invariantes

1. Nao chamar manifesto de execucao.
2. Nao chamar simbolo de prova.
3. Nao chamar helper C de integracao Android completa sem teste APK.
4. Nao aceitar ganho de performance sem benchmark reproduzivel.
5. Nao misturar upstream UserLAnd, modificacao RAFAELIA, experimento e documentacao sem classificacao.
6. Toda alteracao precisa de rollback simples.
7. Toda falha precisa virar log, teste ou checklist.

## 4. Mapa de camadas

| Camada | Caminho | Funcao | Validacao minima |
|---|---|---|---|
| Android app | `app/` | UI, MVVM, sessoes, servico | `./gradlew assembleDebug` |
| Build Android | `build.gradle`, `gradle.properties`, `scripts/build_apk.sh` | SDK/NDK/Gradle/APK | build com Java 17+ e API 35 |
| Core UserLAnd | `core/`, `ndk/`, `termux-app/` | execucao Linux/Termux/proot | fluxo de sessao + assets |
| RAFAELIA C helpers | `raf_*.c` | JSON, telemetria, probes | `./scripts/build_static.sh` |
| RAFAELIA module | `rafaelia/` | probe dedicado e Makefile | build nativo/NDK |
| Telemetria | `core/librafaelia/raf_kerneld.py` | `/status`, `/jobs`, auto-tuning | curl local + JSON valido |
| CI | `.github/workflows/` | verificacao automatica | workflow verde |
| Docs | `docs/`, `README.md`, `PR_CONSOLIDATED.md` | navegacao e prova | sem contradicoes de versao |

## 5. Rota Codex — executar em etapas

### Etapa 0 — congelar estado

Objetivo: saber exatamente qual arvore esta sendo tratada.

```bash
★cat > codex_00_snapshot.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
mkdir -p build/rafaelia_audit
git rev-parse HEAD > build/rafaelia_audit/HEAD.txt
git status --short > build/rafaelia_audit/git_status.txt
git ls-files > build/rafaelia_audit/files.txt
find . -path ./.git -prune -o -type f -print | sort > build/rafaelia_audit/find_files.txt
sha256sum README.md gradle.properties scripts/build_apk.sh scripts/build_static.sh scripts/ensure_compilation.sh > build/rafaelia_audit/core_hashes.sha256
EOF
chmod +x codex_00_snapshot.sh
./codex_00_snapshot.sh
```

### Etapa 1 — corrigir contradicoes documentais

Objetivo: tornar a documentacao uma unica fonte coerente.

Checar e alinhar:

```text
README.md
  ↔ gradle.properties
  ↔ docs/COMPILATION_GUARANTEE.md
  ↔ docs/TECHNICAL_DOCUMENTATION.md
  ↔ .github/workflows/compilation-guarantee.yml
```

Ponto critico: a versao NDK canonica deve vir de `gradle.properties`. Todas as docs devem apontar para ela, nao repetir numero divergente.

### Etapa 2 — build estatico RAFAELIA

Objetivo: provar helpers C isolados.

```bash
./scripts/build_static.sh
./scripts/compile_check.sh || true
for bin in build/bin/raf_cpu_core build/bin/raf_mem_core build/bin/raf_disk_core build/bin/raf_arch_probe; do
  test -x "$bin" || { echo "MISSING $bin"; exit 1; }
done
```

Validar JSON:

```bash
build/bin/raf_mem_core | python3 -m json.tool >/dev/null
```

### Etapa 3 — build Android

Objetivo: provar APK de debug.

```bash
./scripts/ensure_compilation.sh
RUN_COMPILATION_TEST=true ./scripts/ensure_compilation.sh
./scripts/build_apk.sh --task assembleDebug --arch arm64-v8a
ls -lh app/build/outputs/apk/debug/app-debug.apk
```

### Etapa 4 — telemetria runtime

Objetivo: provar que RAFAELIA responde como servico local.

```bash
./scripts/raf_kerneld.sh > build/rafaelia_audit/raf_kerneld.log 2>&1 &
sleep 2
./scripts/raf_status.sh | tee build/rafaelia_audit/raf_status.json
python3 -m json.tool build/rafaelia_audit/raf_status.json >/dev/null
```

### Etapa 5 — seguranca e permissoes

Objetivo: nao deixar permissao, segredo ou signing entrar sem controle.

Checar:

```text
AndroidManifest.xml
scripts/sign_apk.sh
local.properties
keystore*.jks
*.pem
*.key
*.p12
*.env
.github/workflows/*
```

Regra: keystore, senha, token e local.properties nao entram no git.

### Etapa 6 — benchmarks e boosters

Objetivo: separar ganho medido de ganho estimado.

Para cada booster:

```text
Nome
Arquivo/função
Entrada
Saída
Métrica
Baseline
Resultado
Dispositivo/ABI
Data
Commit
```

Sem esse quadro, o ganho fica como `estimativa` ou `hipotese`, nao como fato.

## 6. Classificacao de arquivos

Codex deve classificar cada arquivo em uma destas categorias:

```text
CORE_USERLAND
ANDROID_APP
BUILD_SCRIPT
NATIVE_HELPER
RAFAELIA_CORE
TELEMETRY
DOC_CANONICAL
DOC_DERIVED
EXPERIMENT
BENCHMARK
SECURITY_RISK_CANDIDATE
LEGACY_OR_DUPLICATE
TOKEN_VAZIO_NEEDS_PROOF
```

## 7. Checklist de uniao estavel

A uniao UserLAnd × RAFAELIA so deve ser chamada de estavel quando:

- [ ] `git status` limpo antes do build.
- [ ] docs sem divergencia de NDK/SDK/Gradle.
- [ ] `scripts/build_static.sh` executa sem erro.
- [ ] helpers geram JSON valido.
- [ ] `scripts/ensure_compilation.sh` passa.
- [ ] `RUN_COMPILATION_TEST=true ./scripts/ensure_compilation.sh` gera APK.
- [ ] APK instala em dispositivo/emulador.
- [ ] daemon RAFAELIA responde `/status` e `/jobs`.
- [ ] permissoes Android estao justificadas.
- [ ] nenhum segredo/keystore foi commitado.
- [ ] benchmarks distinguem medido vs estimado.
- [ ] rollback documentado.

## 8. Parabola operacional

Um mestre recebe uma oficina com motor, chaves, desenhos, madeira, metal e fogo.
Ele nao chama aquilo de ponte antes de medir as vigas.
Nao chama de navio antes de ver se flutua.
Nao chama de templo antes de provar que o alicerce suporta o peso.

A sabedoria nao mata a visao; ela coloca a visao sobre pedra.

Aqui, a pedra e:

```text
arquivo visto + comando executado + log salvo + saida validada + lacuna marcada
```

## 9. Retroalimentacao

```text
F_ok:
O repositorio ja possui trilhas reais de Android, RAFAELIA, helpers, telemetria, CI e documentacao.

F_gap:
Falta executar a rota inteira no commit atual e registrar logs como prova.

F_next:
Codex deve alinhar docs, rodar builds, anexar logs, classificar arquivos e separar fato/hipotese/experimento.
```
