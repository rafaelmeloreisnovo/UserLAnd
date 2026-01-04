package tech.ula.utils

import android.os.Build
import android.os.Parcelable
import kotlinx.parcelize.Parcelize

/**
 * DiagnosticInfo - Data class for runtime diagnostics
 * 
 * Contains system information for troubleshooting without exposing PII.
 * All fields are sanitized to avoid sensitive data exposure.
 */
@Parcelize
data class DiagnosticInfo(
    val timestamp: Long,
    val appVersionName: String,
    val appVersionCode: Int,
    val androidVersion: String,
    val androidSdkInt: Int,
    val deviceManufacturer: String,
    val deviceModel: String,
    val supportedAbis: List<String>,
    val storagePermissionStatus: PermissionStatus,
    val notificationPermissionStatus: PermissionStatus,
    val filesystemChecks: FilesystemChecks,
    val nativeLibraryStatus: NativeLibraryStatus,
) : Parcelable {
    /**
     * Export as a human-readable text report (no PII).
     */
    fun toReport(): String = buildString {
        appendLine("=== UserLAnd Diagnostic Report ===")
        appendLine()
        appendLine("Timestamp: ${java.text.SimpleDateFormat("yyyy-MM-dd HH:mm:ss", java.util.Locale.US).format(java.util.Date(timestamp))}")
        appendLine()
        appendLine("-- Application --")
        appendLine("Version: $appVersionName ($appVersionCode)")
        appendLine()
        appendLine("-- Device --")
        appendLine("Android Version: $androidVersion (SDK $androidSdkInt)")
        appendLine("Manufacturer: $deviceManufacturer")
        appendLine("Model: $deviceModel")
        appendLine("Supported ABIs: ${supportedAbis.joinToString(", ")}")
        appendLine()
        appendLine("-- Permissions --")
        appendLine("Storage: $storagePermissionStatus")
        appendLine("Notifications: $notificationPermissionStatus")
        appendLine()
        appendLine("-- Filesystem --")
        appendLine("Internal Storage Available: ${filesystemChecks.internalStorageAvailable}")
        appendLine("Internal Storage Writable: ${filesystemChecks.internalStorageWritable}")
        appendLine("Files Dir Exists: ${filesystemChecks.filesDirExists}")
        appendLine("Files Dir Writable: ${filesystemChecks.filesDirWritable}")
        appendLine()
        appendLine("-- Native Libraries --")
        appendLine("Busybox Present: ${nativeLibraryStatus.busyboxPresent}")
        appendLine("Proot Present: ${nativeLibraryStatus.prootPresent}")
        appendLine("Native Lib Dir: ${nativeLibraryStatus.nativeLibraryDir}")
        appendLine("Native Lib Dir Readable: ${nativeLibraryStatus.nativeLibDirReadable}")
        appendLine()
        appendLine("=== End of Report ===")
    }
}

@Parcelize
data class FilesystemChecks(
    val internalStorageAvailable: Boolean,
    val internalStorageWritable: Boolean,
    val filesDirExists: Boolean,
    val filesDirWritable: Boolean,
    val filesDirPath: String,
) : Parcelable

@Parcelize
data class NativeLibraryStatus(
    val busyboxPresent: Boolean,
    val prootPresent: Boolean,
    val nativeLibraryDir: String,
    val nativeLibDirReadable: Boolean,
) : Parcelable

enum class PermissionStatus {
    GRANTED,
    DENIED,
    NOT_REQUIRED,
}
