package tech.ula.debug

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.Settings
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.FileProvider
import tech.ula.R
import tech.ula.consent.ConsentLedger
import tech.ula.databinding.ActivityDebugPanelBinding
import tech.ula.rafaelia.RafaeliaNative
import java.io.File
import java.text.SimpleDateFormat
import java.util.*

/**
 * Debug Panel Activity - provides comprehensive hardware information
 * and access to advanced debugging features.
 */
class DebugPanelActivity : AppCompatActivity() {
    
    private lateinit var binding: ActivityDebugPanelBinding
    private lateinit var consentLedger: ConsentLedger
    private lateinit var deviceInfoCollector: DeviceInfoCollector
    
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityDebugPanelBinding.inflate(layoutInflater)
        setContentView(binding.root)
        
        consentLedger = ConsentLedger(this)
        deviceInfoCollector = DeviceInfoCollector(this)
        
        setSupportActionBar(binding.toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)
        supportActionBar?.title = getString(R.string.debug_panel_title)
        
        // Display device info
        displayDeviceInfo()
        
        // Setup buttons
        binding.btnGenerateReport.setOnClickListener {
            if (consentLedger.isConsented(ConsentLedger.CONSENT_HARDWARE_REPORT)) {
                generateReport()
            } else {
                armConsentAndPrompt(ConsentLedger.CONSENT_HARDWARE_REPORT)
            }
        }
        
        binding.btnCopySummary.setOnClickListener {
            copySummary()
        }
        
        binding.btnExportFile.setOnClickListener {
            if (consentLedger.isConsented(ConsentLedger.CONSENT_EXPORT_LOGS)) {
                exportFile()
            } else {
                armConsentAndPrompt(ConsentLedger.CONSENT_EXPORT_LOGS)
            }
        }
        
        binding.btnOpenDevOptions.setOnClickListener {
            if (consentLedger.isConsented(ConsentLedger.CONSENT_DEV_OPTIONS_GUIDE)) {
                showDevOptionsGuide()
            } else {
                armConsentAndPrompt(ConsentLedger.CONSENT_DEV_OPTIONS_GUIDE)
            }
        }
        
        binding.btnOpenAppDetails.setOnClickListener {
            openAppDetails()
        }
        
