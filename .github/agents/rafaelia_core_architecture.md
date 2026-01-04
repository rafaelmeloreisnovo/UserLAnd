# Rafaelia Core Architecture - Agent Reference

## Overview

This document serves as a permanent reference for AI agents working on the Rafaelia core library. It documents the architecture decisions made to establish a minimal, secure core API.

## Date

2026-01-04

## Architecture Decisions

### 1. Minimal Core API Pattern

**Decision**: Separate public API from implementation details using directory structure.

**Structure**:
```
core/librafaelia/
├── raf_core_minimal.h     # PUBLIC API - only essential functions
├── raf_crc_ecc.c          # Implementation of public API
└── impl/                   # INTERNAL - implementation details
    ├── raf_core_api.h      # Internal API (CPU metrics, etc.)
    ├── bitstack.h          # Internal data structures
    ├── raf_cpu_parser.c    # Internal CPU parsing
    └── raf_perf.c          # Internal performance calculations
```

**Rationale**: 
- Keeps public API minimal and stable
- Allows internal refactoring without breaking external code
- Clear separation of concerns
- Easier to audit and maintain

### 2. Security-First Design

**Decision**: Implement automated audit tool to prevent telemetry/analytics/sensitive logging in core.

**Tool**: `tools/audit_core_integrity.py`

**Key Features**:
- Scans core directories for security violations
- Detects telemetry, analytics, network operations, sensitive logging
- Excludes impl/ directory (internal code can be more complex)
- Fails build if violations found

**Patterns Detected**:
- Telemetry/analytics keywords
- Network operations
- Sensitive data logging (passwords, tokens, etc.)
- Personal information collection

**Whitelisting**:
- Comments mentioning these topics are allowed
- CPU "user" field (legitimate metric) is allowed
- Internal implementation (impl/) has relaxed rules

### 3. JNI Public API

**Current Public API**:
- `raf_crc32(const uint8_t *data, uint32_t len)` - CRC32 checksum

**JNI Binding**:
- Location: `app/src/main/java/tech/ula/rafaelia/RafaeliaNative.kt`
- Native method: `external fun crc32(data: ByteArray): Int`
- Safe wrapper: `fun safeCrc32(data: ByteArray): Int` with Java fallback
- Implementation: `app/src/main/cpp/rafaelia_jni.c`

**Design Principles**:
- Minimal surface area
- Safe defaults (fallback to Java implementation)
- Clear error handling
- No telemetry or tracking

### 4. Testing Strategy

**Smoke Tests**: `app/src/test/java/tech/ula/rafaelia/RafaeliaNativeSmokeTest.kt`

**Test Coverage**:
- Empty data handling
- Known test vectors (e.g., CRC32 of "123456789")
- Single byte inputs
- Deterministic behavior verification
- Different inputs produce different outputs
- Null byte handling
- Fallback mechanism when native lib unavailable
- Library availability checking
- Error reporting

**Testing Philosophy**:
- Test public API only
- Focus on correctness and safety
- Verify deterministic behavior
- Test error paths

### 5. Core Contract

**Document**: `/CORE_CONTRACT.md`

**Key Invariants**:
1. Minimal Public API
2. No Telemetry or Analytics
3. No Sensitive Logging
4. Implementation Details are Internal
5. Deterministic Behavior
6. Error Handling
7. Memory Safety
8. Thread Safety
9. Platform Compatibility
10. Backward Compatibility

**Enforcement**:
- Automated audit tool
- Unit tests
- Build verification
- Code reviews

## Files Created/Modified

### Created:
- `core/librafaelia/raf_core_minimal.h` - Minimal public API
- `core/librafaelia/impl/` - Implementation directory
- `tools/audit_core_integrity.py` - Security audit tool
- `app/src/test/java/tech/ula/rafaelia/RafaeliaNativeSmokeTest.kt` - Smoke tests
- `CORE_CONTRACT.md` - Core invariants document
- `.github/agents/rafaelia_core_architecture.md` - This document

### Modified:
- `core/librafaelia/CMakeLists.txt` - Updated to use impl/ structure

### Moved:
- `raf_core_api.h` → `impl/raf_core_api.h`
- `bitstack.h` → `impl/bitstack.h`
- `raf_cpu_parser.c` → `impl/raf_cpu_parser.c`
- `raf_perf.c` → `impl/raf_perf.c`

## Build Instructions

### Build Core Library:
```bash
cd /home/runner/work/UserLAnd/UserLAnd
./gradlew app:assembleDebug
```

### Run Audit:
```bash
python3 tools/audit_core_integrity.py
```

### Run Tests:
```bash
./gradlew app:testDebugUnitTest --tests "tech.ula.rafaelia.*"
```

## Future Agents: Important Notes

### When Adding New Functions to Core:

1. **Question if it's truly needed in public API**
   - Can it be internal (in impl/)?
   - Is it essential for external consumers?

2. **Update raf_core_minimal.h**
   - Add function declaration
   - Document purpose and parameters
   - Ensure it follows naming convention

3. **Update CORE_CONTRACT.md**
   - Document the new function
   - Update version history
   - Explain rationale

4. **Add tests**
   - Create unit tests in RafaeliaNativeSmokeTest.kt
   - Test normal cases, edge cases, error cases
   - Verify deterministic behavior

5. **Run audit**
   - Ensure no security violations
   - Fix any issues found

6. **Update CMakeLists.txt if needed**
   - Add new source files
   - Update includes

### When Modifying Implementation:

1. **Keep changes in impl/ directory**
   - Don't modify public API unless absolutely necessary

2. **Run audit after changes**
   - Ensure no violations introduced

3. **Test thoroughly**
   - Run smoke tests
   - Test edge cases

4. **Maintain backward compatibility**
   - Don't change function signatures
   - Don't change return value semantics

### Red Flags - Never Do These:

❌ Add telemetry/analytics to core  
❌ Add network operations to core  
❌ Log sensitive data (passwords, tokens, PII)  
❌ Break public API compatibility  
❌ Add platform-specific code to public API  
❌ Skip running the audit tool  
❌ Introduce memory leaks or undefined behavior  

### Green Flags - Good Practices:

✅ Keep public API minimal  
✅ Put implementation in impl/  
✅ Run audit tool before committing  
✅ Write comprehensive tests  
✅ Document invariants  
✅ Handle errors gracefully  
✅ Use safe C practices (bounds checking, null checks)  
✅ Maintain backward compatibility  

## References

- Main contract: `/CORE_CONTRACT.md`
- Public API: `core/librafaelia/raf_core_minimal.h`
- Internal API: `core/librafaelia/impl/raf_core_api.h`
- Audit tool: `tools/audit_core_integrity.py`
- Tests: `app/src/test/java/tech/ula/rafaelia/RafaeliaNativeSmokeTest.kt`
- JNI wrapper: `app/src/main/java/tech/ula/rafaelia/RafaeliaNative.kt`
- JNI implementation: `app/src/main/cpp/rafaelia_jni.c`

## Summary

This architecture establishes a clean separation between public API and implementation, with automated enforcement of security and privacy invariants. Future work should maintain these principles to ensure the core remains minimal, safe, and reliable.

---

**Document Type**: Agent Reference  
**Status**: Permanent Record  
**Created**: 2026-01-04  
**Purpose**: Guide future AI agents working on Rafaelia core
