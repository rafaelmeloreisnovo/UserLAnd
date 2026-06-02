# Rootless low-level compile-key guard

This guard is intentionally narrow: it validates only local compile-key material and policy acknowledgements. It does **not** request root, bypass Android security controls, fetch external code, or copy third-party material into the build.

## Constraints enforced

- Native implementation uses fixed-size stack storage only; no `malloc`, heap allocator, GC, or external dependency is introduced.
- JNI is limited to copying the provided byte array long enough to compute status and releases it with `JNI_ABORT`.
- Required policy bits must acknowledge license review, copyright review, and no privilege escalation.
- Optional CRC matching lets release automation pin a known key without storing the key itself.
- Entropy and Tag14 checks reject short, repeated, or malformed keys before they enter release signing flows.

## Status model

The Kotlin wrapper returns `CompileKeyReport(status, entropyMilli, tag14)`. A key is accepted only when `status == COMPILE_STATUS_OK`; callers should treat every non-zero bit as a hard stop and keep the existing rollback path.

## Operational safety

Use this guard as a preflight check before compile/sign tasks. Keep real Android signing keys in the platform keystore or CI secret store, never in source control. If a check fails, abort the release job, rotate the candidate key material if necessary, and rebuild from a clean tree.
