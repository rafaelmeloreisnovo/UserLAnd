# UserLAnd — BetaTester one-tap v1 (debug/beta only)

## Launch and exact scope

Install the **signed Debug APK** for the device ABI, open the separate **UserLAnd BetaTester** launcher entry and tap **Executar bateria sequencial**. The original UserLAnd launcher remains available. The Beta APK variant gets the same launcher entry but the CI unsigned beta APK is NOT directly installable until explicitly signed. The release variant does NOT declare this launcher activity.

The battery reads app/src/main/assets/betatester/scenarios.csv, an ASCII semicolon-separated, bounded 8-row manifest, without adding a Gradle dependency. It is a 1-thread sequential executor, never an exhaustive all-events cross-product. Seed is logged; 24 deterministic attempts sample framework Bundle event ordering (at most 6 distinct permutations for 3 variables). This does NOT exercise the production AppsStartupFsm, PRoot startup, a guest OS, SQL, network or device root.

## Positive and negative controls

| ID | Executor | Expected |
|---|---|---|
| ENV_ABI | Android Build.SUPPORTED_ABIS | PASS when present, else FAIL |
| ENV_MEMORY | ActivityManager MemoryInfo | PASS when plausible |
| APP_PACKAGE | PackageManager package identity | PASS when matched |
| PROOT_LIBRARY | app native lib listing | PASS if lib_proot*.so present, else BLOCKED |
| SANDBOX_IO | scratch write/read/delete under app cache only | PASS on exact round-trip |
| FRAMEWORK_STATE | actual Android Bundle overwrite/read state | PASS |
| SEEDED_EVENT_ORDER | 24 bounded seeded Bundle event orders | PASS on deterministic replay |
| LINUX_GUEST_E2E | explicit no-execution boundary | BLOCKED/NOT_RUN |

A failing case has a stable ID, operation, elapsed time, sanitized reason and random seed. An exception is recorded by class only (no stack payload or credentials). A completed header/cases/summary JSONL file is written under private internal files. User-initiated sharing creates a bounded ZIP in FileProvider's scoped cache path containing receipt.jsonl and manifest.csv with its SHA-256 digest. No automatic upload. Android caches may be reclaimed; the primary receipt in filesDir is retained.

## Evidence, boundaries and rollback

- SOURCE: repo current branch and CSV manifest.
- ARTIFACT: exact-head Debug and Beta APKs after CI.
- EXECUTION: Android runtime only after human installation and one physical tap.
- EVIDENCE: file-local JSONL (self-reported, needs external hash/readback for independent claim).
- CLAIM: IMPLEMENTED_UNTESTED until CI; no PRoot/guest E2E claims.
- AUTHORITY: sandbox/device read-only preflight, user consent by tapping; no service startup or permission escalation.
- PRIVACY: no IMEI, account list, network address, SQL, credentials or user files in report.
- ROLLBACK: close the draft PR (master unchanged) or revert additive commits.
- NEXT: inspect exact-head CI and physical ARMv7 device receipt before extending to mutually authorized guest smoke tests.

Router: NOVOexport/001_PONTEIRO/UserLAnd/START_HERE.md.
