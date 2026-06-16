# core/include Inventory Seed

`core/include/` currently mixes stable-looking headers, C sources, Python tools,
shell scripts, text reports, and apparent generated artifacts.  This document is
the first custody layer before moving files.

## Classification policy

| Class | Meaning | Destination candidate |
| --- | --- | --- |
| public-header | stable ABI/API contract | `include/` or `core/librafaelia/` |
| internal-header | internal compile contract | `core/librafaelia/impl/` |
| prototype-source | experimental C/C++ implementation | `core/experimental/` or `add2/` |
| script | executable helper | `scripts/` or `tools/` |
| report | generated/audit output | `custody/` |
| binary-or-generated | built artifact or generated file | `custody/` or ignored build output |

## Initial observed samples

| Path | Class | Status | Note |
| --- | --- | --- | --- |
| `core/include/raf_compat.h` | internal-header | AUDIT | Architecture detection header; needs owner decision. |
| `core/include/raf_core.h` | internal-header | AUDIT | Name duplicates other core surfaces; authority not proven. |
| `core/include/rafaelia_core.c` | prototype-source | AUDIT | Source file inside include tree; should not be public ABI. |
| `core/include/RAFAELIA_BENCH_ALL.sh` | script | AUDIT | Benchmark helper; belongs under tool/script custody. |
| `core/include/MATRIX_REPORT.txt` | report | AUDIT | Existing report; candidate for `custody/`. |
| `core/include/aether.c` | prototype-source | AUDIT | Standalone/prototype source. |
| `core/include/rafaelia_ethics_engine.py` | script | AUDIT | Python helper under include tree. |

## Next inventory command

```sh
find core/include -maxdepth 1 -type f -printf '%f\n' | sort
```
