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
        super.onCreate(savedInstanceState)
        binding = ActivityConsentReviewBinding.inflate(layoutInflater)
        setContentView(binding.root)
        
        consentLedger = ConsentLedger(this)
        
        // Load pending consent items
        pendingItems.addAll(consentLedger.getPendingConsentItems())
        
        if (pendingItems.isEmpty()) {
            // No pending consents, continue to main activity
            proceedToMainActivity()
            return
        }
        
        // Setup UI
        binding.consentTitle.text = getString(R.string.consent_review_title)
        binding.consentSubtitle.text = getString(R.string.consent_review_subtitle)
        
        // Setup RecyclerView
        binding.consentItemsList.layoutManager = LinearLayoutManager(this)
        binding.consentItemsList.adapter = ConsentItemAdapter(pendingItems)
        
        // Setup buttons
        binding.btnConsentAccept.setOnClickListener {
            if (binding.checkboxFinalConsent.isChecked) {
                acceptAllConsents()
            }
        }
        
        binding.btnConsentDeny.setOnClickListener {
            denyAllConsents()
        }
        
        // Enable accept button only when checkbox is checked
        binding.checkboxFinalConsent.setOnCheckedChangeListener { _, isChecked ->
            binding.btnConsentAccept.isEnabled = isChecked
        }
        
        binding.btnConsentAccept.isEnabled = false
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
