# Como gerar APKs no GitHub Actions

Este repositório consegue compilar APKs automaticamente no GitHub Actions, sem precisar de PC local ou de keystore. Os binários são gerados unsigned usando `-PuserlandUnsignedApk=true`.

## Executando o workflow
- Abra a aba **Actions** no GitHub.
- Selecione o workflow **“Build Android APKs (unsigned)”**.
- Clique em **Run workflow** (ou aguarde um `push`, `pull_request` ou `workflow_dispatch`, que também disparam a pipeline).

## Baixando o artifact
- Ao final da execução, abra o run correspondente em **Actions**.
- Na seção **Artifacts**, baixe o arquivo **`userland-unsigned-apks`**. Ele contém os APKs gerados (debug e release).

## Debug x Release
- **Debug**: APK sem otimizações, indicado para testes rápidos. Está `debuggable` e **unsigned**.
- **Release**: APK otimizado (minify/shrink conforme configurado). Também é **unsigned** e pronto para ser assinado manualmente se necessário.

## Instalando o APK no Android
1. Copie o APK para o dispositivo (USB, cloud, etc.).
2. Em **Configurações > Segurança/Privacidade**, habilite **Instalar apps de fontes desconhecidas** (ou “Permitir desta fonte” para o app que fará a instalação).
3. Abra o APK e conclua a instalação.
