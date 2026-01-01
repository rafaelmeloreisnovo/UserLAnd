# Common Development Tasks

Quick reference guide for common development tasks in UserLAnd.

## Table of Contents
- [Building](#building)
- [Testing](#testing)
- [Database](#database)
- [UI Development](#ui-development)
- [Debugging](#debugging)
- [Release](#release)
- [Git Operations](#git-operations)

## Building

### Build Debug APK
```bash
# Using Gradle directly
./gradlew assembleDebug

# Using helper script (handles SDK setup)
./scripts/build_apk.sh --task assembleDebug

# Build for specific architecture (faster)
./scripts/build_apk.sh --abi arm64-v8a
```

### Build Release APK
```bash
# Unsigned release
./scripts/build_unsigned_apk.sh

# Signed release (requires keystore setup)
# Set environment variables first:
export ANDROID_KEY_ALIAS="your-key-alias"
export ANDROID_STORE_PASSWORD="your-store-password"
export ANDROID_KEY_PASSWORD="your-key-password"

./gradlew assembleRelease
```

### Clean Build
```bash
# Clean build directories
./gradlew clean

# Clean all caches
./gradlew cleanBuildCache
rm -rf ~/.gradle/caches/
rm -rf ~/.android/build-cache/

# Full clean + rebuild
./gradlew clean assembleDebug
```

### Build Faster
```bash
# Single architecture for development
./scripts/build_apk.sh --abi arm64-v8a

# Skip tests during build
./gradlew assembleDebug -x test

# Offline mode (use cached dependencies)
./gradlew assembleDebug --offline

# Max workers (if you have resources)
./gradlew assembleDebug --max-workers=8
```

## Testing

### Run All Tests
```bash
# All tests (unit + instrumented)
./gradlew testAll

# Just unit tests
./gradlew test

# Just instrumented tests (requires emulator/device)
./gradlew connectedAndroidTest
```

### Run Specific Tests
```bash
# Single test class
./gradlew test --tests "tech.ula.model.repositories.SessionRepositoryTest"

# Single test method
./gradlew test --tests "tech.ula.model.repositories.SessionRepositoryTest.createSession"

# Tests matching pattern
./gradlew test --tests "*Repository*"
```

### Test with Coverage
```bash
# Generate coverage report
./gradlew jacocoCoverageReport

# Open report in browser
open app/build/reports/jacoco/jacocoCoverageReport/html/index.html

# Or on Linux
xdg-open app/build/reports/jacoco/jacocoCoverageReport/html/index.html
```

### Continuous Testing
```bash
# Watch mode - reruns tests on file changes
./gradlew test --continuous

# Build + test continuously
./gradlew assembleDebug test --continuous
```

## Database

### View Database Schema
```bash
# Export schema to file
./gradlew :app:exportSchema

# View exported schemas
ls -la app/schemas/

# View specific version
cat app/schemas/tech.ula.model.repositories.UlaDatabase/7.json
```

### Test Database Migrations
```kotlin
// In your test
@Test
fun testMigration6To7() {
    val db = helper.createDatabase(TEST_DB, 6)
    
    // Insert test data for version 6
    db.execSQL("INSERT INTO sessions ...")
    db.close()
    
    // Migrate to version 7
    val migratedDb = helper.runMigrationsAndValidate(
        TEST_DB,
        7,
        true,
        MIGRATION_6_7
    )
    
    // Verify migration
    val cursor = migratedDb.query("SELECT * FROM sessions")
    // assertions...
}
```

### Inspect Database on Device
```bash
# Pull database from device
adb pull /data/data/tech.ula/databases/ula.db .

# Open with sqlite3
sqlite3 ula.db

# Or use Android Studio Database Inspector
# View > Tool Windows > App Inspection > Database Inspector
```

## UI Development

### Preview Layouts
```kotlin
// Add preview annotation to composable or view
@Preview(showBackground = true)
@Composable
fun SessionItemPreview() {
    SessionItem(Session(id = 1, name = "Ubuntu"))
}
```

### Test Layout on Different Screens
```bash
# Create emulators for different screen sizes
avdmanager create avd -n phone -k "system-images;android-30;google_apis;x86_64" -d "pixel"
avdmanager create avd -n tablet -k "system-images;android-30;google_apis;x86_64" -d "pixel_c"

# Launch specific emulator
emulator -avd phone &
```

### Extract Strings to Resources
```kotlin
// Before: Hardcoded string
binding.textView.text = "Session started"

// After: Extract to strings.xml
binding.textView.text = getString(R.string.session_started)

// In strings.xml:
// <string name="session_started">Session started</string>
```

### Add New Fragment
```bash
# 1. Create fragment class
# app/src/main/java/tech/ula/ui/MyFragment.kt

# 2. Create layout
# app/src/main/res/layout/fragment_my.xml

# 3. Add to navigation graph
# app/src/main/res/navigation/nav_graph.xml

# 4. Create ViewModel if needed
# app/src/main/java/tech/ula/viewmodel/MyViewModel.kt
```

## Debugging

### Install and Debug
```bash
# Install debug APK
adb install -r app/build/outputs/apk/debug/app-debug.apk

# Start app and attach debugger
adb shell am start -n tech.ula/.MainActivity

# View logs
adb logcat | grep UserLAnd

# Clear app data
adb shell pm clear tech.ula
```

### Debug Specific Issues
```bash
# Debug crashes
adb logcat | grep -i crash

# Debug memory issues
adb shell dumpsys meminfo tech.ula

# Debug network requests
adb logcat | grep -i okhttp

# Debug database queries
adb logcat | grep -i room
```

### Profiling
```bash
# CPU profiling
# 1. Run app in debug mode
# 2. View > Tool Windows > Profiler
# 3. Select CPU profiler

# Memory profiling
# 1. Run app in debug mode
# 2. View > Tool Windows > Profiler
# 3. Select Memory profiler

# Network profiling
# 1. Run app in debug mode
# 2. View > Tool Windows > Profiler
# 3. Select Network profiler
```

## Release

### Update Version
```kotlin
// In app/build.gradle
versionName "2.8.4"  // Update this

// versionCode is auto-generated from timestamp
```

### Create Keystore
```bash
# Generate new keystore
keytool -genkey -v -keystore userland-release.jks \
  -keyalg RSA -keysize 2048 -validity 10000 \
  -alias userland-key

# Verify keystore
keytool -list -v -keystore userland-release.jks

# Create signing.properties
cp signing.properties.template signing.properties
# Edit signing.properties with your values
```

### Build Signed Release
```bash
# Set environment variables
export ANDROID_KEY_ALIAS="userland-key"
export ANDROID_STORE_PASSWORD="your-password"
export ANDROID_KEY_PASSWORD="your-key-password"

# Build signed release
./gradlew assembleRelease

# Verify signature
jarsigner -verify -verbose -certs \
  app/build/outputs/apk/release/app-release.apk
```

### Sign Existing APK
```bash
# Using helper script
./scripts/sign_apk.sh \
  app/build/outputs/apk/release/app-release-unsigned.apk \
  userland-signed.apk

# Or manually with apksigner
apksigner sign --ks userland-release.jks \
  --ks-key-alias userland-key \
  --out userland-signed.apk \
  app-release-unsigned.apk

# Verify
apksigner verify userland-signed.apk
```

## Git Operations

### Start New Feature
```bash
# Create feature branch
git checkout -b feature/my-new-feature

# Make changes
# ... edit files ...

# Stage and commit
git add .
git commit -m "feat: Add my new feature"

# Push to remote
git push origin feature/my-new-feature
```

### Update Branch with Latest Changes
```bash
# Fetch latest changes
git fetch origin

# Rebase on master
git rebase origin/master

# Or merge
git merge origin/master

# Push changes (use --force-with-lease for rebase)
git push --force-with-lease origin feature/my-feature
```

### Squash Commits
```bash
# Interactive rebase for last 3 commits
git rebase -i HEAD~3

# In editor, change "pick" to "squash" for commits to squash
# Save and exit

# Edit commit message if prompted

# Push (requires force)
git push --force-with-lease origin feature/my-feature
```

### Undo Changes
```bash
# Undo unstaged changes
git checkout -- filename.kt

# Undo last commit (keep changes)
git reset --soft HEAD~1

# Undo last commit (discard changes)
git reset --hard HEAD~1

# Undo specific file from last commit
git checkout HEAD~1 -- filename.kt
```

### Cherry-pick Commit
```bash
# Get commit hash
git log

# Cherry-pick specific commit
git cherry-pick abc123def

# Cherry-pick without committing
git cherry-pick --no-commit abc123def
```

## Code Quality

### Check Code Style
```bash
# Check Kotlin code style
./gradlew ktlint

# Auto-fix style issues
./gradlew ktlintFormat

# Check specific directory
./gradlew ktlint -PktlintFiles="**/ui/**/*.kt"
```

### Find Potential Issues
```bash
# Run lint checks
./gradlew lint

# View lint report
open app/build/reports/lint-results-debug.html

# Run specific lint checks
./gradlew lint -Doverride.lint=true
```

### Update Dependencies
```bash
# Check for dependency updates
./gradlew dependencyUpdates

# View report
cat build/dependencyUpdates/report.txt

# Update specific dependency in build.gradle
# Then sync project
```

## Quick Reference Commands

```bash
# Development cycle
./gradlew clean assembleDebug && adb install -r app/build/outputs/apk/debug/app-debug.apk

# Test cycle
./gradlew test && ./gradlew ktlint

# Full CI cycle locally
./gradlew clean test ktlint assembleDebug

# Deploy to device and view logs
adb install -r app/build/outputs/apk/debug/app-debug.apk && \
adb shell am start -n tech.ula/.MainActivity && \
adb logcat | grep UserLAnd
```

## Tips and Tricks

1. **Use Gradle task caching**: Add `--build-cache` to Gradle commands
2. **Parallel builds**: Already enabled in `gradle.properties`
3. **Incremental builds**: Only rebuild changed modules
4. **Use Android Studio shortcuts**:
   - `Cmd/Ctrl + B`: Go to declaration
   - `Cmd/Ctrl + Shift + F`: Find in path
   - `Cmd/Ctrl + Alt + L`: Reformat code
5. **Create run configurations**: Customize app launch parameters
6. **Use Gradle task names**: `./gradlew tasks --all` to see all available tasks

## Need Help?

- Check [BUILD_TROUBLESHOOTING.md](BUILD_TROUBLESHOOTING.md) for common issues
- See [BEST_PRACTICES.md](BEST_PRACTICES.md) for coding guidelines
- Review [DEVELOPMENT_SETUP.md](DEVELOPMENT_SETUP.md) for environment setup
- Ask in [GitHub Discussions](https://github.com/CypherpunkArmory/UserLAnd/discussions)
