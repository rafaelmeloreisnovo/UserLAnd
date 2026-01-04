# New Features: Crash Shield, Consent System & Debug Panel

## Quick Start

This fork includes enhanced stability and debugging features. No root required, opt-in only.

### 1. Access the Debug Panel

**From the app:**
- Open UserLAnd
- Tap menu (⋮) → "Hardware/Debug Panel"

**What you'll see:**
- Device model, Android version, CPU, memory
- Native library status
- Generate hardware reports
- Export diagnostic data
- Access system settings (Developer Options, All Files Access)

### 2. Enable Advanced Features (Consent Required)

**To enable a feature:**
1. Open Debug Panel
2. Tap the feature button (e.g., "Generate Report")
3. App prompts: "Consent Required - Restart to review"
4. Tap "Restart Now" (or restart manually)
5. Review Consent screen appears
6. Read benefits, risks, and how to disable
7. Check "I fully consent to enable these features"
8. Tap "Accept and Continue"

**Available features:**
- ✅ Hardware Report (local) - Collects device specs for diagnostics
- ✅ Export Logs/Reports - Share via other apps
- ✅ All Files Access - Request storage permission (Android 11+)
- ✅ Developer Options Guide - Instructions to enable dev features

### 3. Crash Reporting

**If the app crashes:**
- You'll see a friendly error screen (no silent failure)
- Options available:
  - Copy Error - Copy to clipboard
  - Export Full Report - Share crash + device report
  - Restart App - Try again
  - Close - Exit app

**Crash reports include:**
- Exception type and message
- Stack trace
- Device info (model, Android version, CPU)
- Memory stats
- Timestamp

**Privacy:** No personal data, accounts, or file contents collected.

### 4. Native Library Safe Mode

If native libraries fail to load (e.g., unsupported ABI):
- App launches normally (no crash)
- Debug Panel shows "Native: ✗ Not available"
- Java fallback implementations used
- Some features may be limited

### 5. Revoking Consent

**To disable all features:**
1. Clear app data: Settings → Apps → UserLAnd → Storage → Clear Data
2. Or: Next time consent screen appears, tap "Deny All"

**To disable specific features:**
- Check feature's "How to disable" in consent screen
- Most can be disabled in Android Settings

## For Developers

### Building with New Features

```bash
# Build debug APK
./gradlew :app:assembleDebug

# Build release APK (requires signing)
./gradlew :app:assembleRelease

# Run license audit
python3 scripts/license_audit.py
```

### Testing Crash Shield

```kotlin
// Add to MainActivity.onCreate() to test:
throw RuntimeException("Test crash")
```

App will show crash screen instead of silent failure.

### Testing Consent Flow

1. Enable a feature requiring consent
2. Restart app → Consent screen appears
3. Test Accept/Deny flows
4. Verify state persists

### Architecture

**Crash Shield:**
- `UserLAndApplication` - Installs global exception handler
- `GlobalExceptionHandler` - Catches uncaught exceptions
- `CrashReportActivity` - Shows crash UI

**Consent System:**
- `ConsentLedger` - Manages ARMED → CONSENTED states
- `ConsentReviewActivity` - User consent UI
- `MainActivity` - Checks pending consents on launch

**Debug Panel:**
- `DebugPanelActivity` - Main UI
- `DeviceInfoCollector` - Gathers hardware data

**License Audit:**
- `scripts/license_audit.py` - Scans dependencies
- `.github/workflows/license-check.yml` - CI integration

## Documentation

- **User Guide:** [CONSENT_SAFETY_GUIDE.md](CONSENT_SAFETY_GUIDE.md)
- **Implementation:** [IMPLEMENTATION_NOTES.md](IMPLEMENTATION_NOTES.md)
- **Licenses:** [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)

## Compatibility

- **Android:** 6.0 (API 23) to 15 (API 35)
- **Architectures:** arm64-v8a, armeabi-v7a, x86, x86_64
- **Root:** Not required
- **Google Play Services:** Optional

## Privacy & Security

✅ **Local-First:** All data stays on device unless you export  
✅ **Opt-In:** All features require explicit consent  
✅ **No Tracking:** No analytics, no telemetry  
✅ **Transparent:** Review source code for all features  
✅ **Revocable:** Clear app data to reset all consents

## Troubleshooting

### App crashes on launch
- Check crash report in: `/data/data/tech.ula/files/crash_last.txt`
- Copy and share crash report from crash screen
- Clear app data and retry

### Consent screen appears every launch
- Make sure to check the "I fully consent" checkbox
- Tap "Accept and Continue" (not back button)

### Debug Panel not in menu
- Update app to latest version
- Clear app cache: Settings → Apps → UserLAnd → Storage → Clear Cache

### Can't export reports
- Grant storage permission in Settings
- Ensure you've consented to "Export Logs/Reports"

## Support

Issues? Questions?
1. Check crash report in Debug Panel
2. Export diagnostic report
3. File issue on GitHub with report attached

## License

This feature set maintains UserLAnd's GPL-3.0 license. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for dependency licenses.

---

**Version:** 2.8.3+  
**Last Updated:** 2026-01-04  
**Maintainer:** UserLAnd Development Team
