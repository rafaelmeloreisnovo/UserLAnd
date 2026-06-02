# Vectra VM deterministic vector field

This module converts the provided Vectra/Rafaelia bytecode sketch into a small
native component that can be built with the existing UserLAnd `rafaelia` CMake
pipeline. It intentionally stays deterministic and heap-free:

1. load a number (`VOP_VEC_LOAD_NUM`);
2. reduce it by `mod 42`;
3. map it to an arc in `mod 360`;
4. derive an integer triangular chord proxy in Q15;
5. multiply by `sqrt(3)/2` using fixed-point Q15 math;
6. derive a bounded toroidal node;
7. apply contractive correction by repeated `sqrt(3)/2` Q15 steps;
8. audit and seal the run.

## Integration points

- Public API: `core/librafaelia/impl/vectra_vm_vector_field.h`
- Implementation: `core/librafaelia/impl/vectra_vm_vector_field.c`
- Host regression test: `core/librafaelia/tests/test_vectra_vm_vector_field.c`
- CMake target: `vectra_vm_vector_field_test` when building outside Android.

The module does not allocate memory, does not depend on `libm`, and keeps the VM
state caller-owned so Android startup and rollback paths can use it without heap
or garbage-collection pressure.

## Failsafe edges covered

- null VM or bytecode pointers return `VECTRA_STATUS_NULL`;
- odd/truncated bytecode halts with `VECTRA_STATUS_TRUNCATED`;
- unknown opcodes advance by one instruction pair and reduce `phi_q8` by one;
- `VOP_VEC_SEAL` halts cleanly with `VECTRA_STATUS_HALTED`;
- toroidal node arithmetic uses a 64-bit accumulator before reducing to 0..999.

## Official UserLAnd alignment

The official upstream UserLAnd repository keeps the terminal flow centered on
support assets (`busybox`, `dbclient`, `proot`) and minimal distro filesystems.
This patch keeps that route intact, but adds diagnostics and a `/system/bin/sh`
fallback when the terminal support binaries are missing or not executable. That
makes startup failures visible instead of leaving the terminal blank.
