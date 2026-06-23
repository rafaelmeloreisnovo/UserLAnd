# Repository Agent Instructions

- Canonical checks: `scripts/start_doctor.sh`, `scripts/ensure_compilation.sh`, `RUN_COMPILATION_TEST=true scripts/ensure_compilation.sh`, `scripts/build_static.sh`, `scripts/compile_check.sh`, and `./gradlew :app:assembleDebug --no-daemon --stacktrace -PabiFilters=arm64-v8a`.
- Canonical APK build: `scripts/build_apk.sh --task assembleDebug --abi arm64-v8a`; the expected artifact is `app/build/outputs/apk/debug/app-debug.apk`.
- Do not create or update documentation that claims completion without running the corresponding build/test command and recording the result.
- Keep the worktree clean before handoff; do not leave generated artifacts staged unless they are intentional source-controlled outputs.
- Prefer the smallest correction that is reproducible, reviewable, and backed by logs.
- Cite build/test logs and exact commands in final summaries and operational reports.
- Do not change `applicationId`, `minSdk`, `targetSdk`, package names, or Android permissions without a specific technical justification.