        binding.btnAllFilesAccess.setOnClickListener {
            if (consentLedger.isConsented(ConsentLedger.CONSENT_ALL_FILES)) {
                requestAllFilesAccess()
            } else {
                armConsentAndPrompt(ConsentLedger.CONSENT_ALL_FILES)
            }
        }
    }
    
    override fun onSupportNavigateUp(): Boolean {
        finish()
        return true
    }
    
    private fun displayDeviceInfo() {
        val info = buildString {
            appendLine("DEVICE: ${Build.MANUFACTURER} ${Build.MODEL}")
            appendLine("ANDROID: ${Build.VERSION.RELEASE} (SDK ${Build.VERSION.SDK_INT})")
            appendLine("ABIs: ${Build.SUPPORTED_ABIS.joinToString(", ")}")
            appendLine("CORES: ${Runtime.getRuntime().availableProcessors()}")
            
            val runtime = Runtime.getRuntime()
            val maxMemMB = runtime.maxMemory() / (1024 * 1024)
            val usedMemMB = (runtime.totalMemory() - runtime.freeMemory()) / (1024 * 1024)
            appendLine("MEMORY: $usedMemMB / $maxMemMB MB")
            
            // Native library status
            if (RafaeliaNative.isNativeLibraryAvailable()) {
                appendLine("NATIVE: ✓ Loaded")
            } else {
                appendLine("NATIVE: ✗ Not available")
                val error = RafaeliaNative.getLoadError()
                if (error != null) {
                    appendLine("  Error: ${error.message}")
                }
            }
        }
        
        binding.deviceInfoText.text = info
    }
    
    private fun generateReport() {
        try {
            val report = deviceInfoCollector.collectFullReportText()
            
            // Save to file
            val timestamp = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(Date())
            val filename = "device_report_$timestamp.txt"
            val file = File(filesDir, filename)
            file.writeText(report)
            
            // Show in text view
            binding.deviceInfoText.text = report
            
            Toast.makeText(this, "Report generated: $filename", Toast.LENGTH_SHORT).show()
        } catch (e: Exception) {
            Toast.makeText(this, "Failed to generate report: ${e.message}", Toast.LENGTH_LONG).show()
        }
    }
    
    private fun copySummary() {
        try {
            val summary = binding.deviceInfoText.text.toString()
            val clipboard = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
            val clip = ClipData.newPlainText("Device Summary", summary)
            clipboard.setPrimaryClip(clip)
            Toast.makeText(this, "Summary copied to clipboard", Toast.LENGTH_SHORT).show()
        } catch (e: Exception) {
            Toast.makeText(this, "Failed to copy: ${e.message}", Toast.LENGTH_SHORT).show()
        }
    }
    
    private fun exportFile() {
        try {
            // Generate full report
            val jsonReport = deviceInfoCollector.collectFullReport()
            val textReport = deviceInfoCollector.collectFullReportText()
            
            // Save both JSON and text versions
            val timestamp = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(Date())
            val jsonFile = File(filesDir, "device_report_$timestamp.json")
            val textFile = File(filesDir, "device_report_$timestamp.txt")
            
            jsonFile.writeText(jsonReport.toString(2))
            textFile.writeText(textReport)
            
            // Create share intent
            val jsonUri = FileProvider.getUriForFile(
                this,
                "${applicationContext.packageName}.fileprovider",
                jsonFile
            )
            val textUri = FileProvider.getUriForFile(
                this,
                "${applicationContext.packageName}.fileprovider",
                textFile
            )
            
            val intent = Intent(Intent.ACTION_SEND_MULTIPLE).apply {
                type = "text/*"
                putParcelableArrayListExtra(Intent.EXTRA_STREAM, arrayListOf(jsonUri, textUri))
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
            
            startActivity(Intent.createChooser(intent, getString(R.string.debug_panel_export_title)))
            
        } catch (e: Exception) {
            Toast.makeText(this, "Failed to export: ${e.message}", Toast.LENGTH_LONG).show()
        }
    }
    
    private fun showDevOptionsGuide() {
        AlertDialog.Builder(this)
            .setTitle(getString(R.string.debug_panel_dev_options))
            .setMessage(getString(R.string.debug_dev_options_guide_message))
            .setPositiveButton(R.string.button_continue) { _, _ ->
                try {
                    startActivity(Intent(Settings.ACTION_SETTINGS))
                } catch (e: Exception) {
                    Toast.makeText(this, "Failed to open Settings", Toast.LENGTH_SHORT).show()
                }
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }
    
    private fun openAppDetails() {
        try {
            val intent = Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS).apply {
                data = Uri.fromParts("package", packageName, null)
            }
            startActivity(intent)
        } catch (e: Exception) {
            Toast.makeText(this, "Failed to open app details: ${e.message}", Toast.LENGTH_SHORT).show()
        }
    }
    
    private fun requestAllFilesAccess() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                AlertDialog.Builder(this)
                    .setTitle("All Files Access")
                    .setMessage("""
                        This will open Android Settings where you can grant "All files access" permission.
                        
                        ⚠️ This grants broad storage access. Only grant if you need it.
                        
                        You can revoke this permission at any time in Settings → Apps → UserLAnd → Permissions.
                    """.trimIndent())
                    .setPositiveButton("Open Settings") { _, _ ->
                        try {
                            val intent = Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION).apply {
                                data = Uri.fromParts("package", packageName, null)
                            }
                            startActivity(intent)
                        } catch (e: Exception) {
                            Toast.makeText(this, "Failed to open settings: ${e.message}", Toast.LENGTH_LONG).show()
                        }
                    }
                    .setNegativeButton("Cancel", null)
                    .show()
            } else {
                Toast.makeText(this, "All files access already granted", Toast.LENGTH_SHORT).show()
            }
        } else {
            Toast.makeText(this, "All files access not required on Android < 11", Toast.LENGTH_SHORT).show()
        }
    }
    
    private fun armConsentAndPrompt(consentId: String) {
        consentLedger.armFeature(consentId)
        
        AlertDialog.Builder(this)
            .setTitle(getString(R.string.debug_panel_consent_required))
            .setMessage("This feature requires your consent. Please restart the app to review and grant consent.")
            .setPositiveButton("Restart Now") { _, _ ->
                // Restart app
                val intent = packageManager.getLaunchIntentForPackage(packageName)
                intent?.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_NEW_TASK)
                startActivity(intent)
                finishAffinity()
            }
            .setNegativeButton("Later", null)
            .show()
    }
}
