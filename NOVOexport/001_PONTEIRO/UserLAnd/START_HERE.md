# UserLAnd — RAFAELIA Ω route / current state

- **Producer**: `rafaelmeloreisnovo/UserLAnd`; `UserLAnd2` is a fork-like reference, not a license grant.
- **SOURCE**: `authorial-freestanding/src/{entry,syscall_linux}.c`.
- **AUTHORITY**: user requested new authorial independent code; upstream rights remain with upstream owners.
- **EXECUTION_TARGET**: standalone Linux ELF ABI slice ARMv7/AArch64; no APK integration.
- **EVIDENCE_RULE**: ELF machine+static+symbol check; x86-64 host execution and write error-path check. Physical ARM E2E=NOT_RUN.
- **LIMIT**: claim_allowed=false for Android app/PRoot replacement/license compatibility. No README can supersede a failed gate.
- **ROUTES**: `../../../authorial-freestanding/README.md`, `../../../authorial-freestanding/PROVENANCE.md`, `../../../authorial-freestanding/tests/gate.sh`.
- **NEXT**: ARMv7 Motorola E7 and AArch64 device execution receipts, targeted PRoot requirements, component license file-level chain.

This GitHub pointer is not evidence of a Google Drive sync; update the longitudinal Drive index only after authorized write+readback.

## One-tap debug/beta BetaTester route (proposed)

- [Scenario manifest](../../../app/src/main/assets/betatester/scenarios.csv) -> [executor](../../../app/src/main/java/tech/ula/betatester/BetaTesterActivity.kt) -> [cases, limitations and receipts](../../../docs/BETATESTER_ONE_TAP_V1.md).
- Both debug and beta source sets advertise a separate launcher entry; release does not. Physical device evidence is required to promote this candidate from IMPLEMENTED_UNTESTED.
