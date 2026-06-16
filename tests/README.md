# Test Matrix

`tests/` is the canonical index for smoke, ABI, failsafe, failover, rollback,
and mitigation tests across host, Android, and low-level paths.

Existing executable anchors:

- `core/librafaelia/tests/test_vectra_vm_vector_field.c` — deterministic host C
  test for Vectra VM/vector-field behavior;
- `core/librafaelia/CMakeLists.txt` — registers `vectra_vm_vector_field_test`
  when the build is not Android;
- `tools/audit_core_integrity.py` — audits core privacy/safety constraints.

Required status vocabulary: `PASS`, `FAIL`, `NOT_RUN`, `TOKEN_VAZIO`,
`PENDING`, `AUDIT`, `RUNTIME`, `REFERENCE`.

Minimal validation route:

```sh
python3 tools/audit_core_integrity.py --root . --verbose
cmake -S core/librafaelia -B build/rafaelia-host
cmake --build build/rafaelia-host --target vectra_vm_vector_field_test
ctest --test-dir build/rafaelia-host --output-on-failure
```
