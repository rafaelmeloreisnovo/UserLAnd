package tech.ula.utils

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.provider.Settings
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat

/**
 * AdvancedPermissionsManager handles all permission requests for Android 15+
 * including debugging, system information, and hardware access permissions.
 * 
 * This manager provides comprehensive permission handling for:
 * - Developer mode permissions (DUMP, PACKAGE_USAGE_STATS, READ_LOGS, etc.)
 * - Hardware access permissions (CAMERA, BLUETOOTH, LOCATION, etc.)
 * - System diagnostics permissions
 * - Advanced debugging capabilities
 */
class AdvancedPermissionsManager(private val context: Context) {

    companion object {
        // Request codes for permission requests
        const val REQUEST_CODE_RUNTIME_PERMISSIONS = 1001
        const val REQUEST_CODE_USAGE_STATS = 1002
        const val REQUEST_CODE_SYSTEM_ALERT_WINDOW = 1003
        const val REQUEST_CODE_MANAGE_EXTERNAL_STORAGE = 1004
        const val REQUEST_CODE_WRITE_SETTINGS = 1005

        /**
         * Runtime permissions that can be requested normally
         */
        val RUNTIME_PERMISSIONS = arrayOf(
            Manifest.permission.ACCESS_NETWORK_STATE,
            Manifest.permission.INTERNET,
            Manifest.permission.ACCESS_WIFI_STATE,
            Manifest.permission.CHANGE_WIFI_STATE,
            Manifest.permission.CHANGE_NETWORK_STATE,
            Manifest.permission.FOREGROUND_SERVICE,
            Manifest.permission.POST_NOTIFICATIONS,
            Manifest.permission.WAKE_LOCK,
            Manifest.permission.GET_ACCOUNTS,
            Manifest.permission.BLUETOOTH,
            Manifest.permission.BLUETOOTH_ADMIN,
            Manifest.permission.VIBRATE,
            Manifest.permission.FLASHLIGHT,
            Manifest.permission.CAMERA,
            Manifest.permission.RECORD_AUDIO,
            Manifest.permission.ACCESS_FINE_LOCATION,
            Manifest.permission.ACCESS_COARSE_LOCATION,
            Manifest.permission.REQUEST_INSTALL_PACKAGES,
            Manifest.permission.REQUEST_DELETE_PACKAGES,
            Manifest.permission.READ_PHONE_STATE
        ).let { perms ->
            // Add Android 12+ Bluetooth permissions
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                perms + arrayOf(
                    Manifest.permission.BLUETOOTH_CONNECT,
                    Manifest.permission.BLUETOOTH_SCAN
                )
            } else {
                perms
            }
        }.let { perms ->
            // Add Android 13+ permissions
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                perms // Already added BLUETOOTH permissions above if needed
            } else {
                perms
            }
        }.let { perms ->
            // Add Android 14+ permissions
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                perms + arrayOf(
                    Manifest.permission.FOREGROUND_SERVICE_DATA_SYNC
                )
            } else {
                perms
            }
        }.let { perms ->
            // Add Android 15+ permissions
            if (AndroidVersion.isAndroid15Plus()) {
                perms + arrayOf(
                    Manifest.permission.FOREGROUND_SERVICE_SPECIAL_USE
                )
            } else {
                perms
            }
        }

        /**
         * Storage permissions (scoped to Android 12 and below)
         */
        val STORAGE_PERMISSIONS = if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.S_V2) {
            arrayOf(
                Manifest.permission.WRITE_EXTERNAL_STORAGE,
                Manifest.permission.READ_EXTERNAL_STORAGE
            )
        } else {
            emptyArray()
        }

        /**
         * Background location permission (Android 10+)
         */
        val BACKGROUND_LOCATION_PERMISSION = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            arrayOf(Manifest.permission.ACCESS_BACKGROUND_LOCATION)
        } else {
            emptyArray()
        }
    }

    /**
     * Check if all runtime permissions are granted
     */
    fun areRuntimePermissionsGranted(): Boolean {
        return RUNTIME_PERMISSIONS.all { permission ->
            ContextCompat.checkSelfPermission(context, permission) == PackageManager.PERMISSION_GRANTED
        }
    }

    /**
     * Check if storage permissions are granted (for Android 12 and below)
     */
    fun areStoragePermissionsGranted(): Boolean {
        return if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.S_V2) {
            STORAGE_PERMISSIONS.all { permission ->
                ContextCompat.checkSelfPermission(context, permission) == PackageManager.PERMISSION_GRANTED
            }
        } else {
            true // Not needed on Android 13+
        }
    }

    /**
     * Check if usage stats permission is granted
     */
    fun isUsageStatsPermissionGranted(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP_MR1) {
            try {
                val appOps = context.getSystemService(Context.APP_OPS_SERVICE) as android.app.AppOpsService
                val mode = appOps.checkOpNoThrow(
                    android.app.AppOpsManager.OPSTR_GET_USAGE_STATS,
                    android.os.Process.myUid(),
                    context.packageName
                )
                mode == android.app.AppOpsManager.MODE_ALLOWED
            } catch (e: Exception) {
                false
            }
        } else {
            false
        }
    }

    /**
     * Check if system alert window permission is granted
     */
    fun isSystemAlertWindowPermissionGranted(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            Settings.canDrawOverlays(context)
        } else {
            true // Not needed on older versions
        }
    }

    /**
     * Check if write settings permission is granted
     */
    fun isWriteSettingsPermissionGranted(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            Settings.System.canWrite(context)
        } else {
            true // Not needed on older versions
        }
    }

    /**
     * Check if manage external storage permission is granted
     */
    fun isManageExternalStoragePermissionGranted(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            try {
                android.os.Environment.isExternalStorageManager()
            } catch (e: Exception) {
                false
            }
        } else {
            areStoragePermissionsGranted()
        }
    }

    /**
     * Request runtime permissions
     */
    fun requestRuntimePermissions(activity: Activity) {
        val permissionsToRequest = RUNTIME_PERMISSIONS.filter { permission ->
            ContextCompat.checkSelfPermission(context, permission) != PackageManager.PERMISSION_GRANTED
        }.toTypedArray()

        if (permissionsToRequest.isNotEmpty()) {
            ActivityCompat.requestPermissions(
                activity,
                permissionsToRequest,
                REQUEST_CODE_RUNTIME_PERMISSIONS
            )
        }
    }

    /**
     * Request storage permissions (Android 12 and below)
     */
    fun requestStoragePermissions(activity: Activity) {
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.S_V2) {
            val permissionsToRequest = STORAGE_PERMISSIONS.filter { permission ->
                ContextCompat.checkSelfPermission(context, permission) != PackageManager.PERMISSION_GRANTED
            }.toTypedArray()

            if (permissionsToRequest.isNotEmpty()) {
                ActivityCompat.requestPermissions(
                    activity,
                    permissionsToRequest,
                    REQUEST_CODE_RUNTIME_PERMISSIONS
                )
            }
        }
    }

    /**
     * Request usage stats permission
     * Opens settings page where user must manually grant
     */
    fun requestUsageStatsPermission(activity: Activity) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP_MR1) {
            val intent = Intent(Settings.ACTION_USAGE_ACCESS_SETTINGS)
            intent.data = Uri.parse("package:${context.packageName}")
            activity.startActivityForResult(intent, REQUEST_CODE_USAGE_STATS)
        }
    }

    /**
     * Request system alert window permission
     * Opens settings page where user must manually grant
     */
    fun requestSystemAlertWindowPermission(activity: Activity) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            val intent = Intent(
                Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                Uri.parse("package:${context.packageName}")
            )
            activity.startActivityForResult(intent, REQUEST_CODE_SYSTEM_ALERT_WINDOW)
        }
    }

    /**
     * Request write settings permission
     * Opens settings page where user must manually grant
     */
    fun requestWriteSettingsPermission(activity: Activity) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            val intent = Intent(
                Settings.ACTION_MANAGE_WRITE_SETTINGS,
                Uri.parse("package:${context.packageName}")
            )
            activity.startActivityForResult(intent, REQUEST_CODE_WRITE_SETTINGS)
        }
    }

    /**
     * Request manage external storage permission
     * Opens settings page where user must manually grant
     */
    fun requestManageExternalStoragePermission(activity: Activity) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val intent = Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION)
            intent.data = Uri.parse("package:${context.packageName}")
            activity.startActivityForResult(intent, REQUEST_CODE_MANAGE_EXTERNAL_STORAGE)
        }
    }

    /**
     * Get a summary of all permission statuses
     */
    fun getPermissionsSummary(): Map<String, Boolean> {
        return mapOf(
            "runtimePermissions" to areRuntimePermissionsGranted(),
            "storagePermissions" to areStoragePermissionsGranted(),
            "usageStats" to isUsageStatsPermissionGranted(),
            "systemAlertWindow" to isSystemAlertWindowPermissionGranted(),
            "writeSettings" to isWriteSettingsPermissionGranted(),
            "manageExternalStorage" to isManageExternalStoragePermissionGranted()
        )
    }

    /**
     * Get detailed permission status report
     */
    fun getDetailedPermissionsReport(): String {
        return buildString {
            appendLine("=== Permission Status Report ===")
            appendLine()
            appendLine("Runtime Permissions: ${if (areRuntimePermissionsGranted()) "✓ Granted" else "✗ Not Granted"}")
            appendLine("Storage Permissions: ${if (areStoragePermissionsGranted()) "✓ Granted" else "✗ Not Granted"}")
            appendLine("Usage Stats: ${if (isUsageStatsPermissionGranted()) "✓ Granted" else "✗ Not Granted"}")
            appendLine("System Alert Window: ${if (isSystemAlertWindowPermissionGranted()) "✓ Granted" else "✗ Not Granted"}")
            appendLine("Write Settings: ${if (isWriteSettingsPermissionGranted()) "✓ Granted" else "✗ Not Granted"}")
            appendLine("Manage External Storage: ${if (isManageExternalStoragePermissionGranted()) "✓ Granted" else "✗ Not Granted"}")
            appendLine()
            appendLine("Individual Runtime Permissions:")
            RUNTIME_PERMISSIONS.forEach { permission ->
                val granted = ContextCompat.checkSelfPermission(context, permission) == PackageManager.PERMISSION_GRANTED
                appendLine("  ${permission.substringAfterLast('.')}: ${if (granted) "✓" else "✗"}")
            }
        }
    }
}
