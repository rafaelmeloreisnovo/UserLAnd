#!/usr/bin/env sh
set -eu

need_file() {
  test -f "$1" || {
    printf 'MISSING %s\n' "$1" >&2
    return 1
  }
}

need_dir() {
  test -d "$1" || {
    printf 'MISSING %s\n' "$1" >&2
    return 1
  }
}

need_dir include
need_dir adapters
need_dir asm
need_dir tests
need_dir custody
need_file include/README.md
need_file adapters/README.md
need_file asm/README.md
need_file tests/README.md
need_file custody/README.md
need_file docs/rafaelia_invariant_map.md
need_file docs/core_include_inventory.md
need_file docs/duplication_authority_map.md
need_file core/librafaelia/raf_core_minimal.h
need_file core/librafaelia/tests/test_vectra_vm_vector_field.c
need_file tools/audit_core_integrity.py

printf 'PASS custody skeleton and invariant map are present\n'
