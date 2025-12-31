# Low-level Fullstack Audit (C tools + interoperability)

This document captures a **fullstack view** (input → processing → output → interop) for each low-level tool file, plus curated lists of **33 potential bug classes** and **33 logical bug classes** to guide testing and hardening.

## Scope
- `raf_cpu_core.c`
- `raf_mem_core.c`
- `raf_disk_core.c`
- `raf_userland_debug.c`
- `raf_userland_diag.c`
- `raf_userland_diag_all.c`
- `core/include/raf_core.h`
- `raf_core.h`

---

## File-by-file fullstack notes

### `raf_cpu_core.c`
**Input**: `/proc/stat` stream via stdin.  
**Processing**: parse CPU lines, build array of samples, emit JSON.  
**Output**: JSON `{"timestamp_ms":...,"schema":"proc_stat","cpus":[...]}`.  
**Interop**:
- Assumes Linux `/proc/stat` format and stable fields.
- Protects against overly long lines (drains remainder).

**Potential pitfalls**:
- CPU count capped by `MAX_CPUS`.
- Non-CPU lines terminate parsing early.

---

### `raf_mem_core.c`
**Input**: `/proc/meminfo` file.  
**Processing**: parse `key: value` lines into key/value pairs.  
**Output**: JSON `{"timestamp_ms":...,"meminfo_unit":"kB","meminfo":{...}}`.  
**Interop**:
- Values are from `/proc/meminfo` and are kB (explicit now).
- Drains long lines to avoid partial parse.

**Potential pitfalls**:
- Keys longer than `MAX_KEY_LEN` are dropped.

---

### `raf_disk_core.c`
**Input**: `/proc/diskstats` file.  
**Processing**: parse disk entries, emit JSON.  
**Output**: JSON `{"timestamp_ms":...,"schema":"proc_diskstats","disks":[...]}`.  
**Interop**:
- Assumes kernel exposes `diskstats` with at least 11 numeric fields.
- Name length is bounded but now larger (`MAX_NAME_LEN=64`).

**Potential pitfalls**:
- Extra kernel fields are ignored; only the first 11 are reported.

---

### `raf_userland_debug.c`
**Input**: streamed CSV-like rows on stdin (`cpu,mem,io,threads`).  
**Processing**: normalize metrics, compute severity, CRC, emit matrix JSON.  
**Output**: JSON object `{"tag":...,"severity":...,"matrix":[...]}`.  
**Interop**:
- Explicit low-level parsing and locale-safe numeric parse.
- Guard for line overflow to avoid partial ingest.

**Potential pitfalls**:
- Input rows longer than 256 bytes are dropped with parse error.

---

### `raf_userland_diag.c`
**Input**: stdin with baseline rows, blank line, then live rows.  
**Processing**: parse rows, compute drift/peak/spread, emit JSON.  
**Output**: JSON `{"tag":"userland_trace",...}`.  
**Interop**:
- Locale-independent parsing (dot-decimal, comma-separated).
- Drains long lines to avoid partial parse.

**Potential pitfalls**:
- Requires blank line to switch phase.

---

### `raf_userland_diag_all.c`
**Input**: multi-trace stream; lines with `@tag` start new trace.  
**Processing**: parse multiple groups, compute drift/peak per tag.  
**Output**: JSON array of trace objects.  
**Interop**:
- Locale-independent parsing and overflow guards.

**Potential pitfalls**:
- Group count capped by `Q_GROUPS`.

---

### `core/include/raf_core.h`
**Role**: shared low-level math + core engine definitions.  
**Interop**:
- C11-compatible, aligned structs, inline CRC/xorshift.

**Potential pitfalls**:
- CRC is bitwise and may be slower than hardware CRC on ARM.

---

### `raf_core.h`
**Role**: legacy wrapper for core header include.  
**Interop**:
- Backwards compatibility for older includes.

**Potential pitfalls**:
- Encourages legacy include path; prefer direct `core/include/raf_core.h`.

---

## 33 bug classes (technical/systemic)
1. Buffer overflow (writes beyond bounds)
2. Buffer underflow (negative indexing)
3. Use-after-free
4. Double-free
5. Null dereference
6. Uninitialized read
7. Integer overflow (signed)
8. Integer underflow (signed)
9. Integer overflow (unsigned)
10. Truncation loss (size_t → int)
11. Out-of-range cast
12. Division by zero
13. Time-of-check/time-of-use (TOCTOU)
14. Data race (concurrent access)
15. Deadlock
16. Livelock
17. Memory leak
18. File descriptor leak
19. Handle leak (resource leak)
20. Stack exhaustion (deep recursion)
21. Unbounded loop on malformed input
22. Locale-dependent parsing bug
23. Endianness mismatch
24. Alignment violation (packed data)
25. Off-by-one error
26. Signed/unsigned compare bug
27. Mis-sized allocation (struct padding)
28. Incomplete input read (line truncation)
29. Incorrect error handling path
30. Silent truncation of names/ids
31. Unsupported kernel/procfs format
32. Backpressure failure (blocking read)
33. Incompatible JSON schema changes

## 33 logical bug classes (domain/semantic)
1. Misinterpreting units (kB vs bytes)
2. Wrong normalization range
3. Threshold inversion (warn/crit)
4. Baseline/live phase swap
5. Missing rows not accounted
6. Column mismatch not flagged
7. Drift computed on wrong rows
8. Peak computed before abs()
9. Averaging with zero-filled columns
10. Ignoring device filters (e.g., loop devices)
11. Treating guest CPU time as normal time
12. Overwriting samples on overflow
13. Ignoring NaN/INF values
14. Accepting invalid negative metrics
15. Reporting stale timestamp
16. Using real-time for duration metrics
17. Accepting blank line as data
18. CSV parsing stops too early on comments
19. Lost tags in multi-trace parsing
20. Reusing buffers without clear
21. CRC computed on uninitialized cells
22. Severity stuck due to missing live phase
23. Missing column implies zero but not flagged
24. Drift masked by sparse data
25. Disk name truncation collision
26. CPU lines truncated mid-field
27. Treating partial line as valid row
28. Missing field default zero without warning
29. Multi-trace overlap due to missing seal
30. JSON emitted without schema/version tag
31. Epoch seconds vs ms confusion
32. Memory free forgotten in teardown
33. Failing to cap counts before copy

---

## Suggested low-level evolution
- Add ARMv8 CRC intrinsics for `raf_fast_crc32`.
- Add optional monotonic timestamp alongside epoch time for drift tools.
- Optional schema version fields for JSON consumers.
- Consider expanding diskstats parser to detect kernel field count dynamically.

