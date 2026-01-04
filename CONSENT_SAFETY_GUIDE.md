# Consent & Safety Guide for UserLAnd

## Overview

UserLAnd implements a layered consent system to ensure you have full control over advanced features and understand their implications before enabling them.

## Consent Model: ARMED → CONSENTED

The app uses a two-stage consent process:

1. **ARMED**: You enable a feature in the Debug Panel, marking it for use
2. **CONSENTED**: On next app restart, you review and confirm consent

This ensures you never accidentally enable sensitive features and always understand what you're agreeing to.

## Default Safe Mode

By default, UserLAnd runs in "safe mode" with:
- ✅ Standard app functionality
- ✅ Basic diagnostics
- ❌ No advanced hardware access
- ❌ No automatic data export
- ❌ No special permissions

## Available Consent Items

### 1. Hardware Report (Local)

**What it does:**
- Collects CPU, RAM, GPU, sensor, and kernel information
- Helps diagnose app issues
- ALL data stays on your device

**Risks:**
- Minimal - no data is transmitted

**How to enable:**
1. Open Debug Panel (from menu)
2. Tap "Generate Report"
3. Restart app when prompted
4. Review and accept consent

**How to disable:**
- Open Debug Panel → Settings → Toggle off Hardware Report
- Or: Revoke all consents in Consent Review

### 2. Export Logs/Reports

**What it does:**
- Allows sharing crash reports and logs via other apps
- You control what gets shared

**Risks:**
- You might accidentally share sensitive data
- Always review files before sharing

**How to enable:**
1. Open Debug Panel
2. Tap "Export File"
3. Restart app when prompted
4. Review and accept consent

**How to disable:**
- Simply don't use the export feature
- The app never exports automatically

### 3. All Files Access

**What it does:**
- Opens Android Settings to request storage permission
- Required for advanced filesystem operations

**Risks:**
- Grants broad storage access
- Android will ask for your confirmation

**How to enable:**
1. Open Debug Panel
2. Tap "Request All Files Access"
3. Restart app when prompted
4. Review and accept consent
5. Grant permission in Android Settings

**How to disable:**
- Settings → Apps → UserLAnd → Permissions → Storage → Revoke

### 4. Developer Options Guide

**What it does:**
- Shows instructions for enabling Developer Options
- Opens Android Settings for you

**Risks:**
- Developer Options can affect system stability if misused
- This feature only shows instructions - you control what you enable

**How to enable:**
1. Open Debug Panel
2. Tap "Open Developer Options"
3. Follow instructions

**How to disable:**
- This only shows instructions, nothing to disable
- To disable Developer Options: Settings → System → Developer Options → Toggle off

## Crash Shield

UserLAnd includes a crash shield that prevents silent failures:

- All crashes are captured and logged
- You see a friendly error screen with details
- Reports can be copied or exported
- Device information is saved for debugging

**What's collected on crash:**
- Exception type and message
- Stack trace
- Device model and OS version
- CPU architecture
- Memory usage
- Timestamp

**What's NOT collected:**
- Personal data
- Account information
- File contents
- Network traffic

## Revoking All Consents

To revoke all consents and return to safe mode:

1. Open Debug Panel
2. Restart app
3. In Consent Review screen, tap "Deny All"

Or uninstall and reinstall the app (this clears all data).

## Privacy Guarantees

- ✅ No data is sent to servers automatically
- ✅ You control all exports via Android share dialog
- ✅ All reports stored locally in app's private storage
- ✅ Crash reports never include personal data
- ✅ Native library failure handled gracefully (app doesn't crash)

## Developer Options Safety

If you enable Developer Options, be cautious with:

- ❌ USB Debugging - only enable if you know what you're doing
- ❌ Mock Locations - can break location-based apps
- ❌ Stay Awake - drains battery
- ❌ Aggressive Doze - can break background services

Safe Developer Options:
- ✅ Show Layout Bounds - visual debugging
- ✅ Force RTL Layout - test right-to-left languages
- ✅ Transition Animation Scale - adjust animation speed

## Troubleshooting

### App crashes on launch

1. Clear app data: Settings → Apps → UserLAnd → Storage → Clear Data
2. Uninstall and reinstall
3. Check crash report in: Android/data/tech.ula/files/crash_last.txt (if accessible)

### Native library failed to load

- The app will run in "compatibility mode"
- Some features may be disabled
- No action needed - the app handles this gracefully

### Consent screen appears repeatedly

- Make sure to check the "I fully consent" checkbox
- Tap "Accept and Continue"
- If issue persists, clear app data

### Can't export reports

- Grant Storage permission: Settings → Apps → UserLAnd → Permissions
- Ensure you have consented to "Export Logs/Reports"
- Check available storage space

## Support

For issues or questions:
1. Check crash reports in Debug Panel
2. Review consent status in Debug Panel
3. Export diagnostic report for support
4. File issue on GitHub with exported report

## License & Compliance

See `THIRD_PARTY_NOTICES.md` for:
- Third-party licenses
- Dependency compliance
- Native library licenses

## Changes to This Policy

This guide applies to UserLAnd version 2.8.3+ with the consent system.

Last updated: 2026-01-04
