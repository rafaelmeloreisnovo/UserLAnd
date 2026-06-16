# Low-Level Bridges

`asm/` documents and hosts architecture bridges: registers, stack, branch,
load/store, calling convention, object format, and linker behavior.

The existing low-level references are:

- `app/src/main/cpp/baremetal_asm.S`;
- `app/src/main/cpp/modules/crypto/voynich_cipher.S`;
- `core/librafaelia/impl/raf_arch_probe_core.c`.

Rules:

- keep assembly paths explicit by architecture;
- document ABI inputs, clobbers, return registers, and fallback behavior;
- prefer branchless/fixed-buffer routines when practical;
- pair every low-level path with a smoke or audit command before marking PASS.
