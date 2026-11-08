# RAFAELIA / UserLAnd — freestanding Linux ABI slice V1

**State:** independent kernel ABI smoke test; not an APK, a PRoot replacement or an Android application. Its sources are isolated from the existing Android/Gradle build to avoid regressions.

## Source and build contract

`src/entry.c` → `raf_linux_write()` → Linux kernel syscall → `raf_linux_exit()`. One special-purpose syscall module; no libc, external headers, JNI, SDK, NDK, Java or Gradle *in the runtime* or build invocation. The Clang compiler, lld linker and minimal shell are still build-time tools.

| ABI | Clang triple | Linux write / exit | Test |
|---|---|---|---|
| ARMv7 in ARM mode | armv7a-linux-gnueabi | r7=4 / r7=1; svc #0 | ELF32 static, no interpreter/libraries/undefined |
| AArch64 | aarch64-linux-gnu | x8=64 / x8=93; svc #0 | ELF64 static, no interpreter/libraries/undefined |
| Linux x86-64 host | x86_64-linux-gnu | rax=1 / rax=60; syscall | static ELF plus host execution |

Build and adversarial gate:

```sh
sh authorial-freestanding/build.sh all
sh authorial-freestanding/tests/gate.sh
```

Gate requires `clang`, `ld.lld`, `readelf`, `nm` and `grep`. It validates architecture, static linkage, absence of unresolved symbols, deterministic stdout on x86-64, and exit code 74 on a forced write failure (when /dev/full exists). The build does not download dependencies. A cross-compiled ELF without a dynamic interpreter **still depends on Linux kernel ABI and execution policy**. There is no guarantee that binary execution on Android is permitted.

## Nonclaims and future integration gate

This does not replicate PRoot, ptrace translation, PTYs, installation, Android packaging, SELinux permissions, root filesystem setup or app UI. ARM binaries were compiled/linker-checked but **not run on physical devices**. No `app/`, Gradle, permissions or legacy sources were modified. Complete device gate requires real ARMv7 Motorola + AArch64 execution, ABI/SELinux outcome, and implementation-level comparisons. Any future integration must be separately tested and reversible.

## Custody

The existing `rafaelia/LICENSE` is the user's intended authorial-module license; see `LICENSE.md` and `PROVENANCE.md`. No inherited UserLAnd/PRoot code is relicensed. Rollback is closing the unmerged PR or reverting its commit. Source ≠ linked artifact ≠ host execution ≠ device evidence ≠ PRoot replacement.
