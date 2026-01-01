#!/usr/bin/env bash
set -euo pipefail

### ===== Config fixo =====
GITHUB_USER="rafaelmeloreisnovo"
GITHUB_EMAIL="rafaelmeloreisnovo@gmail.com"
REPO="UserLAnd"
REPO_URL="https://github.com/${GITHUB_USER}/${REPO}.git"
WORKDIR="${HOME}/_work_${REPO}_patch"

### ===== Helpers =====
say() { printf "\n\033[1m%s\033[0m\n" "$*"; }
die() { echo "ERRO: $*" >&2; exit 1; }

need() {
  command -v "$1" >/dev/null 2>&1 || die "comando ausente: $1 (instale e rode de novo)"
}

need bash
need git
need python3

say "Config do Git (local/global) 🧩"
read -r -p "Seu nome para commits (ex: Rafael Melo Reis) [ENTER para default]: " GIT_NAME
GIT_NAME="${GIT_NAME:-Rafael Melo Reis}"

git config --global user.name "$GIT_NAME"
git config --global user.email "$GITHUB_EMAIL"
git config --global pull.rebase false

say "Digite seu PAT do GitHub (não aparece na tela) 🔐"
read -r -s -p "PAT: " GITHUB_PAT
echo
[ -n "$GITHUB_PAT" ] || die "PAT vazio."

# Cria um GIT_ASKPASS temporário (não salva token no repo)
ASKPASS_DIR="$(mktemp -d)"
ASKPASS_SH="${ASKPASS_DIR}/askpass.sh"
cat > "${ASKPASS_SH}" <<'EOF'
#!/usr/bin/env bash
# Git chama isso pedindo username/password.
case "$1" in
  *Username*) echo "${GIT_USERNAME:-x-access-token}" ;;
  *Password*) echo "${GIT_PASSWORD:-}" ;;
  *) echo "" ;;
esac
EOF
chmod +x "${ASKPASS_SH}"

export GIT_ASKPASS="${ASKPASS_SH}"
export GIT_TERMINAL_PROMPT=0
export GIT_USERNAME="${GITHUB_USER}"
export GIT_PASSWORD="${GITHUB_PAT}"

say "Preparando diretório de trabalho 📁"
rm -rf "${WORKDIR}"
mkdir -p "${WORKDIR}"
cd "${WORKDIR}"

say "Clonando ${REPO_URL} 🧬"
git clone "${REPO_URL}"
cd "${REPO}"

say "Criando Godex (M0) 📚"
mkdir -p docs/GODEX tools

# Arquivos do Godex (idempotente)
for f in 00_scope.md 01_architecture.md 02_build_release.md 03_security.md 04_workflows.md 05_tests.md; do
  if [ ! -f "docs/GODEX/${f}" ]; then
    cat > "docs/GODEX/${f}" <<EOF
# ${f}

## Objetivo
(TODO)

## Conteúdo
(TODO)
EOF
  fi
done

# doctor.sh (mínimo útil)
cat > tools/doctor.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

echo "== JSul/UserLAnd Doctor =="

need() { command -v "$1" >/dev/null 2>&1 || { echo "FALTA: $1"; exit 1; }; }

need git
need java
need python3

echo "java: $(java -version 2>&1 | head -n 1)"
echo "git:  $(git --version)"
echo "py:   $(python3 --version)"

if [ -f "./gradlew" ]; then
  echo "gradlew: OK"
else
  echo "gradlew: AUSENTE (verifique repo)"
fi

echo "OK ✅"
EOF
chmod +x tools/doctor.sh

say "Workflows: criando/atualizando RAFAELIA Core CI ⚙️"
mkdir -p .github/workflows

# Workflow mais robusto (matriz + artifacts + NDK opcional)
cat > .github/workflows/rafaelia-core-ci.yml <<'EOF'
name: RAFAELIA Core CI

on:
  push:
    branches: [ master, main ]
    paths:
      - "rafaelia/**"
      - "raf_*"
      - "rafael_*"
      - "scripts/**"
      - ".github/workflows/rafaelia-core-ci.yml"
  pull_request:
    paths:
      - "rafaelia/**"
      - "raf_*"
      - "rafael_*"
      - "scripts/**"
      - ".github/workflows/rafaelia-core-ci.yml"

