# Rafaelia Core Contract

**Version:** 1.0  
**Date:** 2026-01-04  
**Status:** Active

## Purpose

This document defines the core invariants, constraints, and guarantees of the Rafaelia minimal core library. The core is designed to be:

- **Minimal**: Only essential functionality exposed
- **Safe**: No telemetry, analytics, or sensitive data collection
- **Portable**: Works across different architectures and platforms
- **Reliable**: Deterministic and well-tested

## Core Invariants

### 1. Minimal Public API

**Invariant**: The public API consists only of essential functions required by external consumers.

**Current Public API**:
- `raf_crc32()` - CRC32 checksum computation

**Guarantee**: No additional functions will be added to the public API without explicit documentation update and justification.

**Location**: `core/librafaelia/raf_core_minimal.h`

### 2. No Telemetry or Analytics

**Invariant**: The core library MUST NOT contain any telemetry, analytics, tracking, or data collection code.

**Prohibited Activities**:
- Collection of user data
- Network communication
- Telemetry or analytics reporting
- Usage tracking
- Device identification
- Personal information gathering

**Enforcement**: Automated audit via `tools/audit_core_integrity.py`

**Verification**: The audit tool runs on every build and fails if violations are detected.

### 3. No Sensitive Logging

**Invariant**: The core library MUST NOT log sensitive information.

**Prohibited Logging**:
- Passwords, secrets, tokens, or credentials
- Personal identifiable information (PII)
- User activity tracking
- Device-specific identifiers

**Allowed Logging**:
- Error messages for debugging
- Performance metrics for internal use only
- Status information that does not reveal sensitive data

### 4. Implementation Details are Internal

**Invariant**: All implementation details are contained in the `impl/` directory and are not part of the public API.

**Structure**:
```
core/librafaelia/
├── raf_core_minimal.h     # Public API (minimal)
├── raf_crc_ecc.c          # Public implementation
└── impl/                   # Internal implementation
    ├── raf_core_api.h      # Internal API
    ├── bitstack.h          # Internal data structures
    ├── raf_cpu_parser.c    # Internal CPU parsing
    └── raf_perf.c          # Internal performance metrics
```

**Guarantee**: External code should only include `raf_core_minimal.h` and call public functions.

### 5. Deterministic Behavior

**Invariant**: Core functions must produce deterministic results for the same inputs.

**Examples**:
- `raf_crc32(data, len)` always returns the same checksum for the same data
- No reliance on undefined behavior
- No uninitialized memory usage

**Testing**: Verified via smoke tests in `app/src/test/java/tech/ula/rafaelia/RafaeliaNativeSmokeTest.kt`

### 6. Error Handling

**Invariant**: Core functions must handle errors gracefully without crashes.

**Requirements**:
- Null pointer checks
- Bounds checking
- Return error codes for failures
- No undefined behavior on invalid inputs

**Example**:
```c
uint32_t raf_crc32(const uint8_t *data, uint32_t len) {
    if (data == NULL || len == 0) {
        return 0u;  // Safe default
    }
    // ... implementation
}
```

### 7. Memory Safety

**Invariant**: No memory leaks, buffer overflows, or use-after-free bugs.

**Requirements**:
- Proper bounds checking
- Safe string operations
- No dynamic allocation in the minimal core (unless explicitly documented)
- All allocated memory is properly freed

### 8. Thread Safety

**Invariant**: Core functions must be thread-safe or explicitly documented as not thread-safe.

**Current Status**:
- `raf_crc32()` is thread-safe (no shared state)

**Requirement**: If functions are added that are not thread-safe, they must be clearly documented.

### 9. Platform Compatibility

**Invariant**: Core library must work across supported platforms without modification.

**Supported Platforms**:
- Android (ARM, ARM64, x86, x86_64)
- Linux (various architectures)

**Requirements**:
- Standard C11 compliance
- POSIX compliance where needed
- No platform-specific code in public API
- Platform-specific code isolated in impl/

### 10. Backward Compatibility

**Invariant**: Public API must maintain backward compatibility.

**Guarantee**: 
- Function signatures cannot change
- Return value semantics must remain consistent
- New functions can be added but existing ones cannot be removed
- Breaking changes require major version bump and migration guide

## Validation and Enforcement

### Automated Checks

1. **Core Integrity Audit**
   - Tool: `tools/audit_core_integrity.py`
   - Frequency: On every build
   - Action: Fails build if violations detected

2. **Unit Tests**
   - Location: `app/src/test/java/tech/ula/rafaelia/RafaeliaNativeSmokeTest.kt`
   - Coverage: Public API functions
   - Requirement: All tests must pass

3. **Build Verification**
   - CMake builds must succeed
   - No compiler warnings in core code
   - Static analysis checks pass

### Manual Reviews

1. **Code Review**
   - All changes to core must be reviewed
   - Focus on security, safety, and compatibility
   - Verify adherence to contract

2. **Security Review**
   - Periodic security audits
   - Check for vulnerabilities
   - Verify no sensitive data handling

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-01-04 | Initial core contract established |

## Contacts

For questions about this contract or proposed changes:
- Repository: https://github.com/rafaelmeloreisnovo/UserLAnd
- File location: `/CORE_CONTRACT.md`

## Appendix: Audit Tool Usage

```bash
# Run core integrity audit
python3 tools/audit_core_integrity.py

# Run with verbose output
python3 tools/audit_core_integrity.py --verbose

# Specify custom root directory
python3 tools/audit_core_integrity.py --root /path/to/repo
```

## Appendix: Running Smoke Tests

```bash
# Run all Rafaelia tests
./gradlew app:testDebugUnitTest --tests "tech.ula.rafaelia.*"

# Run only smoke tests
./gradlew app:testDebugUnitTest --tests "tech.ula.rafaelia.RafaeliaNativeSmokeTest"
```

## Conclusion

This contract ensures the Rafaelia core remains minimal, safe, and reliable. All contributors must adhere to these invariants. Any violations should be treated as high-priority bugs and fixed immediately.

---

**Document Status**: Active  
**Last Updated**: 2026-01-04  
**Next Review**: 2026-07-04 (6 months)
