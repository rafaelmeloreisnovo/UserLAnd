package tech.ula.utils

import android.content.SharedPreferences

data class SafetyActivation(
    val safeModeUntil: Long,
)

class SafetyStartupGuard(
    private val prefs: SharedPreferences,
    private val timeProvider: () -> Long = System::currentTimeMillis,
) {
    companion object {
        const val KEY_FAILURE_COUNT = "safety_guard_failure_count"
        const val KEY_LAST_FAILURE = "safety_guard_last_failure"
        const val KEY_SAFE_MODE_UNTIL = "safety_guard_safe_mode_until"
        const val KEY_LAST_TRIGGER = "safety_guard_last_trigger"
        const val KEY_LAST_REASON = "safety_guard_last_reason"

        const val MAX_FAILURES = 3
        const val WINDOW_MILLIS = 2 * 60 * 1000L
        const val SAFE_MODE_DURATION_MILLIS = 5 * 60 * 1000L
    }

    fun isSafeModeActive(): Boolean = timeProvider() < prefs.getLong(KEY_SAFE_MODE_UNTIL, 0L)

    fun recordFailure(reason: String): SafetyActivation? {
        val now = timeProvider()
        val lastFailure = prefs.getLong(KEY_LAST_FAILURE, 0L)
        var failures = prefs.getInt(KEY_FAILURE_COUNT, 0)

        if (now - lastFailure > WINDOW_MILLIS) {
            failures = 0
        }

        failures += 1
        var activated: SafetyActivation? = null

        val editor = prefs.edit()
            .putLong(KEY_LAST_FAILURE, now)

        if (failures >= MAX_FAILURES) {
            val safeModeUntil = now + SAFE_MODE_DURATION_MILLIS
            editor
                .putInt(KEY_FAILURE_COUNT, 0)
                .putLong(KEY_SAFE_MODE_UNTIL, safeModeUntil)
                .putLong(KEY_LAST_TRIGGER, now)
                .putString(KEY_LAST_REASON, reason)
            activated = SafetyActivation(safeModeUntil)
        } else {
            editor.putInt(KEY_FAILURE_COUNT, failures)
        }

        editor.apply()
        return activated
    }

    fun clearFailures() {
        prefs.edit()
            .remove(KEY_FAILURE_COUNT)
            .remove(KEY_LAST_FAILURE)
            .apply()
    }

    fun clearSafeMode() {
        prefs.edit()
            .remove(KEY_SAFE_MODE_UNTIL)
            .apply()
    }

    fun clearLogs() {
        prefs.edit()
            .remove(KEY_LAST_TRIGGER)
            .remove(KEY_LAST_REASON)
            .apply()
    }

    fun lastTriggerTime(): Long = prefs.getLong(KEY_LAST_TRIGGER, 0L)

    fun lastReason(): String? = prefs.getString(KEY_LAST_REASON, null)

    fun safeModeUntil(): Long = prefs.getLong(KEY_SAFE_MODE_UNTIL, 0L)
}
