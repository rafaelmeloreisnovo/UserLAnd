package tech.ula.utils

import android.app.ActivityManager
import android.content.Context
import android.util.Log
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

data class SafetyStatus(
    val lowMemory: Boolean,
    val availableMB: Long,
    val thresholdMB: Long,
)

object SafetyMonitor {
    private const val TAG = "SafetyMonitor"
    private const val LOW_MEMORY_THRESHOLD_MB = 50
    private const val SAFETY_LOG_FILE = "safety_monitor.log"

    const val KEY_SAFETY_MONITOR_ENABLED = "pref_safety_monitor_enabled"
    const val KEY_SAFETY_INDICATOR_ENABLED = "pref_safety_indicator_enabled"
    const val KEY_SAFETY_LOGS_ENABLED = "pref_safety_logs_enabled"
    const val KEY_SAFETY_SAFE_MODE = "pref_safety_safe_mode"

    fun isSafeModeActive(context: Context): Boolean {
        return context.defaultSharedPreferences.getBoolean(KEY_SAFETY_SAFE_MODE, false)
    }

    fun exitSafeMode(context: Context) {
        context.defaultSharedPreferences.edit().putBoolean(KEY_SAFETY_SAFE_MODE, false).apply()
        appendLog(context, "SAFE_MODE_EXITED")
    }

    fun markCrash(context: Context, throwable: Throwable) {
        context.defaultSharedPreferences.edit().putBoolean(KEY_SAFETY_SAFE_MODE, true).apply()
        appendLog(
            context,
            "CRASH_DETECTED type=${throwable.javaClass.simpleName} message=${throwable.message ?: ""}"
        )
    }

    fun isMonitorEnabled(context: Context): Boolean {
        return context.defaultSharedPreferences.getBoolean(KEY_SAFETY_MONITOR_ENABLED, true)
    }

    fun isIndicatorEnabled(context: Context): Boolean {
        return context.defaultSharedPreferences.getBoolean(KEY_SAFETY_INDICATOR_ENABLED, true)
    }

    fun updateStatus(context: Context): SafetyStatus {
        if (isSafeModeActive(context) || !isMonitorEnabled(context)) {
            return SafetyStatus(false, 0, 0)
        }

        return try {
            val activityManager =
                context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
            activityManager?.let {
                val memoryInfo = ActivityManager.MemoryInfo()
                it.getMemoryInfo(memoryInfo)

                val availableMB = memoryInfo.availMem / (1024 * 1024)
                val thresholdMB = memoryInfo.threshold / (1024 * 1024)
                val isLow = memoryInfo.lowMemory || availableMB < LOW_MEMORY_THRESHOLD_MB

                if (isLow) {
                    appendLog(
                        context,
                        "LOW_MEMORY available=${availableMB}MB threshold=${thresholdMB}MB"
                    )
                }

                SafetyStatus(isLow, availableMB, thresholdMB)
            } ?: SafetyStatus(false, 0, 0)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to update safety status", e)
            SafetyStatus(false, 0, 0)
        }
    }

    fun clearLogs(context: Context) {
        try {
            val file = File(context.filesDir, SAFETY_LOG_FILE)
            if (file.exists()) {
                file.delete()
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to clear safety logs", e)
        }
    }

    private fun appendLog(context: Context, message: String) {
        if (!context.defaultSharedPreferences.getBoolean(KEY_SAFETY_LOGS_ENABLED, false)) {
            return
        }

        try {
            val file = File(context.filesDir, SAFETY_LOG_FILE)
            val timestamp = SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.US)
                .format(Date())
            file.appendText("$timestamp $message\n")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to append safety log", e)
        }
    }
}
