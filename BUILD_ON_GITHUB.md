# Building UserLAnd APKs on GitHub Actions

This repository provides automated APK builds using GitHub Actions, eliminating the need for a local PC or signing keys for development builds.

## 🚀 Available Workflows

### 1. Build APK (Standard)
Basic workflow for building debug and release APKs with all architectures.

### 2. Build ARM64 Guaranteed ⭐ NEW
Advanced workflow specifically for ARM64 builds with comprehensive validation and auto-repair capabilities.

**Features:**
- ✅ Pre-build environment validation
- ✅ ARM64-specific compilation (arm64-v8a)
- ✅ Post-build APK validation
- ✅ Automatic build repair on failure
- ✅ Detailed GO/NO-GO status reports
- ✅ 90-day artifact retention

### 3. Build All Architectures ⭐ NEW
Parallel builds for all supported architectures:
- ARM64 (arm64-v8a)
- ARMv7 (armeabi-v7a)
- x86_64
- x86

**Benefits:**
- Faster parallel execution
- Individual artifacts per architecture
- Comprehensive summary reports

## 🚀 How to Use the Workflows

### Automatic Triggers
The workflows run automatically on:
- Push to `main`, `master`, or `develop` branches
- Pull requests targeting `main`, `master`, or `develop` branches

### Manual Trigger
1. Navigate to the **Actions** tab in GitHub
2. Select your desired workflow from the left sidebar:
   - **"Build APK"** - Standard build
   - **"Build ARM64 Guaranteed"** - ARM64 with validation
   - **"Build All Architectures"** - All architectures in parallel
3. Click **"Run workflow"** button (top right)
4. Select the branch you want to build from
5. Click the green **"Run workflow"** button
6. Wait 10-15 minutes for the build to complete

## 📥 How to Download APKs

After a workflow run completes successfully:

1. Open the completed workflow run from the **Actions** tab
2. Scroll down to the **Artifacts** section at the bottom of the page
3. Download the APK packages:
   
   **Standard Build Workflow:**
   - **UserLAnd-Debug-Unsigned** - Debug build (retained for 30 days)
   - **UserLAnd-Release-Unsigned** - Release build (retained for 90 days)
   
   **ARM64 Guaranteed Workflow:**
   - **userland-arm64-debug-{sha}** - ARM64 Debug build (retained for 90 days)
   - **userland-arm64-release-{sha}** - ARM64 Release build (retained for 90 days)
   
   **All Architectures Workflow:**
   - **userland-arm64-debug-{sha}** - ARM64 Debug
   - **userland-arm64-release-{sha}** - ARM64 Release
   - **userland-armv7-debug-{sha}** - ARMv7 Debug
   - **userland-armv7-release-{sha}** - ARMv7 Release
   - **userland-x86_64-debug-{sha}** - x86_64 Debug
   - **userland-x86_64-release-{sha}** - x86_64 Release
   - **userland-x86-debug-{sha}** - x86 Debug
   - **userland-x86-release-{sha}** - x86 Release

4. Extract the downloaded ZIP file to get the APK
5. Transfer the APK to your Android device

## 🔧 APK Variants Explained

### Debug APK
- **Purpose**: Development and testing
- **Characteristics**:
  - Larger file size (includes debugging symbols)
  - Not optimized or minified
  - Easier to troubleshoot with logcat
  - Marked as `debuggable` in manifest
- **Use case**: Best for diagnosing issues or testing new features

### Release APK (Unsigned)
- **Purpose**: Production-ready but without signature
- **Characteristics**:
  - Smaller file size (optimized and minified)
  - Code shrinking and resource optimization enabled
  - Not signed with production keystore
  - Better performance than Debug
- **Use case**: Testing production builds or preparing for manual signing

## 📱 Installing APKs on Android

### Method 1: Direct Installation
1. Transfer the APK file to your Android device (via USB, cloud storage, email, etc.)
2. On your Android device, go to **Settings** → **Security** (or **Privacy**)
3. Enable **"Install unknown apps"** for your file manager or browser
   - On newer Android versions: Settings → Apps → Special app access → Install unknown apps → Select your file manager → Allow
4. Tap on the APK file using your file manager
5. Tap **Install** and wait for installation to complete
6. Grant any required permissions when prompted

### Method 2: ADB Installation
If you have ADB (Android Debug Bridge) installed on your computer:
```bash
adb install app-debug.apk
# or
adb install app-release-unsigned.apk
```

## 🛠️ Troubleshooting

### Asset Download Fails
- **Issue**: Network errors during `fetchAssets` step
- **Solution**: The workflow automatically retries up to 3 times with 5-second delays between attempts
- **If persistent**: Check if GitHub Actions has internet connectivity issues (rare)

### Build Fails
- **Issue**: Build errors during compilation
- **Check**: 
  - Java version must be 17 (workflow uses Temurin distribution)
  - Gradle caching is enabled automatically
  - Review the workflow logs for specific error messages
- **ARM64 Guaranteed**: Auto-repair will automatically attempt to fix the build by:
  1. Stopping Gradle daemons
  2. Clearing Gradle caches
  3. Removing build directories
  4. Resyncing dependencies
  5. Retrying the build

### APK Validation Fails (ARM64 Guaranteed Only)
- **Issue**: APK validation step fails
- **Checks performed**:
  - ZIP structure integrity
  - APK size > 10MB
  - ARM64 libraries present (lib/arm64-v8a/)
  - Unsigned status verification
