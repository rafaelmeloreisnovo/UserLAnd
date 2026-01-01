# Build Troubleshooting Guide

This guide covers common issues encountered when building UserLAnd and their solutions.

## Table of Contents
1. [Prerequisites Issues](#prerequisites-issues)
2. [Gradle Build Failures](#gradle-build-failures)
3. [NDK Issues](#ndk-issues)
4. [Signing Issues](#signing-issues)
5. [Memory and Performance](#memory-and-performance)
6. [CI/CD Issues](#cicd-issues)

## Prerequisites Issues

### Java Version Mismatch
**Problem:** Build fails with "Unsupported class file major version" or similar.

**Solution:**
```bash
# Ensure Java 17+ is installed and active
java -version

# On macOS with multiple JDKs
export JAVA_HOME=$(/usr/libexec/java_home -v 17)

# On Linux
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64

# On Windows
set JAVA_HOME=C:\Program Files\Java\jdk-17
```

### Android SDK Not Found
**Problem:** `ANDROID_SDK_ROOT` or `sdk.dir` not set.

**Solution:**
```bash
# Create local.properties if it doesn't exist
echo "sdk.dir=/path/to/your/android/sdk" > local.properties

# Or use scripts/build_apk.sh which handles this automatically
./scripts/build_apk.sh
```

## Gradle Build Failures

### Out of Memory Error
**Problem:** `java.lang.OutOfMemoryError: Java heap space`

**Solution:**
```bash
# Add to gradle.properties
org.gradle.jvmargs=-Xmx4096m -XX:MaxMetaspaceSize=512m

# Or set environment variable
export GRADLE_OPTS="-Xmx4096m"
```

### Build Cache Corruption
**Problem:** Random build failures or "Could not resolve all files"

**Solution:**
```bash
# Clean build directories
./gradlew clean

# Clear Gradle cache
rm -rf ~/.gradle/caches/

# Rebuild
./gradlew assembleDebug
```

### Dependency Resolution Failures
**Problem:** "Could not find" or "Could not resolve" dependency errors.

**Solution:**
```bash
# Check your internet connection first

# Force refresh of dependencies
./gradlew assembleDebug --refresh-dependencies

# Check if repository is accessible
./gradlew dependencies
```

### Plugin Compatibility Issues
**Problem:** "An exception occurred applying plugin request [id: 'xxx']"

**Solution:**
- Verify all plugin versions in `build.gradle` are compatible
- Check that Kotlin version matches across all modules
- Current working versions:
  - Kotlin: 2.0.0
  - Android Gradle Plugin: 8.6.0
  - Navigation: 2.9.6

## NDK Issues

### NDK Not Found
**Problem:** "NDK not configured" or "ndkVersion ... not found"

**Solution:**
```bash
# Option 1: Let Gradle download it automatically
./gradlew assembleDebug

# Option 2: Set NDK path in local.properties
echo "ndk.dir=/path/to/ndk/27.3.13750724" >> local.properties

# Option 3: Set environment variable
export ANDROID_NDK_HOME=/path/to/ndk/27.3.13750724
```

### ABI Filter Issues
**Problem:** Build succeeds but APK doesn't work on target device.

**Solution:**
```bash
# Build for specific ABI
./scripts/build_apk.sh --abi arm64-v8a

# Check which ABIs are in your APK
unzip -l app/build/outputs/apk/debug/app-debug.apk | grep "\.so$"

# Build for all supported ABIs (default)
./scripts/build_apk.sh
```

### CMake/Ninja Build Failures
**Problem:** Native library compilation fails.

**Solution:**
```bash
# Ensure CMake and Ninja are installed
cmake --version
ninja --version

# Clean native build cache
./gradlew clean cleanBuildCache

# Try building without parallel execution
./gradlew assembleDebug --max-workers=1
```

## Signing Issues

### Keystore Not Found
**Problem:** Release build fails with "Keystore file not found"

**Solution:**
```bash
# For debug builds (uses default debug keystore)
./scripts/build_apk.sh --task assembleDebug

# For unsigned release builds
./scripts/build_unsigned_apk.sh

# For signed release (requires keystore setup)
# See docs/APK_SIGNING_QUICKREF.md for details
```

### Wrong Keystore Password
**Problem:** "Failed to read key from keystore"

**Solution:**
- Verify credentials in `signing.properties` or environment variables
- Environment variables needed:
  - `ANDROID_KEY_ALIAS`
  - `ANDROID_STORE_PASSWORD`
  - `ANDROID_KEY_PASSWORD`
- For signing script, use unprefixed variables (see docs/APK_SIGNING_QUICKREF.md)

### APK Installation Failed
**Problem:** "INSTALL_FAILED_UPDATE_INCOMPATIBLE" or signature mismatch

**Solution:**
```bash
# Uninstall existing app first
adb uninstall tech.ula

# Then reinstall
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

## Memory and Performance

### Slow Build Times
**Problem:** Builds take too long.

**Solution:**
```bash
# Enable Gradle daemon (should be on by default)
echo "org.gradle.daemon=true" >> gradle.properties

# Enable parallel execution
echo "org.gradle.parallel=true" >> gradle.properties

# Enable configuration cache (Gradle 8.7+)
echo "org.gradle.configuration-cache=true" >> gradle.properties

# Increase build heap size
echo "org.gradle.jvmargs=-Xmx4096m" >> gradle.properties

# Use single ABI for dev builds
./scripts/build_apk.sh --abi arm64-v8a
```

### Disk Space Issues
**Problem:** "No space left on device"

**Solution:**
```bash
# Check disk usage
df -h

# Clean Gradle caches
./gradlew clean cleanBuildCache
rm -rf ~/.gradle/caches/
rm -rf ~/.android/build-cache/

# Clean build outputs
rm -rf app/build/
rm -rf build/
```

## CI/CD Issues

### GitHub Actions Timeout
**Problem:** Workflow exceeds 45-minute timeout.

**Solution:**
- Use architecture-specific builds instead of building all ABIs
- Enable Gradle cache in workflows (already configured)
- Consider splitting into multiple jobs
- Check for stuck Gradle daemons

### Artifact Upload Failures
**Problem:** "No files were found with the provided path"

**Solution:**
- Verify build completed successfully before upload step
- Check APK output path matches artifact configuration
- Review workflow logs for actual build errors
- Ensure `if-no-files-found: error` catches build failures

### Gradle Daemon Issues in CI
**Problem:** Builds hang or timeout in CI.

**Solution:**
```bash
# Run Gradle with --no-daemon in CI
./gradlew assembleDebug --no-daemon --stacktrace

# Or in workflow, use GitHub's Gradle action (handles daemon lifecycle)
- name: Setup Gradle
  uses: gradle/actions/setup-gradle@v4
```

## Getting Help

If none of these solutions work:

1. Check the [GitHub Issues](https://github.com/CypherpunkArmory/UserLAnd/issues) for similar problems
2. Run with verbose logging:
   ```bash
   ./gradlew assembleDebug --stacktrace --info --scan
   ```
3. Include the following in your bug report:
   - OS and version
   - Java version (`java -version`)
   - Gradle version (`./gradlew --version`)
   - Android SDK version
   - Full error log with stacktrace
   - Steps to reproduce

## Quick Reference Commands

```bash
# Full clean build
./gradlew clean assembleDebug --stacktrace

# Debug build for testing
./scripts/build_apk.sh --task assembleDebug

# Release build (unsigned)
./scripts/build_unsigned_apk.sh

# Architecture-specific build
./scripts/build_apk.sh --abi arm64-v8a

# Check dependencies
./gradlew :app:dependencies

# List all tasks
./gradlew tasks --all

# Verify Gradle setup
./gradlew --version
```
