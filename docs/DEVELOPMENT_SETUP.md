# Development Setup Guide

Complete guide for setting up a UserLAnd development environment.

## Table of Contents
1. [Prerequisites](#prerequisites)
2. [Quick Start](#quick-start)
3. [Detailed Setup](#detailed-setup)
4. [Development Workflow](#development-workflow)
5. [Testing](#testing)
6. [Code Style](#code-style)
7. [Debugging](#debugging)
8. [Common Development Tasks](#common-development-tasks)

## Prerequisites

### Required Software

1. **Java Development Kit (JDK) 17 or newer**
   ```bash
   # Verify installation
   java -version
   # Should show version 17 or higher
   ```

2. **Android Studio** (recommended) or **Android SDK command-line tools**
   - Download from: https://developer.android.com/studio
   - Includes Android SDK, emulator, and essential tools

3. **Git**
   ```bash
   git --version
   ```

### Recommended Software

- **Android Emulator** or physical Android device (API 23+)
- **ADB** (Android Debug Bridge) - included with Android SDK
- **Gradle** (wrapper included in project)

### System Requirements

- **Operating System**: Linux, macOS, or Windows
- **RAM**: 8GB minimum, 16GB recommended
- **Disk Space**: At least 10GB free (for SDK, build cache, and dependencies)

## Quick Start

For experienced Android developers who want to get started quickly:

```bash
# 1. Clone repository
git clone https://github.com/CypherpunkArmory/UserLAnd.git
cd UserLAnd

# 2. Build debug APK (downloads SDK automatically)
./scripts/build_apk.sh --task assembleDebug

# 3. Install on device/emulator
adb install -r app/build/outputs/apk/debug/app-debug.apk

# 4. Run tests
./gradlew testAll

# 5. Check code style
./gradlew ktlint
```

## Detailed Setup

### 1. Clone the Repository

```bash
git clone https://github.com/CypherpunkArmory/UserLAnd.git
cd UserLAnd
git submodule update --init --recursive
```

### 2. Configure Android SDK

#### Option A: Using Android Studio (Recommended)

1. Open Android Studio
2. Select "Open an existing project"
3. Navigate to the UserLAnd directory
4. Android Studio will prompt to install missing SDK components
5. Accept licenses and let it download required components

#### Option B: Using Command Line Tools

```bash
# Run the build script (handles SDK setup automatically)
./scripts/build_apk.sh --skip-build

# This will:
# - Download Android SDK to build/android-sdk
# - Accept SDK licenses
# - Create local.properties
# - Set up NDK
```

#### Option C: Using Existing SDK

If you already have Android SDK installed:

```bash
# Create local.properties
echo "sdk.dir=/path/to/your/android/sdk" > local.properties
echo "ndk.dir=/path/to/your/ndk" >> local.properties
```

### 3. Build the Project

```bash
# Make gradlew executable
chmod +x ./gradlew

# Build debug variant
./gradlew assembleDebug

# Or use the helper script
./scripts/build_apk.sh --task assembleDebug
```

### 4. Run on Device/Emulator

```bash
# List connected devices
adb devices

# Install APK
adb install -r app/build/outputs/apk/debug/app-debug.apk

# View logs
adb logcat | grep UserLAnd
```

## Development Workflow

### Creating a Feature Branch

```bash
# Create and switch to feature branch
git checkout -b feature/my-new-feature

# Or for bug fixes
git checkout -b fix/issue-123-bug-description
```

### Making Changes

1. **Write Code**: Follow the MVVM-C architecture
   - UI: `app/src/main/java/tech/ula/ui/`
   - ViewModels: `app/src/main/java/tech/ula/viewmodel/`
   - Models: `app/src/main/java/tech/ula/model/`
   - Utils: `app/src/main/java/tech/ula/utils/`

2. **Build Frequently**: Catch errors early
   ```bash
   ./gradlew assembleDebug
   ```

3. **Test Your Changes**: Run affected tests
   ```bash
   # Run specific test class
   ./gradlew test --tests "tech.ula.model.YourTestClass"
   
   # Run all tests
   ./gradlew testAll
   ```

4. **Check Code Style**: Before committing
   ```bash
   ./gradlew ktlint
   
   # Auto-fix style issues
   ./gradlew ktlintFormat
   ```

### Committing Changes

```bash
# Stage changes
git add .

# Commit with descriptive message
git commit -m "feat: Add new feature description"

# Follow conventional commits format:
# feat: New feature
# fix: Bug fix
# docs: Documentation changes
# style: Code style changes
# refactor: Code refactoring
# test: Test additions or changes
# chore: Build process or tool changes
```

### Submitting Pull Request

1. Push your branch to your fork
   ```bash
   git push origin feature/my-new-feature
   ```

2. Create Pull Request on GitHub
3. Fill out the PR template completely
4. Wait for CI checks to pass
5. Address review feedback

## Testing

### Running Tests

```bash
# All tests (unit + instrumented)
./gradlew testAll

# Unit tests only
./gradlew test

# Instrumented tests (requires device/emulator)
./gradlew connectedAndroidTest

# Specific test class
./gradlew test --tests "tech.ula.model.repositories.UlaDatabaseTest"

# With coverage report
./gradlew jacocoCoverageReport
# Open: app/build/reports/jacoco/jacocoCoverageReport/html/index.html
```

### Writing Tests

#### Unit Test Example

```kotlin
// app/src/test/java/tech/ula/model/MyTest.kt
class MyTest {
    @Test
    fun `feature should work correctly`() {
        // Arrange
        val input = "test"
        
        // Act
        val result = myFunction(input)
        
        // Assert
        assertEquals("expected", result)
    }
}
```

#### Instrumented Test Example

```kotlin
// app/src/androidTest/java/tech/ula/MyInstrumentedTest.kt
@RunWith(AndroidJUnit4::class)
class MyInstrumentedTest {
    @Test
    fun useAppContext() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        assertEquals("tech.ula", appContext.packageName)
    }
}
```

## Code Style

UserLAnd follows Kotlin coding conventions with ktlint enforcement.

### Running Linter

```bash
# Check code style
./gradlew ktlint

# Auto-fix issues
./gradlew ktlintFormat
```

### Key Style Guidelines

- **Indentation**: 4 spaces (no tabs)
- **Line Length**: 120 characters max
- **Naming**:
  - Classes: PascalCase
  - Functions/Variables: camelCase
  - Constants: UPPER_SNAKE_CASE
- **Imports**: No wildcard imports (except in specific cases)
- **Comments**: Use KDoc format for public APIs

### Example

```kotlin
/**
 * Manages user sessions in the application.
 *
 * @property sessionDao Data access object for sessions
 */
class SessionManager(
    private val sessionDao: SessionDao
) {
    /**
     * Creates a new session for the given filesystem.
     *
     * @param filesystemId ID of the filesystem
     * @return Newly created session
     */
    suspend fun createSession(filesystemId: Long): Session {
        val session = Session(filesystemId = filesystemId)
        sessionDao.insert(session)
        return session
    }
}
```

## Debugging

### Android Studio Debugger

1. Set breakpoints by clicking line numbers
2. Click "Debug" button (bug icon)
3. Select device/emulator
4. App will pause at breakpoints

### Logging

```kotlin
import android.util.Log

private const val TAG = "MyClass"

Log.d(TAG, "Debug message")
Log.i(TAG, "Info message")
Log.w(TAG, "Warning message")
Log.e(TAG, "Error message", exception)
```

### ADB Logging

```bash
# View all logs
adb logcat

# Filter by tag
adb logcat -s UserLAnd

# Filter by priority (V=Verbose, D=Debug, I=Info, W=Warn, E=Error, F=Fatal)
adb logcat *:W

# Save to file
adb logcat -d > logs.txt

# Clear logs
adb logcat -c
```

### Memory Profiling

Use Android Studio Profiler:
1. Run app in debug mode
2. View → Tool Windows → Profiler
3. Select Memory, CPU, or Network profiler

## Common Development Tasks

### Building for Specific Architecture

```bash
# Build for arm64 only (faster for testing)
./scripts/build_apk.sh --abi arm64-v8a

# Build for all architectures
./scripts/build_apk.sh
```

### Cleaning Build

```bash
# Clean Gradle build directories
./gradlew clean

# Clean everything including caches
./gradlew clean cleanBuildCache
rm -rf ~/.gradle/caches/
```

### Updating Dependencies

```bash
# Check for dependency updates
./gradlew dependencyUpdates

# Update wrapper version
./gradlew wrapper --gradle-version=8.7
```

### Building Release APK

```bash
# Unsigned release (for testing)
./scripts/build_unsigned_apk.sh

# Signed release (requires keystore setup)
# See docs/APK_SIGNING_QUICKREF.md
```

### Working with NDK

```bash
# Build native components
./gradlew :app:externalNativeBuild

# Clean native build
./gradlew :app:cleanExternalNativeBuild
```

### Database Migrations

When changing Room database schema:

1. Update entities in `app/src/main/java/tech/ula/model/entities/`
2. Increment version in `UlaDatabase.kt`
3. Add migration:
   ```kotlin
   val MIGRATION_X_Y = object : Migration(X, Y) {
       override fun migrate(database: SupportSQLiteDatabase) {
           database.execSQL("ALTER TABLE ...")
       }
   }
   ```
4. Add to database builder
5. Test migration with instrumented tests

## IDE Setup

### Android Studio Configuration

**Recommended Plugins:**
- Kotlin
- Rainbow Brackets
- Key Promoter X
- SonarLint

**Code Style:**
1. File → Settings → Editor → Code Style
2. Scheme → Import Scheme → IntelliJ IDEA code style XML
3. Use project's `.editorconfig` settings

**Run Configurations:**
- Android Studio automatically creates run configs
- Customize: Run → Edit Configurations

### VS Code (Alternative)

Install extensions:
- Kotlin Language
- Android iOS Emulator
- Gradle for Java

## Getting Help

- **Documentation**: Check `docs/` directory
- **Troubleshooting**: See `docs/BUILD_TROUBLESHOOTING.md`
- **Issues**: Search [GitHub Issues](https://github.com/CypherpunkArmory/UserLAnd/issues)
- **Community**: Join discussions on GitHub
- **Contributing**: Read `CONTRIBUTING.md`

## Next Steps

After setup:

1. Read architecture overview: `docs/TECHNICAL_DOCUMENTATION.md`
2. Browse existing issues for "good first issue" labels
3. Join discussions to understand ongoing work
4. Start with small changes to get familiar with codebase

Happy coding! 🚀
