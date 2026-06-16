# Rafaelia Invariant Map

This map translates the symbolic garden into verifiable engineering surfaces.

## Operational invariant

```text
semantic input
-> normalization
-> deterministic operation / IR
-> public ABI
-> C or ASM implementation
-> object file
-> linker
-> binary/library
-> runtime observation
-> custody record
```

## Shells versus invariant

| Shell | Role | Must not change |
| --- | --- | --- |
| Android NDK | mobile build/runtime shell | exported ABI semantics |
| Termux | userland execution shell | deterministic inputs/outputs |
| GCC/Clang | C/C++ compiler shell | public symbol contract |
| MSVC/Visual Studio | Windows compiler/IDE shell | calling convention documentation |
| Embarcadero | alternate IDE/toolchain shell | fixed-size ABI types |
| ASM | low-level implementation shell | registers, stack, clobbers, return value |

## Symbol translation

| Symbolic phrase | Engineering translation | Proof surface |
| --- | --- | --- |
| bottle shape | invariant ABI and execution route | headers, symbols, linker output |
| label | toolchain/platform adapter | adapter docs and build flags |
| TOKEN_VAZIO | unproven state | custody status |
| toroidal map | normalized multidimensional state | fixed struct/test vector |
| phi/coherence | deterministic score/ratio | golden output or checksum |
| branchless/no heap | low-friction implementation constraint | source audit and compiler flags |

## Current anchor files

- `core/librafaelia/raf_core_minimal.h` — public ABI anchor.
- `core/librafaelia/raf_crc_ecc.c` — CRC32 implementation bridge.
- `core/librafaelia/impl/raf_arch_probe_core.c` — architecture/ASM probe.
- `core/librafaelia/tests/test_vectra_vm_vector_field.c` — deterministic test.
- `tools/audit_core_integrity.py` — privacy/safety audit.
