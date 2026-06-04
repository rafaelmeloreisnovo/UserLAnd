# UserLAnd Technical Documentation

This document consolidates the architecture and operational flow of the UserLAnd
repository as it exists in this codebase. It focuses on the Android app, the
native helpers, the RAFAELIA telemetry stack, and the scripts that tie the
components together.

## 1. Repository layout (high-level)

- `app/`: Android application module (UI, ViewModels, data layer, service).
- `core/`: RAFAELIA telemetry core bindings and helpers.
- `docs/`: Supporting documentation (APK signing, path handling, telemetry clients).
- `scripts/`: Build helpers, telemetry wrappers, APK tooling, and static binary
  packaging.
- `termux-app/`: Termux packaging and asset integration for static binaries.
- `rafaelia/` and `rafaelia_*.c`: Experimental RAFAELIA cores and C utilities.
- Root-level `raf_*.c` helpers: static musl/clang utilities that emit JSON for
  telemetry or low-level data processing.
- Root-level Android/Gradle files: `build.gradle`, `settings.gradle`,
  `gradlew`, `gradle.properties`.

## 2. Android application architecture (`app/`)

### 2.1 Package organization

The app is organized under `app/src/main/java/tech/ula` with a clean split by
responsibility:

- `ui/`: Fragments and adapters for user-facing screens.
- `viewmodel/`: ViewModels backing the fragments (MVVM pattern).
- `model/`: Entities, DAOs, repositories, remote fetchers, and state machines.
- `utils/`: Device helpers, storage tools, notification utilities, BusyBox
  execution, logging, and permissions.
- `provider/`: Content provider integration (`UlaDocProvider`).
- `ServerService.kt`: Long-lived service that manages active sessions and client
  launch workflows.

### 2.2 UI + ViewModel layer (MVVM)

Fragments (for example `AppsListFragment`, `SessionListFragment`,
`FilesystemListFragment`, and `SettingsFragment`) are paired with corresponding
ViewModels (e.g. `AppsListViewModel`, `SessionListViewModel`,
`FilesystemListViewModel`) under `viewmodel/`. This matches a standard MVVM
approach where UI elements observe LiveData from ViewModels and delegate heavy
logic to repositories or state machines.

The entry activity (`MainActivity.kt`) is currently a minimal CI-focused stub
that uses ViewBinding and wires the toolbar; the real UI flow is driven by
fragments and their ViewModels.

### 2.3 Data layer and persistence

UserLAnd uses Room for persistence via `UlaDatabase`:

- Database class: `app/src/main/java/tech/ula/model/repositories/UlaDatabase.kt`.
- Entities: `Session`, `Filesystem`, and `App`.
- DAOs: `SessionDao`, `FilesystemDao`, `AppsDao`.

The database definition includes a sequence of migrations (1→7) that evolve the
schema for filesystems, sessions, and apps. `UlaDatabase` also resets session
activity on open to avoid stale session state if the app is restarted.

Repositories wrap persistence and remote data access:

- `AppsRepository` loads and caches app metadata.
- `AssetRepository` and `AssetDownloader` manage asset discovery and download
  requirements.
- `GithubApiClient` and `GithubAppsFetcher` access GitHub-backed asset lists.

### 2.4 Session lifecycle and startup state machines

Session startup is implemented as a finite state machine in
`SessionStartupFsm.kt`. It coordinates:

- Fetching asset lists.
- Generating download requirements.
- Downloading missing assets.
- Verifying storage capacity.
- Extracting root filesystems.

The FSM publishes state through LiveData, enabling the UI to update the user
about progress or errors. It uses helpers like `FilesystemManager`,
`StorageCalculator`, `AssetDownloader`, and `Logger` for concrete operations.

There is also a parallel `AppsStartupFsm.kt` for app-specific flows.

### 2.5 Session/Server service

`ServerService.kt` is the Android `Service` responsible for orchestrating
sessions and client launches. Key responsibilities include:

- Handling start/stop/restart/kill commands for sessions.
- Managing session lifecycle with concurrency-safe storage of active sessions
  (`ConcurrentHashMap`).
- Delegating system-level operations to `BusyboxExecutor` and
  `LocalServerManager`.
- Broadcasting status updates with `LocalBroadcastManager`.
- Gracefully stopping when all sessions are stopped.

The service uses coroutines to keep background work off the main thread and
avoids indefinite blocking by enforcing timeouts during session start.

### 2.6 Utilities and platform integration

The `utils/` package includes core integration points:

- `BusyboxExecutor`: wraps the BusyBox-like tools used to launch sessions.
- `UlaFiles`: centralizes filesystem paths used by the app.
- `NotificationConstructor`: maintains foreground notification state.
- `AssetDownloader` and `AssetFileClearer`: manage download sessions and cleanup.
- `PermissionHandler`, `QWarningHandler`: OS version handling and permission
  prompts.
- `BillingManager`: handles in-app billing flow.
- `ProotDebugLogger`: logs proot debug output when enabled.

These utilities connect the Android UI to the shell-level tools used to run
Linux distributions in proot/Termux contexts.

