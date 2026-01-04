package tech.ula.consent

import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.CheckBox
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import tech.ula.MainActivity
import tech.ula.R
import tech.ula.databinding.ActivityConsentReviewBinding

/**
 * Activity shown when there are ARMED features requiring user consent.
 * User must review and explicitly consent to each feature.
 */
class ConsentReviewActivity : AppCompatActivity() {
    
    private lateinit var binding: ActivityConsentReviewBinding
    private lateinit var consentLedger: ConsentLedger
    private val pendingItems = mutableListOf<ConsentItem>()
    
    override fun onCreate(savedInstanceState: Bundle?) {
        try {
            super.onCreate(savedInstanceState)
            
            // Initialize binding safely
            binding = ActivityConsentReviewBinding.inflate(layoutInflater)
            setContentView(binding.root)
            
            // Initialize consent ledger safely
            consentLedger = ConsentLedger(this)
            
            // Load pending consent items
            val items = consentLedger.getPendingConsentItems()
            pendingItems.addAll(items)
            
            if (pendingItems.isEmpty()) {
                // No pending consents, continue to main activity
                proceedToMainActivity()
                return
            }
            
            // Setup UI safely
            try {
                binding.consentTitle.text = getString(R.string.consent_review_title)
                binding.consentSubtitle.text = getString(R.string.consent_review_subtitle)
            } catch (e: Exception) {
                // Fallback to simple titles if resources fail
                binding.consentTitle.text = "Review Permissions"
                binding.consentSubtitle.text = "The following features require your consent:"
            }
            
            // Setup RecyclerView safely
            try {
                binding.consentItemsList.layoutManager = LinearLayoutManager(this)
                binding.consentItemsList.adapter = ConsentItemAdapter(pendingItems)
            } catch (e: Exception) {
                // If RecyclerView setup fails, proceed to main activity
                proceedToMainActivity()
                return
            }
            
            // Setup buttons with null checks
            binding.btnConsentAccept.setOnClickListener {
                try {
                    if (binding.checkboxFinalConsent.isChecked) {
                        acceptAllConsents()
                    }
                } catch (e: Exception) {
                    // On error, just proceed
                    proceedToMainActivity()
                }
            }
            
            binding.btnConsentDeny.setOnClickListener {
                try {
                    denyAllConsents()
                } catch (e: Exception) {
                    // On error, just proceed
                    proceedToMainActivity()
                }
            }
            
            // Enable accept button only when checkbox is checked
            binding.checkboxFinalConsent.setOnCheckedChangeListener { _, isChecked ->
                try {
                    binding.btnConsentAccept.isEnabled = isChecked
                } catch (e: Exception) {
                    // Ignore checkbox errors
                }
            }
            
            binding.btnConsentAccept.isEnabled = false
            
        } catch (e: Exception) {
            // If consent review fails entirely, proceed to main activity
            proceedToMainActivity()
        }
    }
    
    private fun acceptAllConsents() {
        // Grant consent for all pending items
        pendingItems.forEach { item ->
            consentLedger.grantConsent(item.id)
        }
        
        consentLedger.markConsentReviewShown()
        proceedToMainActivity()
    }
    
    private fun denyAllConsents() {
        // Revoke all consents
        consentLedger.revokeAll()
        consentLedger.markConsentReviewShown()
        proceedToMainActivity()
    }
    
    private fun proceedToMainActivity() {
        val intent = Intent(this, MainActivity::class.java)
        startActivity(intent)
        finish()
    }
    
    /**
     * Adapter for consent items list.
     */
    private class ConsentItemAdapter(
        private val items: List<ConsentItem>
    ) : RecyclerView.Adapter<ConsentItemAdapter.ViewHolder>() {
        
        class ViewHolder(view: View) : RecyclerView.ViewHolder(view) {
            val title: TextView = view.findViewById(R.id.consent_item_title)
            val description: TextView = view.findViewById(R.id.consent_item_description)
            val benefit: TextView = view.findViewById(R.id.consent_item_benefit)
            val risk: TextView = view.findViewById(R.id.consent_item_risk)
            val howToDisable: TextView = view.findViewById(R.id.consent_item_disable)
        }
        
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
            val view = LayoutInflater.from(parent.context)
                .inflate(R.layout.item_consent, parent, false)
            return ViewHolder(view)
        }
        
        override fun onBindViewHolder(holder: ViewHolder, position: Int) {
            val item = items[position]
            holder.title.text = item.title
            holder.description.text = item.description
            holder.benefit.text = "✓ ${item.benefit}"
            holder.risk.text = "⚠ ${item.risk}"
            holder.howToDisable.text = "How to disable: ${item.howToDisable}"
        }
        
        override fun getItemCount() = items.size
    }
}
