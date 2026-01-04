package tech.ula.crash

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.FileProvider
import tech.ula.R
import tech.ula.databinding.ActivityCrashReportBinding
import java.io.File

/**
 * Activity displayed when the app crashes.
 * Shows crash details and allows user to copy error or export full report.
 */
class CrashReportActivity : AppCompatActivity() {
    
    private lateinit var binding: ActivityCrashReportBinding
    
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityCrashReportBinding.inflate(layoutInflater)
        setContentView(binding.root)
        
        // Get crash details from intent
        val exceptionType = intent.getStringExtra("exception_type") ?: "Unknown"
        val exceptionMessage = intent.getStringExtra("exception_message") ?: "Unknown error"
        
        // Load full crash report
        val crashReport = loadCrashReport()
        
        // Display crash info
        binding.crashTitle.text = getString(R.string.crash_title)
        binding.crashSubtitle.text = getString(R.string.crash_subtitle, exceptionType)
        binding.crashMessage.text = exceptionMessage
        binding.crashDetails.text = crashReport
        
        // Setup buttons
        binding.btnCopyError.setOnClickListener {
            copyToClipboard(crashReport)
        }
        
        binding.btnExportReport.setOnClickListener {
            exportReport()
        }
        
        binding.btnRestart.setOnClickListener {
            restartApp()
        }
        
        binding.btnClose.setOnClickListener {
            finishAffinity()
        }
    }
    
    private fun loadCrashReport(): String {
        return try {
            val crashFile = File(filesDir, "crash_last.txt")
            if (crashFile.exists()) {
                crashFile.readText()
            } else {
                "No crash report available"
            }
        } catch (e: Exception) {
            "Failed to load crash report: ${e.message}"
        }
    }
    
    private fun copyToClipboard(text: String) {
        try {
            val clipboard = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
            val clip = ClipData.newPlainText("UserLAnd Crash Report", text)
            clipboard.setPrimaryClip(clip)
            Toast.makeText(this, R.string.crash_copied, Toast.LENGTH_SHORT).show()
        } catch (e: Exception) {
            Toast.makeText(this, "Failed to copy: ${e.message}", Toast.LENGTH_SHORT).show()
        }
    }
    
    private fun exportReport() {
        try {
            val crashFile = File(filesDir, "crash_last.txt")
            val deviceFile = File(filesDir, "device_report.json")
            
            val filesToShare = mutableListOf<File>()
            if (crashFile.exists()) filesToShare.add(crashFile)
            if (deviceFile.exists()) filesToShare.add(deviceFile)
            
            if (filesToShare.isEmpty()) {
                Toast.makeText(this, "No reports to export", Toast.LENGTH_SHORT).show()
                return
            }
            
            val uris = filesToShare.map { file ->
                FileProvider.getUriForFile(
                    this,
                    "${applicationContext.packageName}.fileprovider",
                    file
                )
            }
            
            val intent = Intent(Intent.ACTION_SEND_MULTIPLE).apply {
                type = "text/*"
                putParcelableArrayListExtra(Intent.EXTRA_STREAM, ArrayList(uris))
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
            
            startActivity(Intent.createChooser(intent, getString(R.string.crash_export_title)))
            
        } catch (e: Exception) {
            Toast.makeText(this, "Failed to export: ${e.message}", Toast.LENGTH_LONG).show()
        }
    }
    
    private fun restartApp() {
        try {
            val intent = packageManager.getLaunchIntentForPackage(packageName)
            intent?.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_NEW_TASK)
            startActivity(intent)
            finishAffinity()
        } catch (e: Exception) {
            Toast.makeText(this, "Failed to restart: ${e.message}", Toast.LENGTH_SHORT).show()
            finishAffinity()
        }
    }
}
