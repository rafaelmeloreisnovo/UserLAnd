# Building UserLAnd APKs on GitHub Actions

This repository provides automated APK builds using GitHub Actions, eliminating the need for a local PC or signing keys for development builds.

## 🚀 How to Use the Workflow

### Automatic Triggers
The workflow runs automatically on:
- Push to `main`, `master`, or `develop` branches
- Pull requests targeting `main`, `master`, or `develop` branches

### Manual Trigger
1. Navigate to the **Actions** tab in GitHub
2. Select the **"Build APK"** workflow from the left sidebar
3. Click **"Run workflow"** button (top right)
4. Select the branch you want to build from
5. Click the green **"Run workflow"** button
6. Wait 5-10 minutes for the build to complete

## 📥 How to Download APKs

After a workflow run completes successfully:

1. Open the completed workflow run from the **Actions** tab
2. Scroll down to the **Artifacts** section at the bottom of the page
3. Download the APK packages:
   - **UserLAnd-Debug-Unsigned** - Debug build (retained for 30 days)
   - **UserLAnd-Release-Unsigned** - Release build (retained for 90 days)
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

### Network Resilience
- Automatic retry logic for asset downloads (3 attempts with 5-second delays)
- Handles transient network failures gracefully

### Build Optimization
- Gradle dependency caching via `gradle/actions/setup-gradle@v3`
- Faster subsequent builds (dependencies cached)
- `--no-daemon` flag to reduce memory usage in CI

### Artifact Management
- Debug APKs retained for 30 days
- Release APKs retained for 90 days
- Separate artifact uploads for easier identification
- Detailed APK path logging for verification

### No Signing Required
- Uses `-PuserlandUnsignedApk=true` flag (line 64 in `app/build.gradle` confirms support)
- No keystore files needed for CI builds
- Suitable for development and testing workflows

## 🔍 Viewing Build Logs

To see detailed build information:
1. Open the workflow run in the **Actions** tab
2. Click on any step to expand its logs
3. The **Display APK Paths** step shows generated APK locations and sizes
4. Check the **Fetch Assets** step for download progress and retry attempts

## 📚 Additional Resources

- [Android Developer Guide - App Signing](https://developer.android.com/studio/publish/app-signing)
- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [UserLAnd Contributing Guide](CONTRIBUTING.md)

---

**Note**: This workflow is designed for development and testing. For Play Store releases, you'll need to sign APKs with your production keystore and use `bundleRelease` to generate an AAB (Android App Bundle) file.
