# Operational Completion Report

This report records the operational finalization work performed against the local repository state. It reflects commands that were executed and code paths that were verified; it is not a promise of untested behavior.

## 1. What was corrected

- Added root `AGENTS.md` with canonical build/test commands and repository operating rules.
- Restored RAFAELIA Core CI compatibility by adding executable wrappers:
  - `scripts/build_core.sh` -> `scripts/build_static.sh`
  - `scripts/test_core.sh` -> `scripts/compile_check.sh`
- Updated CircleCI Android toolchain references to Android SDK 35, build-tools 35.0.0, and NDK 27.3.13750724.
- Fixed `scripts/build_static.sh` so the optional `rafaelia_arch_probe_v2` helper links with `rafaelia/rafaelia_hw_map.c`.
- Kept `scripts/build_static.sh` host-runnable by default for local smoke tests, while allowing explicit Android helper cross-compilation when packaging Termux assets.
- Updated `termux-app/package_static_bins.sh` to request arm64 Android helper binaries by default for APK assets.
- Improved `scripts/start_doctor.sh` parsing of Gradle `compileSdk 35` syntax.
- Improved `scripts/ensure_compilation.sh` to detect the NDK installed under `local.properties` and to run the full compilation gate through the arm64-v8a APK path.

## 2. What compiles

- Android debug APK for arm64-v8a compiled successfully.
- Native C/RAFAELIA helpers compile with the host compiler via `scripts/build_static.sh` and `scripts/compile_check.sh`.
- Termux packaged helper binaries are cross-compiled for Android arm64 when `termux-app/package_static_bins.sh` runs with the locally installed NDK.

## 3. APK artifact

- APK path: `app/build/outputs/apk/debug/app-debug.apk`
- Verified size: `15M`
- `file app/build/outputs/apk/debug/app-debug.apk` could not be executed because the container does not have the `file` command installed.

## 4. Commands executed

| Command | Result | Notes |
| --- | --- | --- |
| `git status` | PASS | Initial status showed pre-existing untracked `add/Cargo.lock`. |
| `git branch --show-current` | PASS | Branch: `work`. |
| `ls -la` | PASS | Repository root inspected. |
| `find .github/workflows -maxdepth 1 -type f -print` | PASS | Workflow files listed. |
| `find scripts -maxdepth 1 -type f -print \| sort` | PASS | Script files listed. |
| `grep -RniE "build_core.sh\|test_core.sh\|compile_check.sh\|build_static.sh\|assembleDebug\|compileSdk\|targetSdk\|ndkVersion" .github scripts build.gradle app/build.gradle gradle.properties .circleci 2>/dev/null \|\| true` | PASS | Confirmed missing `build_core.sh`/`test_core.sh` references and SDK/NDK CI drift. |
| `scripts/start_doctor.sh` | PASS | Startup/bootstrap/proot sanity checks passed. |
| `scripts/ensure_compilation.sh` | PASS after SDK bootstrap | Dependency resolution passed after Android SDK/NDK were installed by the build script. |
| `RUN_COMPILATION_TEST=true scripts/ensure_compilation.sh` | PASS | Full arm64-v8a debug compilation and native compile check passed. |
| `scripts/build_apk.sh --task assembleDebug --abi arm64-v8a` | PASS | Generated `app-debug.apk`. |
| `ls -lh app/build/outputs/apk/debug/*.apk` | PASS | Confirmed `app-debug.apk` exists and is 15M. |
| `file app/build/outputs/apk/debug/app-debug.apk \|\| true` | WARNING | `file` utility is absent in this container. |
| `scripts/build_static.sh` | PASS | Host helper binaries compiled. |
| `scripts/compile_check.sh` | PASS | Required helper binaries exist and are executable. |
| `ls -lh build/bin/` | PASS | Helper binaries listed. |
| `cat /proc/stat \| build/bin/raf_cpu_core \|\| true` | PASS | Emitted CPU JSON. |
| `build/bin/raf_mem_core \|\| true` | PASS | Emitted memory JSON. |
| `build/bin/raf_disk_core \|\| true` | PASS | Emitted diskstats JSON. |
| `termux-app/package_static_bins.sh` | PASS | Copied Android arm64 helper binaries and wrappers into Termux assets. |
| `find termux-app/terminal-term/src/main/assets/bin -maxdepth 1 -type f -name 'raf_*' -ls` | PASS | Confirmed expected `raf_*` binaries/wrappers in assets. |

## 5. Remaining failures or warnings

- `file app/build/outputs/apk/debug/app-debug.apk` could not run because the `file` package is missing from the container.
- Android build emits non-blocking warnings for deprecated APIs and `android:extractNativeLibs`; these warnings did not block APK generation.
- The full unfiltered all-ABI `assembleDebug` path previously hit an ARM32 NDK flag issue (`-mthumb-interwork`). The verified operational gate is arm64-v8a, matching the requested target path.

## 6. Next real installation/test step

Install the generated debug APK on an authorized Android device or emulator:

```sh
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Then create/start a UserLAnd filesystem/session and verify the Android -> BusyBox -> PRoot -> filesystem/session path on-device, because process launch and proot behavior require Android runtime verification.

## 7. Verification statement

Documentation in this report reflects code paths verified by the commands above. Any behavior not covered by those commands is intentionally listed as a remaining on-device validation step, not as a completed guarantee.