- **Solution**: Check the validation logs for specific failures

### APK Won't Install on Device
- **Issue**: "App not installed" or similar error
- **Solutions**:
  1. Ensure "Install unknown apps" is enabled for your file manager
  2. Uninstall any previous version of UserLAnd first
  3. Check if you have enough storage space
  4. Try installing the Debug APK first (it's less restricted)

### App Crashes on Launch
- **Issue**: App crashes immediately after opening
- **Solutions**:
  1. Use the Debug APK for better crash logs
  2. Run `adb logcat` to see detailed error messages
  3. Check if your device meets minimum requirements (Android 5.0+)
  4. Clear app data and cache if upgrading from older version

### Build Takes Too Long
- **Issue**: Workflow exceeds expected time
- **Cause**: First build on a fresh runner needs to download dependencies
- **Solution**: Subsequent builds will be faster due to Gradle caching

## 🔐 Optional: Signing APKs Manually

The workflow produces unsigned APKs. If you need signed APKs for production:

### Step 1: Generate a Keystore
```bash
keytool -genkey -v -keystore my-release-key.jks \
  -keyalg RSA -keysize 2048 -validity 10000 \
  -alias my-key-alias
```

You'll be prompted to:
- Create a keystore password
- Enter your name and organization details
- Create a key password

**⚠️ Important**: Keep your keystore file and passwords secure! Loss means you cannot update your app.

### Step 2: Sign the APK
```bash
jarsigner -verbose -sigalg SHA256withRSA -digestalg SHA-256 \
  -keystore my-release-key.jks \
  app-release-unsigned.apk \
  my-key-alias
```

Enter the keystore password when prompted.

### Step 3: Verify the Signature
```bash
jarsigner -verify -verbose -certs app-release-unsigned.apk
```

You should see "jar verified" in the output.

### Step 4: Optimize with Zipalign
```bash
# Install Android SDK build-tools if not already available
zipalign -v 4 app-release-unsigned.apk app-release-signed.apk
```

This optimizes the APK for faster loading on devices.

### Step 5: Verify Final APK
```bash
zipalign -c -v 4 app-release-signed.apk
```

You should see "Verification successful" in the output.

## 📊 Workflow Features

### Standard Build Workflow
- Network resilience with automatic retry logic (3 attempts)
- Gradle dependency caching via `gradle/actions/setup-gradle@v3`
- Faster subsequent builds (dependencies cached)
- `--no-daemon` flag to reduce memory usage in CI

### ARM64 Guaranteed Workflow ⭐
- **Pre-build validation**: Checks MainActivity.kt, AndroidManifest.xml, build.gradle
- **Post-build validation**: Comprehensive APK structure and content validation
- **Auto-repair system**: Automatically fixes common build issues
- **Detailed reporting**: GO/NO-GO status in GitHub Actions summary
- **Extended retention**: 90-day artifact retention
- **Validation script**: `scripts/validate-arm64-apk.sh` for manual testing
- **Repair script**: `scripts/auto-repair-build.sh` for local debugging

### All Architectures Workflow ⭐
- **Parallel builds**: All architectures build simultaneously
- **Matrix strategy**: Efficient use of CI resources
- **Per-architecture artifacts**: Easy selection of specific builds
- **Individual validation**: Each architecture validated separately

### Artifact Management
- Debug APKs retained for 30-90 days (depending on workflow)
- Release APKs retained for 90 days
- Separate artifact uploads for easier identification
- Detailed APK path logging for verification

### No Signing Required
- Uses `-PuserlandUnsignedApk=true` flag (line 64 in `app/build.gradle` confirms support)
- Environment variable `USERLAND_UNSIGNED_APK=1` for compatibility
- No keystore files needed for CI builds
- Suitable for development and testing workflows

## 🔍 Viewing Build Logs

To see detailed build information:
1. Open the workflow run in the **Actions** tab
2. Click on any step to expand its logs
3. The **Display APK Paths** step shows generated APK locations and sizes
4. Check the **Fetch Assets** step for download progress and retry attempts

### ARM64 Guaranteed Workflow Summary
The ARM64 workflow provides a comprehensive summary at the end of each run:
- ✅ Pre-build validation status
- ✅ Debug APK compilation status
- ✅ Release APK compilation status
- ✅ ARM64 libraries verification
- ✅ Sanity tests results
- 🎯 Final GO/NO-GO decision
- 📊 APK sizes and checksums
- 🔐 RAFAELIA project signature

## 🧪 Local Testing Scripts

The ARM64 workflow includes scripts you can use locally:

### Validate ARM64 APK
```bash
./scripts/validate-arm64-apk.sh path/to/your-app.apk
```
This script validates:
- ZIP structure integrity
- APK size (must be > 10MB)
- ARM64 native libraries presence
- Unsigned status
- SHA256 checksum

### Auto-repair Build Issues
```bash
./scripts/auto-repair-build.sh [gradle-task]
```
Default task is `assembleRelease`. This script:
1. Stops Gradle daemons
2. Clears all caches
3. Removes build directories
4. Resyncs dependencies
5. Fetches assets with retry
6. Attempts rebuild

## 📚 Additional Resources

- [Android Developer Guide - App Signing](https://developer.android.com/studio/publish/app-signing)
- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [UserLAnd Contributing Guide](CONTRIBUTING.md)

---

**Note**: This workflow is designed for development and testing. For Play Store releases, you'll need to sign APKs with your production keystore and use `bundleRelease` to generate an AAB (Android App Bundle) file.
