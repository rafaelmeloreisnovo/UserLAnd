# Android 15 (API 35) Compatibility Guide

This document details all changes made to ensure UserLAnd operates robustly on Android 15 (API 35) while maintaining backward compatibility with earlier Android versions.

## Table of Contents

1. [Android 15 Core Compatibility Changes](#android-15-core-compatibility-changes)
2. [Permission Updates](#permission-updates)
3. [Foreground Service Changes](#foreground-service-changes)
4. [Storage Model Updates](#storage-model-updates)
5. [Security Hardening](#security-hardening)
6. [Diagnostic System](#diagnostic-system)
7. [Known Limitations](#known-limitations)
8. [Migration Notes](#migration-notes)

## Android 15 Core Compatibility Changes

### AndroidManifest.xml Updates

#### Component Export Declaration
All components with intent-filters now explicitly declare `android:exported`:
- **MainActivity**: `android:exported="true"` - Required for LAUNCHER intent
- **ServerService**: `android:exported="false"` - Internal service, not exposed
- **UlaDocProvider**: `android:exported="true"` - Document provider for external access

**Rationale**: Android 12+ requires explicit export declaration for security. This prevents accidental exposure of internal components.

#### Foreground Service Type
```xml
<service
    android:name=".ServerService"
    android:stopWithTask="true"
    android:foregroundServiceType="dataSync"
    android:exported="false" />
```

**Rationale**: Android 14+ requires declaration of foreground service types. UserLAnd's ServerService manages filesystem synchronization and session data, making `dataSync` the most appropriate type.

### Permission Updates

#### New Permissions
```xml
<!-- Android 13+ notification permission -->
<uses-permission android:name="android.permission.POST_NOTIFICATIONS" />

<!-- Android 14+ foreground service type permission -->
<uses-permission android:name="android.permission.FOREGROUND_SERVICE_DATA_SYNC" />
```

#### Updated Storage Permissions
```xml
<!-- Legacy storage permissions scoped to Android 12 and below -->
<uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE"
    android:maxSdkVersion="32" />
<uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE"
    android:maxSdkVersion="32" />
```

**Rationale**: 
- Android 13+ uses scoped storage model; legacy permissions are no longer needed
- POST_NOTIFICATIONS is runtime permission on Android 13+
- Scoping old permissions prevents unnecessary permission requests on modern devices

## Permission Updates

### PermissionHandler Modernization

The `PermissionHandler` class has been updated to handle Android's evolving permission model:

#### Scoped Storage (Android 13+)
```kotlin
fun permissionsAreGranted(context: Context): Boolean {
    // Android 13+ uses scoped storage, no need for legacy storage permissions
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        return true
    }
    // Check legacy permissions on older Android versions
    ...
}
```

#### Notification Permission
New methods for handling POST_NOTIFICATIONS permission:
- `notificationPermissionGranted()`: Check if permission is granted
- `requestNotificationPermission()`: Request permission at runtime
- `notificationPermissionWasGranted()`: Handle permission result

**Best Practice**: Request notification permission when user first starts a session, not on app launch.

## Foreground Service Changes

### Service Type Declaration
UserLAnd's `ServerService` is now declared with `foregroundServiceType="dataSync"` because it:
- Manages session data and state
- Synchronizes filesystem operations
- Coordinates proot executions

### Permission Requirement
Added `FOREGROUND_SERVICE_DATA_SYNC` permission to manifest for Android 14+ compatibility.

## Storage Model Updates

### Android 13+ Scoped Storage
UserLAnd now fully embraces scoped storage on Android 13+:
- App uses internal storage (`Context.getFilesDir()`) for all filesystem operations
- No direct access to external storage
- Legacy `READ_EXTERNAL_STORAGE` and `WRITE_EXTERNAL_STORAGE` permissions only requested on Android 12 and below

### FileProvider for Sharing
Added FileProvider configuration for secure file sharing:
```xml
<provider
    android:name="androidx.core.content.FileProvider"
    android:authorities="${applicationId}.fileprovider"
    android:exported="false"
    android:grantUriPermissions="true">
    <meta-data
        android:name="android.support.FILE_PROVIDER_PATHS"
        android:resource="@xml/file_paths" />
</provider>
```

Used for sharing diagnostic reports without exposing internal file paths.

## Security Hardening

### Password Sanitization in Logs
Added `sanitizeEnvForLogging()` function in `BusyboxExecutor` to prevent credential leaks:

```kotlin
private fun sanitizeEnvForLogging(env: Map<String, String>): Map<String, String> {
    val sensitiveKeys = setOf(
        "INITIAL_PASSWORD", "INITIAL_VNC_PASSWORD",
        "PASSWORD", "VNC_PASSWORD", "PASS",
        "SECRET", "TOKEN", "KEY"
    )
    
    return env.mapValues { (key, value) ->
        if (sensitiveKeys.any { key.contains(it, ignoreCase = true) }) {
            "***REDACTED***"
        } else {
            value
        }
    }
}
```

**Impact**: All environment variable logging now sanitizes passwords and secrets.

### PendingIntent Flags
All PendingIntents already use `FLAG_IMMUTABLE` on Android 12+ as required:
- `NotificationConstructor`: Uses `FLAG_IMMUTABLE` for all notification PendingIntents
- Mutable PendingIntents only created when absolutely necessary with explicit `FLAG_MUTABLE`

## Diagnostic System

### New Features
Added comprehensive diagnostic system for troubleshooting:

#### DiagnosticInfo
Data class containing system information without PII:
- App version and build info
- Android version and SDK level
- Device manufacturer and model (generic)
- Supported ABIs
- Permission status
- Filesystem checks
- Native library status

#### DiagnosticChecker
Runtime verification utility that checks:
- ABI compatibility
- File system paths and writability
- Storage availability
- Native library presence (busybox, proot)
- Permission grants

#### DiagnosticFragment
New UI screen accessible from Settings that allows:
- Running system diagnostics
- Viewing diagnostic report
- Copying report to clipboard
- Sharing report via Intent (for support requests)
- Requesting notification permission if needed

### Privacy Protection
All diagnostic data is sanitized:
- File paths use generic markers: `/data/user/<uid>/`
- No usernames or personal data
- No filesystem content
- No session credentials

## Known Limitations

### Default VNC Password
- Default VNC password "userland" is hardcoded in database migrations (UlaDatabase.kt:95-96)
- This is a legacy design decision for ease of use
- Users should change default passwords during filesystem setup
- **Recommendation**: Consider removing default and forcing user to set password

### Sentry Migration Pending
Logger.kt contains stubbed Sentry integration with TODOs for migration to Sentry 5.x+ API:
- Currently logs to Android Log instead
- Breadcrumb system implemented but not sent to Sentry
- Full migration deferred to separate task

### Billing Library Version
BillingManager.kt contains TODOs for migrating to newer billing library APIs:
- Currently using deprecated `queryPurchases()` synchronous method
- Should migrate to `queryPurchasesAsync()` for billing library 6.x+
- Billing downgraded to 6.2.1 for Kotlin 2.0.x compatibility

## Migration Notes

### For Developers

#### Building on Android 15
- Project now targets SDK 35 (Android 15)
- Minimum SDK remains 23 (Android 6.0)
- All builds tested on API 23-35 range

#### Testing Checklist
1. Verify notification permission request on Android 13+
2. Test foreground service starts correctly with dataSync type
3. Confirm storage operations work on all Android versions
4. Verify diagnostic system exports reports correctly
5. Check that no sensitive data appears in logs

### For Users

#### Android 13+ Changes
- Will see notification permission request when starting first session
- No more storage permission requests (scoped storage is automatic)
- Existing filesystems and sessions migrate automatically

#### Android 14+ Changes
- Foreground service behavior unchanged from user perspective
- "Session running" notification continues to work as before

#### Android 15 Changes
- All existing features work without user intervention
- New diagnostic feature available in Settings

## References

- [Android 15 Behavior Changes](https://developer.android.com/about/versions/15/behavior-changes-all)
- [Foreground Service Types](https://developer.android.com/about/versions/14/changes/fgs-types-required)
- [Scoped Storage](https://developer.android.com/about/versions/11/privacy/storage)
- [Notification Permission](https://developer.android.com/develop/ui/views/notifications/notification-permission)
- [PendingIntent Mutability](https://developer.android.com/about/versions/12/behavior-changes-12#pending-intent-mutability)

## Version History

- **2026-01-04**: Initial Android 15 compatibility implementation
  - Added foreground service type declaration
  - Updated permission handling for Android 13+
  - Implemented diagnostic system
  - Added password sanitization in logs
  - Updated documentation
