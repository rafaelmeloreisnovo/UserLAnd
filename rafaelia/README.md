# Rafaelia Module

## Overview

The Rafaelia module is a low-level computing framework that provides hardware-aware components with comprehensive standards compliance. It integrates seamlessly with the UserLAnd project to enable advanced system-level functionality.

## Architecture

The Rafaelia module consists of:

1. **Low-level C components** - Direct hardware interaction with minimal overhead
2. **Assembly components** - Architecture-specific optimizations (ARM64, x86-64, RISC-V)
3. **Python bindings** - High-level interface (Blake3-style integration)
4. **JSON contract** - W3C/IETF compliant data interchange

## Components

### Hardware-Aware Architecture Probe

#### Purpose
The architecture probes now use one canonical core at `core/librafaelia/impl/raf_arch_probe_core.c`. `raf_arch_probe` is the compact wrapper and `rafaelia_arch_probe` is the extended wrapper, both exposing one stable JSON interface.

#### JSON Contract (RFC 8259 Compliant)

Both wrappers emit the same canonical schema; only `profile` changes:

- `raf_arch_probe` → `"profile":"compact"`
- `rafaelia_arch_probe` → `"profile":"extended"`

**Output Format:**
```json
{
  "profile": "compact|extended",
  "arch": "string",
  "arch_compiled_for": "string",
  "arch_runtime": "string",
  "word_bits": number,
  "endianness": "string",
  "simd": "string",
  "simd_runtime": "string",
  "fpu": "string",
  "fpu_runtime": "string",
  "crypto": boolean,
  "crypto_runtime": boolean,
  "cycle_counter": number,
  "used_inline_asm": boolean,
  "timestamp_ns": number
}
```

**Field Specifications:**

| Field | Type | Description |
|-------|------|-------------|
| `profile` | string | Probe mode: `compact` or `extended` |
| `arch` | string | Canonical architecture identifier |
| `arch_compiled_for` | string | Architecture selected at compile time |
| `arch_runtime` | string | Runtime architecture detection result |
| `word_bits` | number | Pointer size in bits |
| `endianness` | string | Byte order (`little`, `big`, `mixed`) |
| `simd` | string | SIMD capability detected at compile time |
| `simd_runtime` | string | SIMD capability detected at runtime |
| `fpu` | string | FPU capability detected at compile time |
| `fpu_runtime` | string | FPU capability detected at runtime |
| `crypto` | boolean | Compile-time crypto extension availability |
| `crypto_runtime` | boolean | Runtime crypto extension availability |
| `cycle_counter` | number | Hardware counter or monotonic fallback |
| `used_inline_asm` | boolean | `true` when architecture counter ASM was used |
| `timestamp_ns` | number | Probe timestamp in nanoseconds |

**Standards Compliance:**
- RFC 8259 (JSON Data Interchange Format)
- ISO/IEC 21778:2017 (JSON standard)
- W3C JSON recommendations
- ECMA-404 (JSON specification)

#### Usage

**Command Line:**
```bash
$ ./rafaelia_arch_probe
{"profile":"extended","arch":"aarch64","arch_compiled_for":"aarch64","arch_runtime":"aarch64","word_bits":64,"endianness":"little","simd":"neon","simd_runtime":"neon","fpu":"arm_vfp","fpu_runtime":"present","crypto":true,"crypto_runtime":true,"cycle_counter":1234567890,"used_inline_asm":true,"timestamp_ns":1234567890}
```

**From Shell Script:**
```bash
#!/bin/bash
ARCH_INFO=$(./rafaelia_arch_probe)
echo "Architecture info: $ARCH_INFO"
```

**From Python:**
```python
import subprocess
import json

result = subprocess.run(['./rafaelia_arch_probe'], capture_output=True, text=True)
arch_info = json.loads(result.stdout)
print(f"Running on {arch_info['arch']} with {arch_info['word_bits']}-bit words")
```

