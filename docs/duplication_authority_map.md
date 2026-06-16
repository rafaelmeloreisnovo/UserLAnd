# Duplication Authority Map

Duplicate names are not automatically wrong, but they are ABI and custody risks
when ownership is unclear.  This seed records authority before any move/delete.

| Duplicate name | Observed paths/examples | Current authority | Risk | Next step |
| --- | --- | --- | --- | --- |
| `raf_core.h` | root/core variants observed by name scan | TOKEN_VAZIO | wrong include target | Map every path and choose canonical header. |
| `rafaelia_core.c` | root/core/include/add2-style variants | TOKEN_VAZIO | duplicate implementation | Identify compiled target per build system. |
| `raf_cpu_core.c` | root and core/include-style variants | TOKEN_VAZIO | runtime divergence | Tie to one adapter or mark legacy. |
| `raf_zipraf.c` | root and core/include-style variants | TOKEN_VAZIO | behavior ambiguity | Add checksum/golden test before refactor. |
| `RAFAELIA_BENCH_ALL.sh` | root/core include variants | TOKEN_VAZIO | benchmark drift | Pick one script and archive the other. |
| `CMakeLists.txt` | multiple modules | REFERENCE | acceptable per-module | Ensure each CMake file has clear scope. |
| `Makefile` | root and submodules | REFERENCE | acceptable per-module | Document top-level delegation. |

Recommended expansion command:

```sh
find . -path './.git' -prune -o -maxdepth 5 -type f -printf '%f %p\n' | sort
```
