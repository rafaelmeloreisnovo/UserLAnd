# Public ABI Surface

`include/` is the canonical landing zone for stable public headers.  The current
published Rafaelia ABI remains `core/librafaelia/raf_core_minimal.h`, where the
only exposed function is `raf_crc32`.

Invariant path:

```text
semantic input -> public contract -> ABI symbol -> object -> linker -> runtime
```

Rules:

- keep headers deterministic and freestanding-friendly;
- do not introduce heap allocation, background telemetry, or implicit runtime;
- document every exported symbol before exposing it here;
- prefer `uint*_t`, fixed sizes, and explicit status codes.

Current references:

- `core/librafaelia/raf_core_minimal.h` — active minimal ABI;
- `CORE_CONTRACT.md` — public contract and safety invariant.