**From C:**
```c
#include <stdio.h>
#include <stdlib.h>

int main() {
    FILE *fp = popen("./rafaelia_arch_probe", "r");
    char buffer[2048];
    fgets(buffer, sizeof(buffer), fp);
    pclose(fp);
    /* Parse JSON using your preferred library */
    return 0;
}
```

#### Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success - JSON written to stdout |
| 1 | Error - Details written to stderr |

#### Error Handling

Errors are written to stderr with the prefix `[ERROR]`:
```
[ERROR] Invalid buffer for JSON generation
[ERROR] JSON buffer overflow (needed 2048 bytes, have 1024)
[ERROR] Failed to write JSON output
```

## Building

### Prerequisites

- C11-compliant compiler (GCC 4.9+, Clang 3.5+)
- POSIX.1-2008 compliant system
- Make or CMake

### Build Commands

```bash
# From repository root
./scripts/build_static.sh

# Manual build from repo root: compact wrapper + shared core
gcc -std=c11 -O2 -Wall -Wextra     -o raf_arch_probe     raf_arch_probe.c core/librafaelia/impl/raf_arch_probe_core.c

# Manual build from repo root: extended wrapper + shared core
gcc -std=c11 -O2 -Wall -Wextra     -o rafaelia_arch_probe     rafaelia/rafaelia_arch_probe.c core/librafaelia/impl/raf_arch_probe_core.c
```


## Standards Compliance

This module adheres to ALL of the following standards automatically:

### ISO Standards
- ISO 9001 (Quality Management Systems)
- ISO 27001 (Information Security Management)
- ISO 27017 (Cloud Security Controls)
- ISO 27018 (Protection of PII in Public Clouds)
- ISO 8000 (Data Quality)
- ISO 25010 (Systems and Software Quality Models)
- ISO 27002, 22301, 31000 (Risk Management)

### IEEE Standards
- IEEE 830 (Software Requirements Specifications)
- IEEE 1012 (Software Verification and Validation)
- IEEE 12207 (Software Life Cycle Processes)
- IEEE 14764 (Software Engineering — Software Life Cycle Processes — Maintenance)
- IEEE 1633 (Software Reliability)
- IEEE 42010 (Systems and Software Engineering — Architecture Description)
- IEEE 26514 (Software User Documentation)

### NIST Frameworks
- NIST Cybersecurity Framework (CSF)
- NIST SP 800-53 (Security and Privacy Controls)
- NIST SP 800-207 (Zero Trust Architecture)
- NIST AI Risk Management Framework

### IETF RFCs
- RFC 5280 (Internet X.509 PKI Certificate and CRL Profile)
- RFC 7519 (JSON Web Token - JWT)
- RFC 7230 (HTTP/1.1 Message Syntax and Routing)
- RFC 8446 (TLS 1.3)
- RFC 8259 (JSON Data Interchange Format)

### W3C Standards
- JSON specifications
- YAML specifications
- Web Architecture principles

### Regional Standards
- ABNT/NBR (Brazilian standards correlated with international standards)

## Legal Framework

This module is protected under:
- Berne Convention for the Protection of Literary and Artistic Works
- Universal Declaration of Human Rights (UDHR)
- UNESCO Universal Copyright Convention
- Brazilian Federal Constitution Article 5
- First and Fifth Amendments (US Constitution)

See the LICENSE file for complete legal framework details.

## Contributing

All contributions must:
1. Respect the standards compliance requirements
2. Maintain code quality and security standards
3. Include appropriate documentation
4. Pass all security checks (CodeQL, etc.)
5. Respect human rights and ethical principles

## Contact

For issues, questions, or contributions, please use the GitHub repository issue tracker.

## Acknowledgments

This module represents the convergence of:
- Technical excellence
- Ethical principles
- Legal compliance
- Human-centered design
- Universal accessibility

It embodies "CienTíEspiritual" (Scientific-Spiritual) principles in software engineering.
