package tech.ula.utils

import android.app.ActivityManager
import android.content.Context
import android.util.Log
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object SafetyGuard {
    private const val TAG = "SafetyGuard"

    const val KEY_ENABLED = "pref_safety_guard_enabled"
    const val KEY_INDICATOR_ENABLED = "pref_safety_guard_indicator"
    const val KEY_LOGGING_ENABLED = "pref_safety_guard_logging"
    const val KEY_LAST_EVENT = "pref_safety_guard_last_event"
    const val KEY_LAST_EVENT_AT = "pref_safety_guard_last_event_at"
    const val KEY_SAFE_MODE = "pref_safety_guard_safe_mode"
    const val KEY_STARTUP_MODULES = "pref_safety_guard_startup_modules"

    private const val WARNING_RUNTIME_USAGE_PERCENT = 70
    private const val CRITICAL_RUNTIME_USAGE_PERCENT = 85
    private const val MIN_AVAILABLE_MB_WARNING = 120
    private const val MIN_AVAILABLE_MB_CRITICAL = 80

    enum class Severity {
        Normal,
        Warning,
        Critical,
    }

    data class GuardState(
        val severity: Severity,
        val reason: String,
        val delayMillis: Long,
        val isTriggered: Boolean,
        val safeModeActive: Boolean,
    )

    fun evaluate(context: Context): GuardState {
        val prefs = context.defaultSharedPreferences
        val safeModeActive = prefs.getBoolean(KEY_SAFE_MODE, false)
        val memoryInfo = ActivityManager.MemoryInfo()
        val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
        activityManager?.getMemoryInfo(memoryInfo)

        val availableMB = (memoryInfo.availMem / (1024 * 1024)).toInt()
        val runtime = Runtime.getRuntime()
        val runtimeUsed = runtime.totalMemory() - runtime.freeMemory()
        val runtimePercentUsed = ((runtimeUsed.toDouble() / runtime.maxMemory()) * 100).toInt()

        val severity = when {
            memoryInfo.lowMemory || availableMB <= MIN_AVAILABLE_MB_CRITICAL || runtimePercentUsed >= CRITICAL_RUNTIME_USAGE_PERCENT -> Severity.Critical
            availableMB <= MIN_AVAILABLE_MB_WARNING || runtimePercentUsed >= WARNING_RUNTIME_USAGE_PERCENT -> Severity.Warning
            else -> Severity.Normal
        }

        val reason = when (severity) {
            Severity.Critical -> "critical_memory_pressure"
            Severity.Warning -> "elevated_memory_pressure"
            Severity.Normal -> "normal"
        }

        val delayMillis = when (severity) {
            Severity.Critical -> 1500L
            Severity.Warning -> 500L
            Severity.Normal -> 0L
        }

        val triggered = severity != Severity.Normal || safeModeActive

        return GuardState(
            severity = severity,
            reason = reason,
            delayMillis = delayMillis,
            isTriggered = triggered,
            safeModeActive = safeModeActive,
        )
    }

    fun recordStartupModules(context: Context) {
        val prefs = context.defaultSharedPreferences
        if (!prefs.getBoolean(KEY_LOGGING_ENABLED, true)) return

        val modules = startupModulesForSdk(Build.VERSION.SDK_INT)
        prefs.edit().putString(KEY_STARTUP_MODULES, modules.joinToString(", ")).apply()
        Log.i(TAG, "Startup safety modules: ${modules.joinToString(", ")}")
    }

    fun recordEvent(
        context: Context,
        state: GuardState,
        trigger: String,
    ) {
        val prefs = context.defaultSharedPreferences
        if (!prefs.getBoolean(KEY_LOGGING_ENABLED, true)) return
        if (!state.isTriggered) return

        val timestamp = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(Date())
        val message = "${state.reason} (severity=${state.severity}, safeMode=${state.safeModeActive}) via $trigger"

        prefs.edit()
            .putString(KEY_LAST_EVENT, message)
            .putString(KEY_LAST_EVENT_AT, timestamp)
            .apply()

        Log.w(TAG, "Safety guard triggered at $timestamp: $message")
    }

    fun setSafeMode(context: Context, enabled: Boolean) {
        val prefs = context.defaultSharedPreferences
        prefs.edit().putBoolean(KEY_SAFE_MODE, enabled).apply()
    }

    fun clearLastEvent(context: Context) {
        val prefs = context.defaultSharedPreferences
        prefs.edit()
            .remove(KEY_LAST_EVENT)
            .remove(KEY_LAST_EVENT_AT)
            .apply()
    }

    fun startupModulesForSdk(sdk: Int): List<String> {
        val baseModules = mutableListOf("memory_guard", "startup_throttle")
        if (AndroidVersion.isAndroid15Plus(sdk)) {
            baseModules.add("android15_pressure_watch")
        }
        return baseModules
    }
}
