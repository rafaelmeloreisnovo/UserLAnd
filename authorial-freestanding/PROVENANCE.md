# Scope, source and license matrix — 2026-11-08

| Scope | Primary source checked | Rights/claim | State |
|---|---|---|---|
| `authorial-freestanding/` | New independent ABI prototype prepared in this branch | Local RAFAELIA module license intended; authorial assignment/compatibility review pending | HOST_PASS_SCOPED; ARM device NOT_RUN |
| inherited `UserLAnd/` files | `rafaelmeloreisnovo/UserLAnd` existing COPYRIGHT and NOTICE | upstream ownership preserved | INHERITED; NO_RELICENSE |
| `UserLAnd2/LICENSE` | GitHub fork states GPLv3; terminal emulator Apache 2.0 | not ownership proof for all files | THIRD_PARTY_PROVENANCE |
| `CypherpunkArmory/UserLAnd` upstream | inspected its present root; no top-level LICENSE shown | exact upstream licensing conflicting among available declarations | LICENSE_AUTHORITY_TOKEN_VAZIO |
| PRoot | not imported into this module | original copyright/licensing untouched | NOT_IMPORTED |

Do not promote an isolated ABI build to a PRoot implementation, a deployable APK, or a fully independent Android application. No static ELF proves runtime independence from the Linux kernel. Naming, comments and whitespace do not optimize executable code unless they affect parsing, preprocessing, compilation settings or generated sections. A one-bit change can be critical, but code-size/performance improvement needs a benchmark and baseline.
