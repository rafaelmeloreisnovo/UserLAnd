# UserLAnd Enhanced Stability & Debug System - Implementation Notes

> **⚠️ NOTA:** Este documento foi consolidado em [PR_CONSOLIDATED.md](PR_CONSOLIDATED.md)
> 
> Para a documentação completa e atualizada de todos os PRs, consulte o documento consolidado.

---

## Overview

This document describes the crash shield, consent system, and debug panel features added to UserLAnd for enhanced stability and diagnostic capabilities.

## Features Implemented

### 1. Crash Shield (Anti-Splash-Crash)

**Purpose:** Prevent silent app crashes and provide actionable error information to users.

**Components:**
- `UserLAndApplication.kt` - Application class that installs global exception handler
- `GlobalExceptionHandler.kt` - Captures uncaught exceptions app-wide
- `CrashReportActivity.kt` - User-friendly crash screen with export functionality

**How it works:**
1. Global exception handler installed in `Application.onCreate()`
2. Any uncaught exception is captured before app termination
3. Crash details saved to `filesDir/crash_last.txt`
4. Device report saved to `filesDir/device_report.json`
5. `CrashReportActivity` launched showing error details
6. User can copy error, export full report, restart app, or close

**What's collected on crash:**
- Exception type and message
- Full stack trace
- Device info (manufacturer, model, Android version, SDK)
- CPU info (supported ABIs, processor count)
- Memory stats (max, total, free, used)
- Timestamp

**Privacy:** No personal data, network info, or file contents collected.

### 2. Native Library Safe Loading

**Purpose:** Prevent crashes when native libraries fail to load (e.g., ABI mismatch, missing .so files).

**Changes to `RafaeliaNative.kt`:**
- Wrapped `System.loadLibrary()` in try-catch
- Added `isNativeLibraryAvailable()` to check load status
- Added `safeCrc32()` with Java fallback implementation
- App gracefully degrades to compatibility mode if native lib unavailable

**Benefits:**
- App launches even on unsupported devices
- Clear error reporting in Debug Panel
- No silent crashes on splash screen

### 3. Consent System (ARMED → CONSENTED)

**Purpose:** Give users full control over advanced features with explicit, informed consent.

**Components:**
- `ConsentItem.kt` - Data class for consent items
- `ConsentLedger.kt` - Persistent consent state management
- `ConsentReviewActivity.kt` - Consent review UI

**Consent Flow:**
1. User enables feature in Debug Panel → State: ARMED
2. On next app launch, `MainActivity` checks for pending consents
3. If pending, redirect to `ConsentReviewActivity`
4. User reviews each item (benefit, risk, how to disable)
5. User must check "I fully consent" and tap Accept
6. States transition: ARMED → CONSENTED
7. App proceeds to main activity

**Consent Items:**
| ID | Title | Requires Reconfirmation |
|----|-------|-------------------------|
| `hardware_report` | Hardware Report (Local) | No |
| `export_logs` | Export Logs/Reports | Yes |
| `all_files_access` | All Files Access | Yes |
| `dev_options_guide` | Developer Options Guide | No |

**How to revoke:**
- Tap "Deny All" in consent review screen
- Clear app data
- Uninstall/reinstall app

### 4. Debug Panel

**Purpose:** Comprehensive hardware diagnostics and system access panel.

**Components:**
- `DebugPanelActivity.kt` - Main debug panel UI
- `DeviceInfoCollector.kt` - Collects device/hardware information

**Features:**
- **Device Info Display:**
  - Device model, manufacturer, Android version
  - CPU ABIs and core count
  - Memory usage (used/max)
  - Native library status
  
- **Report Actions:**
  - Generate full report (text + JSON)
  - Copy summary to clipboard
  - Export files via share intent
  
- **System Actions:**
  - Open Developer Options (with instructions)
  - Open App Details settings
  - Request All Files Access (Android 11+)

**Information Collected:**
- Device: manufacturer, model, brand, fingerprint
- OS: Android version, SDK, security patch
- CPU: supported ABIs, processor count, /proc/cpuinfo (truncated)
- Memory: max/total/free/used RAM
- Storage: internal/external/app files/cache sizes
- GPU: (requires OpenGL context - noted in report)
- Sensors: list of all sensors with vendor info
- Kernel: /proc/version
- SELinux: status (enforcing/permissive/disabled)

**Privacy:** All data stays local unless user explicitly exports.

### 5. License Audit System

**Purpose:** Ensure legal compliance by tracking all third-party dependencies and their licenses.

**Components:**
- `scripts/license_audit.py` - Python script to scan Gradle dependencies
- `THIRD_PARTY_NOTICES.md` - License inventory
- `.github/workflows/license-check.yml` - CI workflow

**How it works:**
1. Script runs `./gradlew app:dependencies`
2. Parses output to extract all dependencies
3. Matches dependencies against known license database
4. Generates markdown and JSON reports
5. Fails if any dependency has unknown license

**CI Integration:**
- Runs on PR and push to main/develop
- Fails build if unlicensed dependencies found
- Uploads license report as artifact
- Comments on PR with unknown licenses

**Approved Licenses:**
- Apache-2.0, MIT, BSD variants
- GPL-2.0, GPL-3.0, LGPL-2.1, LGPL-3.0
- EPL-1.0, EPL-2.0, MPL-2.0
- CC0-1.0, Public Domain

