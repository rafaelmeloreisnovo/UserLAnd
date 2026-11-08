# PRoot official upstream — source and license boundary

**Primary sources inspected 2026-11-08**:
- PRoot project: https://github.com/proot-me/proot (default branch `master`)
- License declaration in the project's `README.rst`: `SPDX-License-Identifier: GPL-2.0-or-later`, with link to `COPYING`.
- Full text of `COPYING`: GNU General Public License, version 2, June 1991.
- `AUTHORS` attributes the original PRoot/CARE copyright to STMicroelectronics and identifies initial contributors.

## File-level authorial boundary

1. **Third-party PRoot**: original PRoot code stays under its own applicable GPL-2.0-or-later terms and any additional file-specific notices. No attempt here to relicense upstream code or to attribute it to RAFAELIA.
2. **RAFAELIA independent modules**: the new `authorial-freestanding/` files contain an independently drafted, minimal Linux syscall ABI smoke test. They contain no copied PRoot sources and use no PRoot library. A separate authorial scope notice refers to `rafaelia/LICENSE`, subject to review of compatibility if integrated with other licensed works.
3. **Potential future adaptation**: changing, statically linking or distributing PRoot-derived material can trigger GPL obligations for the resulting covered work. `GPL-2.0-or-later` is not the same as `GPL-2.0-only`; license compatibility must be checked against all other components and actual redistribution manner.
4. **UserLAnd existing app**: `UserLAnd2/LICENSE` says GPLv3 plus Apache 2.0 terminal emulator while `UserLAnd/LICENSE` contains additional custom restrictions and inherited `COPYRIGHT`. Exact per-file rights remain unresolved; conflicting labels do not automatically license inherited files under the RAFAELIA custom license.

## Current state / gate

- `PRoot_SOURCE_IMPORTED=false`.
- `PRoot_RUNTIME_REPLACEMENT=false`.
- `PROOT_DERIVATIVE_PERMISSION` = TOKEN_VAZIO until lineage and distribution review.
- Independent Linux ABI ELF host checks = PASS_SCOPED, physical ARM/Android = NOT_RUN.
- Release gate: HOLD_FOR_LEGAL_AND_DEVICE_E2E; this file is a custody record and not a legal opinion.

Rollback: close draft PR #404 before merge or revert its commit; do not delete upstream license files or history.
