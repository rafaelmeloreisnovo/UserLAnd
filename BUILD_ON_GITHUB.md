# Como gerar APKs no GitHub Actions

Este repositório agora pode compilar APKs automaticamente usando o GitHub Actions, sem precisar de um PC local ou de uma keystore.

## Executando o workflow
- Abra a aba **Actions** no GitHub.
- Selecione o workflow **“Build Android APKs (unsigned)”**.
- Clique em **Run workflow** (ou aguarde um `push`/`pull_request`, que também acionam a pipeline).

## Baixando o artifact
- Ao final da execução, abra o run correspondente em **Actions**.
- Na seção **Artifacts**, baixe o arquivo **`userland-unsigned-apks`**. Ele contém os APKs gerados (debug e release).

## Debug x Release
- **Debug**: APK sem otimizações, indicado para testes rápidos. Já vem marcado como `debuggable`.
- **Release**: APK otimizado (minify/shrink). Também está **unsigned** (gerado com `-PuserlandUnsignedApk=true`), pronto para ser assinado manualmente caso necessário.

## Instalando o APK no Android
1. Copie o APK para o dispositivo (via USB, cloud, etc).
2. Em **Configurações > Segurança/Privacidade**, habilite **Instalar apps de fontes desconhecidas** (ou “Permitir desta fonte” para o app que fará a instalação).
3. Abra o APK e conclua a instalação.