## Architecture Decisions

### Why Two-Stage Consent?

1. **Prevents Accidental Activation:** User must explicitly confirm after restart
2. **Time to Reflect:** User can research feature before final decision
3. **Clear Intent:** Checkbox + button confirm deliberate choice
4. **Reversible:** Easy to deny during review

### Why Local-First?

- All data stored in app's private storage (`filesDir`)
- No automatic network transmission
- User controls all exports via Android share dialog
- Crash reports never leave device unless user shares

### Why Graceful Degradation?

- Native library failure is common on diverse Android devices
- App should work without native features
- Better UX than crash on launch
- Clear messaging about compatibility mode

## Security Considerations

### Data Minimization

- Crash reports: only technical info, no personal data
- Device reports: hardware specs, no file contents or network data
- Consent system: only stores boolean flags

### Permission Model

- App never requests permissions automatically
- Always opens Settings for user to grant
- Consent required before requesting sensitive permissions
- Clear explanation of risks before permission request

### Attack Surface

- Crash handler: Cannot be abused to inject data (writes to private storage)
- Device info: Read-only access to /proc files (standard Android)
- Export: Uses Android's share intent (OS handles security)
- Consent: Stored in SharedPreferences (private to app)

## Testing

### Manual Test Scenarios

1. **Crash Handling:**
   - Force crash: `throw RuntimeException("Test crash")`
   - Verify CrashReportActivity appears
   - Check crash_last.txt created
   - Test copy and export buttons

2. **Native Library Failure:**
   - Remove or rename librafaelia.so
   - App should launch without crash
   - Debug Panel should show "Native: ✗ Not available"
   - safeCrc32() should use Java fallback

3. **Consent Flow:**
   - Open Debug Panel → Generate Report
   - Restart app
   - Verify ConsentReviewActivity appears
   - Test Accept and Deny flows
   - Verify consent persists after Accept

4. **Debug Panel:**
   - Open from menu
   - Verify device info displays
   - Test all buttons
   - Verify Settings intents work

5. **License Audit:**
   - Run `python3 scripts/license_audit.py`
   - Verify THIRD_PARTY_NOTICES.md updated
   - Check for unknown licenses

### Automated Tests

- Unit tests for ConsentLedger state transitions
- Unit tests for DeviceInfoCollector data parsing
- Integration tests for crash handler
- CI: License check on every PR

## Compatibility

- **Minimum SDK:** 23 (Android 6.0)
- **Target SDK:** 35 (Android 15)
- **Architectures:** arm64-v8a, armeabi-v7a, x86, x86_64
- **Tested on:** Android 13, 14, 15

### Android Version Specific Features

- **Android 11+ (API 30):** All Files Access permission available
- **Android 13+ (API 33):** POST_NOTIFICATIONS permission
- **Android 14+ (API 34):** Enhanced crash reporting
- **Android 15 (API 35):** Full compatibility tested

## Performance Impact

- **Startup:** ~10-20ms overhead from exception handler installation
- **Crash:** ~50-100ms to write reports before termination
- **Memory:** <1MB for all new components
- **Storage:** ~100KB for typical crash/device reports

## Future Enhancements

### Potential Additions

1. **Crash Analytics Dashboard:**
   - Local crash history viewer
   - Trend analysis (crashes over time)
   - Most common crash types

2. **Advanced Hardware Tests:**
   - CPU benchmark
   - Memory stress test
   - Storage I/O test
   - GPU rendering test

3. **Remote Debugging:**
   - Optional cloud upload of crash reports
   - Privacy-preserving telemetry
   - Remote log viewer

4. **Consent Expiration:**
   - Time-based consent (e.g., 30 days)
   - Re-prompt for sensitive features
   - Usage statistics

5. **License Automation:**
   - Gradle plugin integration
   - Auto-detect licenses from POM files
   - SPDX format support

## Documentation

- **User Guide:** `CONSENT_SAFETY_GUIDE.md`
- **License Inventory:** `THIRD_PARTY_NOTICES.md`
- **Implementation Notes:** This file

## Maintenance

### When Adding Dependencies

1. Add dependency to `app/build.gradle`
2. If license unknown, add to `KNOWN_LICENSES` in `scripts/license_audit.py`
3. Run `python3 scripts/license_audit.py`
4. Commit updated `THIRD_PARTY_NOTICES.md`

### When Updating Android Target SDK

1. Test crash handler on new SDK
2. Update consent items if new permissions needed
3. Test debug panel hardware detection
4. Update compatibility section in this doc

### When Adding Consent Items

1. Add constant to `ConsentLedger.kt`
2. Add item to `getConsentCatalog()`
3. Update strings.xml
4. Update `CONSENT_SAFETY_GUIDE.md`
5. Test ARMED → CONSENTED flow

## References

- [Android Exception Handling](https://developer.android.com/reference/java/lang/Thread.UncaughtExceptionHandler)
- [Android Permissions Best Practices](https://developer.android.com/training/permissions/requesting)
- [GDPR Data Minimization](https://gdpr-info.eu/art-5-gdpr/)
- [SPDX License List](https://spdx.org/licenses/)

---

**Version:** 1.0  
**Last Updated:** 2026-01-04  
**Maintainer:** UserLAnd Development Team