jobs:
  build-core:
    runs-on: ubuntu-latest
    strategy:
      fail-fast: false
      matrix:
        include:
          - name: native-static
            ndk: false
            static: true
            arch: native
          - name: android-arm64
            ndk: true
            static: false
            arch: arm64
          - name: android-x86_64
            ndk: true
            static: false
            arch: x86_64

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install build tools
        run: |
          sudo apt-get update
          sudo apt-get install -y build-essential clang make

      - name: Set up Android NDK
        if: matrix.ndk == true
        uses: nttld/setup-ndk@v1
        with:
          ndk-version: r26b

      - name: Build static RAFAELIA tools
        shell: bash
        run: |
          set -euo pipefail
          chmod +x scripts/build_static.sh
          export STATIC="${{ matrix.static }}"
          export NDK_ARCH="${{ matrix.arch }}"
          if [ "${{ matrix.ndk }}" = "true" ]; then
            # setup-ndk expõe ANDROID_NDK_HOME
            export ANDROID_NDK_HOME="${ANDROID_NDK_HOME}"
          fi
          ./scripts/build_static.sh

      - name: Sanity compile core C files
        shell: bash
        run: |
          set -euo pipefail
          CC=clang
          CFLAGS="-std=c11 -Wall -Wextra -Werror -O2"
          for f in raf_cpu_core.c raf_dev_debug_perf.c raf_userland_debug.c rafael_userland_debug_runner.c; do
            if [ -f "$f" ]; then
              $CC $CFLAGS -c "$f"
            fi
          done

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: rafaelia-${{ matrix.name }}
          path: |
            build/bin/**
            build/**.json
          if-no-files-found: warn
EOF

say "Patch low-level: reforços mínimos em rafaelia/Makefile e scripts/build_static.sh 🛠️"
python3 - <<'PY'
import pathlib, re

def patch_file(path: str, apply_fn):
    p = pathlib.Path(path)
    if not p.exists():
        return
    s = p.read_text(encoding="utf-8", errors="ignore")
    ns = apply_fn(s)
    if ns != s:
        p.write_text(ns, encoding="utf-8")

# 1) Makefile: adicionar STATIC + gerar build/rafaelia_arch.json (sem quebrar)
def patch_makefile(s: str) -> str:
    if "STATIC ?=" not in s:
        s = "STATIC ?= 1\n\n" + s
    # injeta LDFLAGS -static se STATIC=1 (sem duplicar)
    if "ifeq ($(STATIC),1)" not in s:
        insert = "\nifeq ($(STATIC),1)\n  LDFLAGS += -static\nendif\n"
        # tenta inserir após CFLAGS/LDFLAGS se existir, senão no topo
        m = re.search(r"(CFLAGS.*\n(?:.*\n){0,5}?LDFLAGS.*\n)", s)
        if m:
            s = s[:m.end()] + insert + s[m.end():]
        else:
            s = insert + s
    # gerar JSON após build do binário (somente se existir regra do probe)
    if "rafaelia_arch.json" not in s:
        # procura regra do target rafaelia_arch_probe
        s = re.sub(
            r"(\$\(BIN_DIR\)/rafaelia_arch_probe:[^\n]*\n(?:.*\n){1,8}?\t@?\$?\(CC\)[^\n]*\n)",
            r"\1\t@mkdir -p build && ./$@ > build/rafaelia_arch.json || true\n",
            s,
            count=1
        )
    return s

# 2) build_static.sh: aceitar ANDROID_NDK_ROOT e log
def patch_build_static(s: str) -> str:
    if "ANDROID_NDK_ROOT" not in s:
        s = re.sub(
            r"(choose_ndk_toolchain\(\)\s*\{\s*)",
            r"\1\n  # Accept ANDROID_NDK_ROOT as well\n  if [[ -z \"${NDK_HOME:-}\" && -n \"${ANDROID_NDK_ROOT:-}\" ]]; then\n    export NDK_HOME=\"$ANDROID_NDK_ROOT\"\n  fi\n",
            s,
            count=1
        )
    if "build/build.log" not in s:
        # adiciona logger simples perto do topo
        s = re.sub(
            r"(set -euo pipefail\s*)",
            r"\1\n\nLOG_FILE=\"${ROOT_DIR:-.}/build/build.log\"\nmkdir -p \"${ROOT_DIR:-.}/build\" || true\nlog(){ echo \"$@\" | tee -a \"$LOG_FILE\"; }\n",
            s,
            count=1
        )
    return s

patch_file("rafaelia/Makefile", patch_makefile)
patch_file("scripts/build_static.sh", patch_build_static)
PY

say "Commitando mudanças 🧾"
git add docs/GODEX tools/doctor.sh .github/workflows/rafaelia-core-ci.yml rafaelia/Makefile scripts/build_static.sh || true

if git diff --cached --quiet; then
  say "Nada para commitar (repo já estava nesse estado) ✅"
else
  git commit -m "M0: add Godex + doctor; improve RAFAELIA Core CI; low-level hardening"
fi

say "Fazendo push para origin (via PAT, sem salvar token) 🚀"
git push origin HEAD

say "Pronto ✅  (repo atualizado)."
say "Agora rode:  ./tools/doctor.sh   e verifique Actions no GitHub."
