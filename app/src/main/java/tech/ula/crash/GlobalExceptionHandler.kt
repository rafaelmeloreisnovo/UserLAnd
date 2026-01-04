package tech.ula.crash

import android.content.Context
import android.content.Intent
import android.os.Build
import android.util.Log
import org.json.JSONObject
import java.io.File
import java.io.PrintWriter
import java.io.StringWriter
import java.text.SimpleDateFormat
import java.util.*

/**
 * Global exception handler that captures uncaught exceptions and generates crash reports.
 * Ensures the app never crashes silently - always shows a user-friendly error screen.
 */
class GlobalExceptionHandler(
    private val context: Context,
    private val defaultHandler: Thread.UncaughtExceptionHandler?
) : Thread.UncaughtExceptionHandler {

    companion object {
        private const val TAG = "GlobalExceptionHandler"
        private const val CRASH_LOG_FILE = "crash_last.txt"
        private const val DEVICE_REPORT_FILE = "device_report.json"

        fun install(context: Context) {
            val currentHandler = Thread.getDefaultUncaughtExceptionHandler()
            if (currentHandler !is GlobalExceptionHandler) {
                Thread.setDefaultUncaughtExceptionHandler(
                    GlobalExceptionHandler(context.applicationContext, currentHandler)
                )
            }
        }
    }

    override fun uncaughtException(thread: Thread, throwable: Throwable) {
        try {
            Log.e(TAG, "Uncaught exception on thread ${thread.name}", throwable)
            
            // Save crash report
            try {
                saveCrashReport(thread, throwable)
            } catch (e: Exception) {
                Log.e(TAG, "Failed to save crash report", e)
            }
            
            // Save device report
            try {
                saveDeviceReport(throwable)
            } catch (e: Exception) {
                Log.e(TAG, "Failed to save device report", e)
            }
            
            // Launch crash report activity with multiple fallback attempts
            try {
                launchCrashReportActivity(throwable)
            } catch (e: Exception) {
                Log.e(TAG, "Failed to launch crash report activity", e)
                // Try alternative approach - direct launch without extras
                try {
                    val intent = Intent(context, CrashReportActivity::class.java).apply {
                        flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
                    }
                    context.startActivity(intent)
                } catch (e2: Exception) {
                    Log.e(TAG, "Alternative crash activity launch also failed", e2)
                }
            }
            
            // Small delay to let crash activity start
            try {
                Thread.sleep(200)
            } catch (e: InterruptedException) {
                // Ignore
            }
            
        } catch (e: Exception) {
            Log.e(TAG, "Critical error in exception handler", e)
        } finally {
            // Let default handler finish (this will terminate the process)
            // Note: This will call System.exit() or similar, but it's the proper way
            // to let the system handle the crash after we've saved our diagnostics
            try {
                defaultHandler?.uncaughtException(thread, throwable)
            } catch (e: Exception) {
                // If default handler also fails, log but let process terminate naturally
                Log.e(TAG, "Default exception handler also failed", e)
            }
        }
    }

    private fun saveCrashReport(thread: Thread, throwable: Throwable) {
        try {
            val filesDir = context.filesDir
            val crashFile = File(filesDir, CRASH_LOG_FILE)
            
            val timestamp = SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.US)
                .format(Date())
            
            val stackTrace = StringWriter().apply {
                throwable.printStackTrace(PrintWriter(this))
            }.toString()
            
            val report = buildString {
                appendLine("=== UserLAnd Crash Report ===")
                appendLine("Timestamp: $timestamp")
                appendLine("Thread: ${thread.name}")
                appendLine("Exception: ${throwable.javaClass.name}")
                appendLine("Message: ${throwable.message}")
                appendLine()
                appendLine("Stack Trace:")
                appendLine(stackTrace)
                appendLine()
                appendLine("Caused by chain:")
                var cause = throwable.cause
                while (cause != null) {
                    appendLine("  Caused by: ${cause.javaClass.name}: ${cause.message}")
                    cause = cause.cause
                }
            }
            
            crashFile.writeText(report)
            Log.i(TAG, "Crash report saved to ${crashFile.absolutePath}")
            
        } catch (e: Exception) {
            Log.e(TAG, "Failed to save crash report", e)
        }
    }

    private fun saveDeviceReport(throwable: Throwable) {
        try {
            val filesDir = context.filesDir
            val deviceFile = File(filesDir, DEVICE_REPORT_FILE)
            
            val report = JSONObject().apply {
                put("timestamp", System.currentTimeMillis())
                put("timestamp_human", SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(Date()))
                put("exception_type", throwable.javaClass.name)
                put("exception_message", throwable.message ?: "")
                
                // Device info
                val deviceInfo = JSONObject().apply {
                    put("manufacturer", Build.MANUFACTURER)
                    put("model", Build.MODEL)
                    put("device", Build.DEVICE)
                    put("product", Build.PRODUCT)
                    put("board", Build.BOARD)
                    put("hardware", Build.HARDWARE)
                    put("brand", Build.BRAND)
                    put("fingerprint", Build.FINGERPRINT)
                }
                put("device", deviceInfo)
                
                // OS info
                val osInfo = JSONObject().apply {
                    put("android_version", Build.VERSION.RELEASE)
                    put("sdk_int", Build.VERSION.SDK_INT)
                    put("codename", Build.VERSION.CODENAME)
                    put("security_patch", Build.VERSION.SECURITY_PATCH)
                }
                put("os", osInfo)
                
                // CPU info
                val cpuInfo = JSONObject().apply {
                    put("supported_abis", Build.SUPPORTED_ABIS.joinToString(", "))
                    if (Build.VERSION.SDK_INT >= 21) {
                        put("supported_32bit_abis", Build.SUPPORTED_32_BIT_ABIS.joinToString(", "))
                        put("supported_64bit_abis", Build.SUPPORTED_64_BIT_ABIS.joinToString(", "))
                    }
                }
                put("cpu", cpuInfo)
                
                // Runtime info
                val runtime = Runtime.getRuntime()
                val runtimeInfo = JSONObject().apply {
                    put("max_memory_mb", runtime.maxMemory() / (1024 * 1024))
                    put("total_memory_mb", runtime.totalMemory() / (1024 * 1024))
                    put("free_memory_mb", runtime.freeMemory() / (1024 * 1024))
                    put("processors", runtime.availableProcessors())
                }
                put("runtime", runtimeInfo)
            }
            
            deviceFile.writeText(report.toString(2))
            Log.i(TAG, "Device report saved to ${deviceFile.absolutePath}")
            
        } catch (e: Exception) {
            Log.e(TAG, "Failed to save device report", e)
        }
    }

    private fun launchCrashReportActivity(throwable: Throwable) {
        try {
            val intent = Intent(context, CrashReportActivity::class.java).apply {
                flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
                putExtra("exception_type", throwable.javaClass.simpleName)
                putExtra("exception_message", throwable.message ?: "Unknown error")
            }
            context.startActivity(intent)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to launch crash report activity", e)
        }
    }
}
