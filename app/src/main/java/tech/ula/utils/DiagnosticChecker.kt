package tech.ula.utils

import android.content.Context
import android.os.Build
import android.os.Environment
import tech.ula.BuildConfig
import java.io.File

/**
 * DiagnosticChecker - Runtime verification utility
 *
 * Performs comprehensive system checks for troubleshooting:
 * - ABI compatibility
 * - File system paths and permissions
 * - Storage availability
 * - Native library presence
 *
 * All checks are designed to avoid exposing PII (personally identifiable information).
 */
class DiagnosticChecker(
    private val context: Context,
) {
    /**
     * Perform all diagnostic checks and return a comprehensive report.
     */
    fun performDiagnostics(): DiagnosticInfo =
        DiagnosticInfo(
            timestamp = System.currentTimeMillis(),
            appVersionName = BuildConfig.VERSION_NAME,
            appVersionCode = BuildConfig.VERSION_CODE,
            androidVersion = Build.VERSION.RELEASE,
            androidSdkInt = Build.VERSION.SDK_INT,
            deviceManufacturer = Build.MANUFACTURER,
            deviceModel = Build.MODEL,
            supportedAbis = getSupportedAbis(),
            storagePermissionStatus = checkStoragePermission(),
            notificationPermissionStatus = checkNotificationPermission(),
            filesystemChecks = checkFilesystem(),
            nativeLibraryStatus = checkNativeLibraries(),
        )

    /**
     * Get list of supported ABIs for this device.
     */
    private fun getSupportedAbis(): List<String> =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            Build.SUPPORTED_ABIS.toList()
        } else {
            @Suppress("DEPRECATION")
            listOf(Build.CPU_ABI, Build.CPU_ABI2).filter { it.isNotEmpty() }
        }

    /**
     * Check storage permission status.
     * On Android 13+, scoped storage is used, so legacy permissions are not required.
     */
    private fun checkStoragePermission(): PermissionStatus =
        when {
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU -> PermissionStatus.NOT_REQUIRED
            PermissionHandler.permissionsAreGranted(context) -> PermissionStatus.GRANTED
            else -> PermissionStatus.DENIED
        }

    /**
     * Check notification permission status.
     * Required on Android 13+ (API 33+).
     */
    private fun checkNotificationPermission(): PermissionStatus =
        when {
            Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU -> PermissionStatus.NOT_REQUIRED
            PermissionHandler.notificationPermissionGranted(context) -> PermissionStatus.GRANTED
            else -> PermissionStatus.DENIED
        }

    /**
     * Check filesystem paths and writability.
     */
    private fun checkFilesystem(): FilesystemChecks {
        val filesDir = context.filesDir
        val internalStorageState = Environment.getExternalStorageState()

        return FilesystemChecks(
            internalStorageAvailable = internalStorageState == Environment.MEDIA_MOUNTED,
            internalStorageWritable = internalStorageState == Environment.MEDIA_MOUNTED,
            filesDirExists = filesDir.exists(),
            filesDirWritable = filesDir.canWrite(),
            filesDirPath = sanitizePath(filesDir.absolutePath),
        )
    }

    /**
     * Check if native libraries (busybox, proot) are present.
     */
    private fun checkNativeLibraries(): NativeLibraryStatus {
        val nativeLibDir = context.applicationInfo.nativeLibraryDir
        val nativeLibDirFile = File(nativeLibDir)

        val busyboxPresent = checkLibraryPresent(nativeLibDir, "libbusybox.so")
        val prootPresent = checkLibraryPresent(nativeLibDir, "libproot.so")

        return NativeLibraryStatus(
            busyboxPresent = busyboxPresent,
            prootPresent = prootPresent,
            nativeLibraryDir = sanitizePath(nativeLibDir),
            nativeLibDirReadable = nativeLibDirFile.canRead(),
        )
    }

    /**
     * Check if a specific native library file exists.
     */
    private fun checkLibraryPresent(
        libDir: String,
        libraryName: String,
    ): Boolean {
        val libFile = File(libDir, libraryName)
        return libFile.exists() && libFile.canRead()
    }

    /**
     * Sanitize file paths to avoid exposing user-specific information.
     * Replaces user-specific parts with generic markers.
     */
    private fun sanitizePath(path: String): String {
        // Replace package-specific paths with generic markers
        return path
            .replace(context.packageName, "<package>")
            .replace(Regex("/data/user/\\d+/"), "/data/user/<uid>/")
            .replace(Regex("/storage/emulated/\\d+/"), "/storage/emulated/<uid>/")
    }
}
