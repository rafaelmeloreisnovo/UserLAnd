# Android NDK drop-in location

Place an extracted Android NDK in this directory to make native helper builds
reproducible in local environments and CI. The build scripts will also respect
`NDK_DIR`, `ANDROID_NDK_HOME`, or `NDK_HOME` if you prefer to keep the NDK
elsewhere.

Expected layout (example for Linux hosts):

```
ndk/
└── toolchains/llvm/prebuilt/linux-x86_64/bin/clang
```

Only this documentation file is tracked; the actual NDK contents remain local
to avoid bloating the repository.
