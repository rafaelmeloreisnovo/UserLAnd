package tech.ula.consent

/**
 * Represents a feature that requires user consent.
 */
data class ConsentItem(
    val id: String,
    val title: String,
    val description: String,
    val benefit: String,
    val risk: String,
    val howToDisable: String,
    val requiresReconfirmation: Boolean = false
)

/**
 * Consent state for a feature.
 */
enum class ConsentState {
    /** Not armed, default safe mode */
    NONE,
    
    /** Armed in the debug panel but not yet consented */
    ARMED,
    
    /** Fully consented and active */
    CONSENTED
}
