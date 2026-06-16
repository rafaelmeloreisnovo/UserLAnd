# Environment Adapters

`adapters/` is reserved for shells around the same core intention: Android NDK,
Termux, GCC/Clang, MSVC, Visual Studio, Embarcadero, Python, Rust, or C#.

Adapters are labels, not the bottle.  They may translate build flags, calling
conventions, package layout, or runtime entrypoints, but must preserve:

```text
input -> normalization -> core operation -> ABI -> binary -> observed output
```

Rules:

- keep adapter-specific code outside the core ABI;
- record toolchain flags and runtime assumptions;
- provide rollback/failover notes when an adapter selects a fallback path;
- never claim support without a command, output, and custody state.
