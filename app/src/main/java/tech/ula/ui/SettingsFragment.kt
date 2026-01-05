package tech.ula.ui

import android.content.Context
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.graphics.drawable.Drawable
import android.os.Bundle
import java.text.DateFormat
import android.widget.Toast
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import tech.ula.R
import tech.ula.utils.ProotDebugLogger
import tech.ula.utils.SafetyStartupGuard
import tech.ula.utils.SafetyGuard
import tech.ula.utils.SafetyMonitor
import tech.ula.utils.UlaFiles
import tech.ula.utils.defaultSharedPreferences

class SettingsFragment : PreferenceFragmentCompat() {
    private val prootDebugLogger by lazy {
        val ulaFiles = UlaFiles(activity!!, activity!!.applicationInfo.nativeLibraryDir)
        ProotDebugLogger(activity!!.defaultSharedPreferences, ulaFiles)
    }

    override fun onCreatePreferences(
        savedInstanceState: Bundle?,
        rootKey: String?,
    ) {
        addPreferencesFromResource(R.xml.preferences)

        val deleteFilePreference: Preference = findPreference("pref_proot_delete_debug_file")!!
        deleteFilePreference.setOnPreferenceClickListener {
            prootDebugLogger.deleteLogs()
            true
        }

        val clearAutoStartPreference: Preference = findPreference("pref_clear_auto_start")!!
        clearAutoStartPreference.setOnPreferenceClickListener {
            val prefs = activity!!.getSharedPreferences("apps", Context.MODE_PRIVATE)
            with(prefs.edit()) {
                remove("AutoApp")
                apply()
                true
            }
        }

        val safetyGuardStatusPreference: Preference = findPreference("pref_safety_guard_status")!!
        val safetyGuardClearPreference: Preference = findPreference("pref_safety_guard_clear")!!
        val safetyGuard = SafetyStartupGuard(activity!!.defaultSharedPreferences)

        safetyGuardStatusPreference.summary = buildSafetyGuardSummary(safetyGuard)

        safetyGuardClearPreference.setOnPreferenceClickListener {
            safetyGuard.clearFailures()
            safetyGuard.clearSafeMode()
            safetyGuard.clearLogs()
            safetyGuardStatusPreference.summary = buildSafetyGuardSummary(safetyGuard)
            true
        }
    }

    private fun buildSafetyGuardSummary(safetyGuard: SafetyStartupGuard): String {
        val now = System.currentTimeMillis()
        val dateFormat = DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.SHORT)
        val safeModeUntil = safetyGuard.safeModeUntil()
        val lastTriggerTime = safetyGuard.lastTriggerTime()
        val lastReason = safetyGuard.lastReason()

        return when {
            safeModeUntil > now -> {
                getString(R.string.pref_safety_guard_status_active, dateFormat.format(safeModeUntil))
            }

            lastTriggerTime > 0L -> {
                val reason = lastReason ?: "unknown"
                getString(
                    R.string.pref_safety_guard_status_triggered,
                    dateFormat.format(lastTriggerTime),
                    reason,
                )
            }

            else -> {
                getString(R.string.pref_safety_guard_status_none)
            }
        }
    }

    override fun setDivider(divider: Drawable?) {
        super.setDivider(ColorDrawable(Color.TRANSPARENT))
    }

    override fun setDividerHeight(height: Int) {
        super.setDividerHeight(0)
    }
}
