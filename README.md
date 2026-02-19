![UserLAnd Feature Graphic](https://raw.githubusercontent.com/CypherpunkArmory/UserLAnd/master/fastlane/metadata/android/en-US/images/featureGraphic.png)

# Welcome to UserLAnd

The easiest way to run a Linux distribution or application on Android.   
Features: 
* Run full linux distros or specific applications on top of Android.
* Install and uninstall like a regular app.
* No root required.

[<img src="https://play.google.com/intl/en_us/badges/images/generic/en-play-badge.png"
     alt="Get it on Google Play"
     height="80">](https://play.google.com/store/apps/details?id=tech.ula)

## 📚 Documentation

### Pull Request & Implementation Documentation
- **[📄 PR Consolidated Documentation](PR_CONSOLIDATED.md)** - ⭐ **NEW!** Complete documentation of all merged pull requests (#1-#17)
- **[📋 Applied PRs List](APPLIED_PRS.md)** - Quick reference to applied pull requests
- **[📖 Consolidation Summary](CONSOLIDATION_SUMMARY.md)** - How PR documentation was consolidated

### Development Documentation
- **[Development Setup Guide](docs/DEVELOPMENT_SETUP.md)** - Complete guide to set up your development environment
- **[Compilation Guarantee](docs/COMPILATION_GUARANTEE.md)** - How to ensure successful compilation in any environment
- **[Build Troubleshooting](docs/BUILD_TROUBLESHOOTING.md)** - Solutions to common build issues
- **[Best Practices](docs/BEST_PRACTICES.md)** - Coding standards and architectural patterns
- **[Common Tasks](docs/COMMON_TASKS.md)** - Quick reference for everyday development tasks
- **[Technical Documentation](docs/TECHNICAL_DOCUMENTATION.md)** - Architecture overview and component map
- **[Workflow Checks & Validation](docs/README_checks.md)** - Documentation for running validation tools and checks locally

### Performance & Optimization Documentation
- **[⚡ Performance Boosters](BOOSTERS.md)** - ⭐ **NEW!** Complete guide to RAFAELIA performance boosters, types, benchmarks, and optimization techniques
- **[⚡ Performance Boosters (PT-BR)](BOOSTERS_PT.md)** - ⭐ **NOVO!** Guia completo dos boosters RAFAELIA, tipos, benchmarks e técnicas de otimização (Português)

### Project Documentation
- **[Contributing Guidelines](CONTRIBUTING.md)** - How to contribute to UserLAnd
- **[Governance Policies](.github/CONTRIBUTING.md)** - Ethical compliance, sign-off requirements, and file protection
- **[Security Policy](SECURITY.md)** - Reporting vulnerabilities and security guidelines
- **[Changelog](CHANGELOG.md)** - Version history and release notes
- **[Vantagens e Ganhos](docs/VANTAGENS_E_GANHOS.md)** - Advantages and estimated gains of RAFAELIA optimizations (PT-BR)
     
## Have a bug report or a feature request?
You can see our templates by visiting our [issue center](https://github.com/CypherpunkArmory/UserLAnd/issues).

## Want to contribute?
See our [CONTRIBUTING](CONTRIBUTING.md) document and [Development Setup Guide](docs/DEVELOPMENT_SETUP.md).

## 🚀 Quick Start

### Build the APK locally
UserLAnd depends on modern Android Gradle Plugin tooling that expects Java 17 or
newer alongside Android SDK platform 35 (Android 15). To build from a clean
machine:

1. Install a Java 17+ JDK and ensure `JAVA_HOME` points to it (or export
  `JAVA_HOME_11_X64` on Windows).
2. Run `scripts/build_apk.sh` to download the Android command-line tools into
  `build/android-sdk`, accept licenses, generate `local.properties`, and invoke
  `assembleDebug`. To limit builds to a specific ABI (for example, arm64-only
  artifacts for Android 15 devices), pass `--abi arm64-v8a`.
3. Pass `--skip-sdk` if you already have an SDK available, or `--skip-build` if
  you only want to bootstrap the SDK without running Gradle.

The native helpers and terminal emulator JNI sources are built against Android
NDK `27.3.13750724`, pinned in `gradle.properties` for reproducibility across CI
and local machines. If you already have that NDK locally, point `ANDROID_NDK_HOME`
or `ndk.dir` in `local.properties` to the install location. To test a newer NDK
release, invoke Gradle with `-PndkVersion=<version>`; builds stay unsigned when
you pair that flag with `scripts/build_unsigned_apk.sh`.

### Build an unsigned APK

To force an unsigned variant (helpful when you want to inject a custom signing
step), use the helper wrapper:

```sh
scripts/build_unsigned_apk.sh             # assembleRelease without signing
scripts/build_unsigned_apk.sh --task :app:bundleRelease --skip-sdk
scripts/build_unsigned_apk.sh --abi arm64-v8a
```

The wrapper forwards `-PunsignedApk=true` to Gradle and also exports
`USERLAND_UNSIGNED_APK=1` for compatibility with the legacy
`-PuserlandUnsignedApk` flag. Gradle prioritizes `unsignedApk`: when present it
skips signing even if release keystore credentials are available. The
`USERLAND_UNSIGNED_APK` variable (or `-PuserlandUnsignedApk`) is only consulted
after that check, so if you have valid signing credentials but omit
`-PunsignedApk=true`, the release build will still be signed.

### Build a sideloadable APK for Android 15 (arm64)

To build an arm64-only APK that can be installed on Android 15 without Play
Store signing, use the debug build (signed with the default debug key) and limit
the ABI list to arm64:

```sh
scripts/build_apk.sh --task assembleDebug --arch arm64-v8a
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

If you need a release APK that stays unsigned (for custom signing steps), use:

```sh
scripts/build_unsigned_apk.sh --arch arm64-v8a
```

Unsigned APKs cannot be installed directly on devices; sign them first or stick
to the debug build for local sideloading.
To generate an arm64-only artifact suitable for Android 15 devices, pass an ABI
filter and then sign locally (not Play Store signing) before installation:

```sh
scripts/build_unsigned_apk.sh --abi arm64-v8a
scripts/sign_apk.sh app/build/outputs/apk/release/app-release-unsigned.apk \
  userland-arm64-signed.apk
adb install userland-arm64-signed.apk
```

The `--abi` flag forwards `-PabiFilters=arm64-v8a` to Gradle, which also limits
the downloaded assets to ARM64.
### Build an unsigned release APK

For offline or sideloaded experiments that must remain unsigned even when
release signing credentials are present, use:

```sh
scripts/build_unsigned_apk.sh
```

This wraps `build_apk.sh`, sets `USERLAND_UNSIGNED_APK=1`, and forwards
`-PunsignedApk=true` to Gradle so the release variant is packaged without a
keystore. The `-PunsignedApk=true` property has priority when release signing
credentials exist; otherwise `USERLAND_UNSIGNED_APK=1` (or `-PuserlandUnsignedApk`)
keeps signing disabled.

## Start using UserLAnd
See our [Getting Started](https://github.com/CypherpunkArmory/UserLAnd/wiki/Getting-Started-in-UserLAnd) page.

## Technical documentation
See [docs/TECHNICAL_DOCUMENTATION.md](docs/TECHNICAL_DOCUMENTATION.md) for a
repository-level architecture overview and component map.

## UserLAnd assets
The assets that UserLAnd depends on and the scripts that build them are contained in other repositories.  

The common assets that are used for all distros and application are found at [CypherpunkArmory/UserLAnd-Assets-Support](https://github.com/CypherpunkArmory/UserLAnd-Assets-Support).  

Distribution or application specific assets are found under CypherpunkArmory/UserLAnd-Assets-(__Distribution/App__). For example, our Debian specific assets can be found at [CypherpunkArmory/UserLAnd-Assets-Debian](https://github.com/CypherpunkArmory/UserLAnd-Assets-Debian)

## Static musl/clang utilities

We ship tiny, static helpers compiled with musl/clang and invoked by thin shell scripts (no Python wrappers).

* `raf_bitraf_vector.c` é um vetorizar minimalista (C puro) que mapeia buffers
  arbitrários de entrada — incluindo pixels crus — para um espaço BITRAF de
  1024 estados com duas paridades internas. Ele serve como módulo de
  ultra-baixo nível para reduzir dependências externas em pipelines de
  processamento.

Build them all in one reproducible step:

```sh
./scripts/build_static.sh
```

Install/copy them so they are reachable on your `$PATH`:

```sh
# Local install for the UserLAnd tree
./scripts/install_static.sh

# Package into the Termux APK assets (used during app builds)
./termux-app/package_static_bins.sh
```

### JSON contracts

All helpers emit stable JSON payloads so monitoring tools can parse the same schema across environments.

* `raf_cpu_core` reads `/proc/stat` from stdin and prints:

  ```json
  {
    "timestamp_ms": 1715890000000,
    "cpus": [
      {
        "id": "cpu0",
        "user": 1,
        "nice": 2,
        "system": 3,
        "idle": 4,
        "iowait": 5,
        "irq": 6,
        "softirq": 7,
        "steal": 8,
        "guest": 9,
        "guest_nice": 10
      }
    ]
  }
  ```

* `raf_mem_core` reads `/proc/meminfo` directly and prints a single object with all keys from `/proc/meminfo`:

  ```json
  {
    "timestamp_ms": 1715890000000,
    "meminfo": {
      "MemTotal": 16303424,
      "MemFree": 1024
    }
  }
  ```

* `raf_disk_core` reads `/proc/diskstats` directly and prints:

  ```json
  {
    "timestamp_ms": 1715890000000,
    "disks": [
      {
        "name": "sda",
        "major": 8,
        "minor": 0,
        "reads_completed": 0,
        "reads_merged": 0,
        "sectors_read": 0,
        "time_reading_ms": 0,
        "writes_completed": 0,
        "writes_merged": 0,
        "sectors_written": 0,
        "time_writing_ms": 0,
        "ios_in_progress": 0,
        "time_doing_io_ms": 0,
        "weighted_time_io_ms": 0
      }
    ]
  }
  ```

* `raf_arch_probe` and `rafaelia_arch_probe_v2` now share a single source of truth
  in `core/librafaelia/impl/raf_arch_probe_core.c`. Both emit the same canonical
  JSON contract and differ only by `profile` (`compact` vs `extended`):

  ```json
  {
    "profile": "compact",
    "arch": "aarch64",
    "arch_compiled_for": "aarch64",
    "arch_runtime": "aarch64",
    "word_bits": 64,
    "endianness": "little",
    "simd": "neon",
    "simd_runtime": "neon",
    "fpu": "arm_vfp",
    "fpu_runtime": "present",
    "crypto": true,
    "crypto_runtime": true,
    "cycle_counter": 123456789,
    "used_inline_asm": true,
    "timestamp_ns": 123456789
  }
  ```

  In `compact`, runtime capability fields stay stable but default to compile-time
  values; in `extended`, runtime probing is enabled for deeper detection.

* `raf_bitraf_store` absorbs a raw byte vector (or synthetic gradient), applies
  a programmable verb (`pass`, `shift`, `fractalize`, or `bitwise`), derives
  four fractal fingerprints plus two parity rails, and appends a compact
  ZipRafStore record to `databaseraf.bin` without any external dependencies.
  The `bitwise` verb materializes a 10x10x10 BITRAF matrix (1024-state, base²⁰)
  and passes it through a deterministic kernel so the stored payload keeps the
  same parity rails and four fractal levels mentioned above:

  ```sh
  cc -std=c11 -O2 raf_bitraf_store.c -o raf_bitraf_store
  ./raf_bitraf_store input.raw --verb fractalize
  ```

Each binary has a matching shell entrypoint (`scripts/raf_cpu_core.sh`, `scripts/raf_mem_core.sh`, `scripts/raf_disk_core.sh`) that prefers the local `build/bin` artifacts but also falls back to whatever is on the `$PATH`, ensuring consistent invocation names everywhere.

Two additional wrappers are available for the higher-level tools:

* `scripts/raf_kerneld.sh` launches the daemon binary from `build/bin` (or whichever `raf_kerneld` is on your `$PATH`) and emits a short JSON error plus exit code `127` if the binary is missing.
* `scripts/raf_status.sh` wraps the `raf_status` client the same way, giving a JSON error payload and non-zero exit if the binary cannot be resolved.

### Kernel telemetry daemon

The lightweight daemon `core/librafaelia/raf_kerneld.py` polls the helpers above, keeps a rolling CPU/Mem/Disk state in memory and exposes a JSON API for the Android/UserLAnd app to consume.

Minimum requirements:

* Python 3.10+
* `gcc` (to build `raf_cpu_core`, `raf_mem_core`, `raf_disk_core` via `scripts/build_static.sh`)
* Optional: `librrafaelia_core.so` compiled from `core/include/raf_core.c` (drop it into `core/librafaelia/librrafaelia_core.so` or point `--core-lib` to its location)

Build the helper binaries once (no need to commit the artifacts):

```sh
./scripts/build_static.sh
```

Start the daemon on the internal loopback, pointing it at the helper scripts (they automatically pick up the freshly built binaries):

```sh
python core/librafaelia/raf_kerneld.py --host 127.0.0.1 --port 8027 --interval 2 \
  --cpu-cmd scripts/raf_cpu_core.sh --mem-cmd scripts/raf_mem_core.sh --disk-cmd scripts/raf_disk_core.sh
```

The same pipeline using the wrappers keeps Android/UserLAnd integration scripts simple and consistently named:

```sh
scripts/raf_kerneld.sh --host 127.0.0.1 --port 8027 --interval 2 \
  --cpu-cmd scripts/raf_cpu_core.sh --mem-cmd scripts/raf_mem_core.sh --disk-cmd scripts/raf_disk_core.sh &

# Query the JSON status later on (fails fast with a JSON error + non-zero exit if the binary is unavailable)
scripts/raf_status.sh --host 127.0.0.1 --port 8027
```

HTTP endpoints (all JSON):

* `GET /status` → latest telemetry (CPU load, `MemFree`, disk I/O deltas) plus current RAF core config (`active_cores`, `decay_rate`, universe MB) and thresholds.
* `GET /jobs` → recent auto-tuning actions (core throttling, decay adjustments, universe resize) and collector errors, useful for UI surface in the app.

Client snippets for hitting `/status` from Python, Node.js, and Go are in [docs/telemetry_client_examples.md](docs/telemetry_client_examples.md).

## RAFAELIA kernel v2.2 (bio-quantum demo)

The `rafaelia_kernel_v22.c` sample implements the latest “MELHOR DOS MUNDOS” loop: a branch-light C11 kernel that uses short-circuit recursion, dual toroidal rotations (13/20), a harmonic 137 anchor, and a 42-column ASCII tomogram output. It is meant for quick experiments on both desktop toolchains and Termux builds.

Compile and run a standalone frame generator:

```sh
gcc -std=c11 -O3 -march=native rafaelia_kernel_v22.c -o raf_kernel_v22
./raf_kernel_v22 > kernel_frames.txt
```

You can also include it in the existing bench harness via `RAFAELIA_BENCH_ALL.sh`, which now builds `raf_kernel_v22` alongside the other cores. The binary emits deterministic “frames” (42 columns each) that can be fed into compression/hash tests or piping workflows.

The daemon auto-tunes the core when telemetry crosses thresholds:

* High CPU load throttles `active_cores`; low load ramps back up until `--max-cores`.
* Low `MemFree` halves the allocated universe via `raf_alloc_universe`.
* Sustained disk I/O time nudges `decay_rate` down to cool the workload.

### RAFAELIA "GOD" core example

Para testar o núcleo RAFAELIA mais completo já incluído neste repositório, use o
script de build dedicado e siga o guia `RAF_GOD_CORE_EXAMPLE.md`:

```sh
./scripts/build_raf_god_core.sh
```

O binário interativo/bridge será gerado em `build/bin/raf_god_core_example`.