## 3. Native/static helpers (`raf_*.c` and scripts)

UserLAnd includes a suite of small static helpers compiled with musl/clang and
called via shell wrappers in `scripts/`. These utilities are used both for
telemetry and for low-level instrumentation:

- `raf_cpu_core.c`: parses `/proc/stat` input and emits JSON with CPU counters.
- `raf_mem_core.c`: reads `/proc/meminfo` and emits JSON with memory totals.
- `raf_disk_core.c`: reads `/proc/diskstats` and emits JSON with disk I/O info.
- `raf_arch_probe.c`: reports toolchain architecture, endianness, and cycle
  counters for probing runtime architecture.
- `raf_bitraf_vector.c` and `raf_bitraf_store.c`: low-level BITRAF processing
  tools that operate on raw byte vectors and emit deterministic outputs.

Scripts under `scripts/` provide:

- `build_static.sh` and `install_static.sh` to build and install the helper
  binaries.
- `raf_cpu_core.sh`, `raf_mem_core.sh`, and `raf_disk_core.sh` wrappers to ensure
  the correct binary is found on the path.
- `raf_kerneld.sh` and `raf_status.sh` wrappers for the telemetry daemon.

The JSON contracts emitted by these helpers are documented in `README.md` and
are consumed by the RAFAELIA telemetry daemon (see below).

## 4. RAFAELIA telemetry core (`core/`)

### 4.1 Python daemon (`core/librafaelia/raf_kerneld.py`)

The telemetry daemon periodically runs the static helpers and exposes a JSON
HTTP API. It provides:

- `TelemetryCollector` to execute helper commands and compute deltas.
- `AutoTuner` to adjust RAFAELIA parameters based on CPU/memory/disk thresholds.
- A threaded HTTP server exposing `/status` and `/jobs`.

This service is intended to be run locally (e.g. via the wrapper
`scripts/raf_kerneld.sh`) and queried by the Android app or other clients.

### 4.2 Core bindings and controllers

`raf_core_binding.py` and `raf_core_api.h` provide the binding interface between
Python and the RAFAELIA C core library. The binding calls into a shared library
(`librrafaelia_core.so`) when present to set active cores, resize the universe,
compute status, and update decay rates.

### 4.3 RAFAELIA C modules

The repository contains a large set of experimental C modules at the root
(e.g. `rafaelia_kernel_v22.c`, `rafaelia_compact_core.c`,
`rafaelia_trinity_ultimate.c`) alongside a dedicated `rafaelia/` directory.
These are used for benchmarks or algorithmic experiments and are driven by
scripts like `RAFAELIA_BENCH_ALL.sh`.

## 5. Termux integration (`termux-app/`)

UserLAnd includes Termux-specific packaging to include the static helpers in the
APK assets. The `termux-app/package_static_bins.sh` script copies binaries into
the correct asset layout for builds that embed them directly.

## 6. Build system and automation

### 6.1 Gradle and Android build

The build is driven by Android Gradle Plugin and expects JDK 17+ and Android SDK
platform 35. The root `build.gradle`, `settings.gradle`, and `gradle.properties`
control dependency versions. The canonical Android NDK version is pinned in
`gradle.properties` via `ndkVersion`; at the time of this document it is
`27.3.13750724`. Update `gradle.properties` first, then update this document and
other docs to avoid version drift.

### 6.2 Scripted build helpers

- `scripts/build_apk.sh` bootstraps SDK components and runs Gradle builds.
- `scripts/build_unsigned_apk.sh` forces unsigned builds.
- `scripts/sign_apk.sh` signs a release artifact locally.

### 6.3 GitHub and CI guidance

`BUILD_ON_GITHUB.md` describes GitHub Actions build requirements, while
`CONTRIBUTING.md` and `PULL_REQUEST_TEMPLATE.md` document contribution
expectations.

## 7. Operational flow (end-to-end summary)

1. **User selects or configures a session.**
   - UI fragments push state into ViewModels and FSMs.
   - `SessionStartupFsm` validates assets and storage.
2. **Assets are downloaded and extracted.**
   - `AssetRepository` computes required assets.
   - `AssetDownloader` fetches packages; `FilesystemManager` extracts rootfs.
3. **Session is launched.**
   - `ServerService` uses `BusyboxExecutor` and `LocalServerManager` to start
     proot/Termux sessions.
4. **Telemetry (optional).**
   - `raf_kerneld.py` polls helper binaries and exposes `/status` and `/jobs`.
   - The RAFAELIA core can auto-tune based on thresholds.

## 8. Entry points for further study

- `app/src/main/java/tech/ula/ServerService.kt` for session lifecycle and
  service management.
- `app/src/main/java/tech/ula/model/state/SessionStartupFsm.kt` for the detailed
  FSM that prepares a session.
- `core/librafaelia/raf_kerneld.py` for telemetry/auto-tuning.
- `scripts/build_apk.sh`, `scripts/build_static.sh` for build automation.
- Root-level `raf_cpu_core.c`, `raf_mem_core.c`, `raf_disk_core.c` for JSON
  helpers.
