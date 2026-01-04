# Quick Reference: Ensuring Compilation

## 🚀 Quick Start

```bash
# Verify compilation requirements
./scripts/ensure_compilation.sh

# Run full compilation test
RUN_COMPILATION_TEST=true ./scripts/ensure_compilation.sh

# Build APK
./gradlew assembleDebug
```

## ✅ Pre-Commit Hook (Optional)

```bash
# Install
cp .git-hooks/pre-commit .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit

# Skip (when needed)
git commit --no-verify
```

## 📋 Compilation Checklist

- [ ] Java 17+ installed
- [ ] Android SDK configured
- [ ] Gradle wrapper executable
- [ ] Dependencies resolved
- [ ] Build successful
- [ ] APK generated

## 🔧 Common Issues

### Java not found
```bash
export JAVA_HOME=/path/to/java-17
```

### SDK not configured
```bash
./scripts/build_apk.sh --skip-build
```

### Build fails
```bash
./gradlew clean
./gradlew assembleDebug --refresh-dependencies
```

## 📚 Full Documentation

See [COMPILATION_GUARANTEE.md](COMPILATION_GUARANTEE.md) for complete guide.
