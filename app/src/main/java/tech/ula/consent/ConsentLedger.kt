package tech.ula.consent

import android.content.Context
import android.content.SharedPreferences
import android.util.Log
import org.json.JSONArray
import org.json.JSONObject

/**
 * Persistent consent ledger that tracks user consent for features.
 * Features move from NONE → ARMED → CONSENTED states.
 */
class ConsentLedger(context: Context) {
    
    companion object {
        private const val TAG = "ConsentLedger"
        private const val PREFS_NAME = "consent_ledger"
        private const val KEY_CONSENT_DATA = "consent_data"
        
        // Consent catalog items
        const val CONSENT_HARDWARE_REPORT = "hardware_report"
        const val CONSENT_EXPORT_LOGS = "export_logs"
        const val CONSENT_ALL_FILES = "all_files_access"
        const val CONSENT_DEV_OPTIONS_GUIDE = "dev_options_guide"
    }
    
    private val prefs: SharedPreferences = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    
    /**
     * Get the consent catalog with all available items.
     */
    fun getConsentCatalog(): List<ConsentItem> = listOf(
        ConsentItem(
            id = CONSENT_HARDWARE_REPORT,
            title = "Hardware Report (Local)",
            description = "Collect detailed device information for diagnostics",
            benefit = "Helps troubleshoot issues by providing CPU, RAM, GPU, sensors, and kernel information",
            risk = "Minimal - all data stays on your device",
            howToDisable = "Toggle off in Debug Panel settings",
            requiresReconfirmation = false
        ),
        ConsentItem(
            id = CONSENT_EXPORT_LOGS,
            title = "Export Logs/Reports",
            description = "Share diagnostic files via other apps",
            benefit = "Easily share crash reports and logs for support",
            risk = "You control what gets shared, but ensure you don't share sensitive data",
            howToDisable = "Simply don't use the export feature",
            requiresReconfirmation = true
        ),
        ConsentItem(
            id = CONSENT_ALL_FILES,
            title = "All Files Access",
            description = "Request permission to access all storage (opens Settings)",
            benefit = "Required for advanced filesystem operations",
            risk = "Grants broad storage access - Android will ask for confirmation",
            howToDisable = "Revoke permission in Android Settings → Apps → UserLAnd → Permissions",
            requiresReconfirmation = true
        ),
        ConsentItem(
            id = CONSENT_DEV_OPTIONS_GUIDE,
            title = "Developer Options Guide",
            description = "Instructions for enabling Developer Options and Wireless Debugging",
            benefit = "Unlocks advanced Android debugging features",
            risk = "Developer Options can affect system stability if misused",
            howToDisable = "This only shows instructions - you control what you enable in Settings",
            requiresReconfirmation = false
        )
    )
    
    /**
     * Get the current consent state for a feature.
     */
    fun getConsentState(itemId: String): ConsentState {
        val data = loadConsentData()
        return when (data.optString(itemId, "NONE")) {
            "ARMED" -> ConsentState.ARMED
            "CONSENTED" -> ConsentState.CONSENTED
            else -> ConsentState.NONE
        }
    }
    
    /**
     * Set the consent state for a feature.
     */
    fun setConsentState(itemId: String, state: ConsentState) {
        val data = loadConsentData()
        data.put(itemId, state.name)
        saveConsentData(data)
        Log.i(TAG, "Consent state for $itemId set to $state")
    }
    
    /**
     * Arm a feature (NONE → ARMED).
     * Will require reconfirmation on next app launch.
     */
    fun armFeature(itemId: String) {
        if (getConsentState(itemId) == ConsentState.NONE) {
            setConsentState(itemId, ConsentState.ARMED)
        }
    }
    
    /**
     * Grant consent for a feature (ARMED → CONSENTED).
     */
    fun grantConsent(itemId: String) {
        if (getConsentState(itemId) == ConsentState.ARMED) {
            setConsentState(itemId, ConsentState.CONSENTED)
        }
    }
    
    /**
     * Revoke consent for a feature (any state → NONE).
     */
    fun revokeConsent(itemId: String) {
        setConsentState(itemId, ConsentState.NONE)
    }
    
    /**
     * Revoke all consents.
     */
    fun revokeAll() {
        getConsentCatalog().forEach { item ->
            revokeConsent(item.id)
        }
        Log.i(TAG, "All consents revoked")
    }
    
    /**
     * Check if there are any ARMED features pending consent.
     */
    fun hasPendingConsents(): Boolean {
        return getConsentCatalog().any { item ->
            getConsentState(item.id) == ConsentState.ARMED
        }
    }
    
    /**
     * Get list of features with ARMED state.
     */
    fun getPendingConsentItems(): List<ConsentItem> {
        return getConsentCatalog().filter { item ->
            getConsentState(item.id) == ConsentState.ARMED
        }
    }
    
    /**
     * Check if a feature is fully consented.
     */
    fun isConsented(itemId: String): Boolean {
        return getConsentState(itemId) == ConsentState.CONSENTED
    }
    
    /**
     * Mark that the consent review has been shown for current session.
     */
    fun markConsentReviewShown() {
        prefs.edit().putBoolean("consent_review_shown", true).apply()
    }
    
    /**
     * Check if consent review has been shown in current session.
     */
    fun wasConsentReviewShown(): Boolean {
        return prefs.getBoolean("consent_review_shown", false)
    }
    
    private fun loadConsentData(): JSONObject {
        val json = prefs.getString(KEY_CONSENT_DATA, "{}")
        return try {
            JSONObject(json ?: "{}")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load consent data", e)
            JSONObject()
        }
    }
    
    private fun saveConsentData(data: JSONObject) {
        prefs.edit().putString(KEY_CONSENT_DATA, data.toString()).apply()
    }
}
