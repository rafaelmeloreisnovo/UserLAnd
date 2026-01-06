package tech.ula

import android.app.ProgressDialog
import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import tech.ula.consent.ConsentLedger
import tech.ula.consent.ConsentReviewActivity
import tech.ula.databinding.ActivityMainBinding
import tech.ula.debug.DebugPanelActivity
import tech.ula.model.entities.App
import tech.ula.model.entities.Session
import tech.ula.ui.AppsListFragment
import tech.ula.ui.SessionListFragment
import tech.ula.utils.Android15CompatibilityHelper
import tech.ula.utils.MemoryMonitor
import tech.ula.utils.SafetyGuard
import tech.ula.utils.defaultSharedPreferences
import java.util.Locale

/**
 * Minimal stub MainActivity for CI builds.
 * TODO: Full ViewBinding migration required - the original code references
 * kotlin-android-extensions synthetics which are no longer supported in Kotlin 1.8+.
 */
class MainActivity :
    AppCompatActivity(),
    SessionListFragment.SessionSelection,
    AppsListFragment.AppSelection {
    private lateinit var binding: ActivityMainBinding
    private var progressDialog: ProgressDialog? = null
    private val uiHandler = Handler(Looper.getMainLooper())

    companion object {
        private const val TAG = "MainActivity"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        try {
            super.onCreate(savedInstanceState)
            Log.i(TAG, "MainActivity.onCreate() starting")
            
            // Validate environment before proceeding
            val environmentIssues = Android15CompatibilityHelper.validateEnvironment(this)
            if (environmentIssues.isNotEmpty()) {
                Log.w(TAG, "Environment validation found ${environmentIssues.size} issue(s):")
                environmentIssues.forEach { issue -> Log.w(TAG, "  - $issue") }
                
                // Show warning to user if architecture is not supported (critical)
                if (environmentIssues.any { it.contains("Unsupported architecture", ignoreCase = true) }) {
                    Toast.makeText(
                        this,
                        "Warning: Device architecture may not be fully supported. Some features may not work correctly.",
                        Toast.LENGTH_LONG
                    ).show()
                }
            }
            
            // Check for pending consents first
            val consentLedger = ConsentLedger(this)
            if (consentLedger.hasPendingConsents() && !consentLedger.wasConsentReviewShown()) {
                Log.i(TAG, "Redirecting to consent review")
                // Redirect to consent review with safe intent flags
                val intent = Intent(this, ConsentReviewActivity::class.java).apply {
                    addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP)
                    addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP)
                }
                startActivity(intent)
                finish()
                return
            }
            
            // Initialize binding safely
            binding = ActivityMainBinding.inflate(layoutInflater)
            setContentView(binding.root)
            
            // Set up toolbar safely
            try {
                setSupportActionBar(binding.toolbar)
                supportActionBar?.setDisplayShowTitleEnabled(true)
            } catch (e: Exception) {
                Log.e(TAG, "Error setting up toolbar", e)
            }

            setupSafetyIndicator()
            
            Log.i(TAG, "MainActivity initialized successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Error during MainActivity initialization", e)
            // Re-throw to let global exception handler catch it
            throw e
        }
    }
    
    override fun onResume() {
        super.onResume()
        Log.i(TAG, "MainActivity.onResume()")
        
        // Log memory status to help diagnose issues
        try {
            MemoryMonitor.logMemoryStatus(this, TAG)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to log memory status", e)
        }

        try {
            updateSafetyIndicator()
        } catch (e: Exception) {
            Log.e(TAG, "Failed to update safety indicator", e)
        }
    }
    
    override fun onPause() {
        super.onPause()
        Log.i(TAG, "MainActivity.onPause()")
    }
    
    override fun onDestroy() {
        super.onDestroy()
        Log.i(TAG, "MainActivity.onDestroy()")
    }

    private fun setupSafetyIndicator() {
        binding.safetyIndicator.setOnClickListener {
            val prefs = defaultSharedPreferences
            val lastEvent = prefs.getString(SafetyGuard.KEY_LAST_EVENT, null)
            val lastEventAt = prefs.getString(SafetyGuard.KEY_LAST_EVENT_AT, null)
            val message =
                if (!lastEvent.isNullOrBlank() && !lastEventAt.isNullOrBlank()) {
                    "SafetyGuard: $lastEventAt\n$lastEvent"
                } else {
                    "SafetyGuard: no recent events"
                }
            Toast.makeText(this, message, Toast.LENGTH_LONG).show()
        }
    }

    private fun updateSafetyIndicator() {
        val prefs = defaultSharedPreferences
        val indicatorEnabled = prefs.getBoolean(SafetyGuard.KEY_INDICATOR_ENABLED, true)
        val guardEnabled = prefs.getBoolean(SafetyGuard.KEY_ENABLED, true)
        if (!indicatorEnabled || !guardEnabled) {
            binding.safetyIndicator.visibility = View.GONE
            return
        }

        val state = SafetyGuard.evaluate(this)
        val colorRes =
            when (state.severity) {
                SafetyGuard.Severity.Critical -> R.color.holo_red_light
                SafetyGuard.Severity.Warning -> R.color.holo_orange_light
                SafetyGuard.Severity.Normal -> R.color.holo_green_light
            }

        binding.safetyIndicator.apply {
            visibility = View.VISIBLE
            setTextColor(getColor(colorRes))
            contentDescription = "Safety guard ${state.severity.name.lowercase(Locale.getDefault())}"
        }

        if (state.isTriggered) {
            uiHandler.postDelayed({ updateSafetyIndicator() }, 1500L)
        }
    }

    override fun onCreateOptionsMenu(menu: Menu?): Boolean {
        menuInflater.inflate(R.menu.menu_options, menu)
        return super.onCreateOptionsMenu(menu)
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.debug_panel -> {
                startActivity(Intent(this, DebugPanelActivity::class.java))
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    @Suppress("DEPRECATION")
    fun updateFilesystemExportProgress(details: String) {
        if (progressDialog == null) {
            progressDialog =
                ProgressDialog(this).apply {
                    setMessage(getString(R.string.progress_exporting_filesystem))
                    setCancelable(false)
                    setProgressStyle(ProgressDialog.STYLE_HORIZONTAL)
                    setProgress(0)
                    show()
                }
        }
        progressDialog?.setMessage(details)
    }

    fun stopProgressFromFilesystemList() {
        progressDialog?.dismiss()
        progressDialog = null
    }

    @Suppress("DEPRECATION")
    fun updateFilesystemDeleteProgress() {
        if (progressDialog == null) {
            progressDialog =
                ProgressDialog(this).apply {
                    setMessage(getString(R.string.progress_deleting_filesystem))
                    setCancelable(false)
                    setProgressStyle(ProgressDialog.STYLE_SPINNER)
                    show()
                }
        }
    }

    fun displayGenericErrorDialog(
        titleResId: Int,
        messageResId: Int,
    ) {
        try {
            // Check if activity is still valid
            if (isFinishing || isDestroyed) {
                Log.w(TAG, "Cannot show dialog - activity is finishing or destroyed")
                return
            }
            
            AlertDialog
                .Builder(this)
                .setTitle(titleResId)
                .setMessage(messageResId)
                .setPositiveButton(R.string.button_ok) { dialog, _ -> dialog.dismiss() }
                .create()
                .show()
        } catch (e: Exception) {
            Log.e(TAG, "Failed to display error dialog", e)
        }
    }
    
    /**
     * Display error with fallback text if resources fail.
     */
    fun displayGenericErrorDialogSafe(
        title: String,
        message: String,
    ) {
        try {
            // Check if activity is still valid
            if (isFinishing || isDestroyed) {
                Log.w(TAG, "Cannot show dialog - activity is finishing or destroyed")
                return
            }
            
            AlertDialog
                .Builder(this)
                .setTitle(title)
                .setMessage(message)
                .setPositiveButton("OK") { dialog, _ -> dialog.dismiss() }
                .create()
                .show()
        } catch (e: Exception) {
            Log.e(TAG, "Failed to display error dialog", e)
        }
    }

    override fun sessionHasBeenSelected(session: Session) {
        try {
            val serviceIntent = Intent(this, ServerService::class.java)
            if (session.active) {
                // Session is already active, restart the client
                serviceIntent.putExtra("type", "restartRunningSession")
                serviceIntent.putExtra("session", session)
            } else {
                // Start a new session
                serviceIntent.putExtra("type", "start")
                serviceIntent.putExtra("session", session)
            }
            startService(serviceIntent)
        } catch (e: Exception) {
            Log.e(TAG, "Error starting session: ${session.name}", e)
            displayGenericErrorDialogSafe(
                getString(R.string.error_session_start_title),
                getString(R.string.error_session_start, e.message ?: "Unknown error")
            )
        }
    }

    override fun appHasBeenSelected(
        app: App,
        autoStart: Boolean,
    ) {
        try {
            // Apps are launched via their associated session
            // For the minimal stub implementation, we show a simple message
            // Full implementation would use MainActivityViewModel flow
            Toast.makeText(
                this,
                getString(R.string.info_app_starting, app.name),
                Toast.LENGTH_SHORT
            ).show()
            
            Log.i(TAG, "App selected: ${app.name}, autoStart: $autoStart")
            // Note: A complete implementation would require:
            // 1. Initialize MainActivityViewModel
            // 2. Call viewModel.submitAppSelection(app, autoStart)
            // 3. Observe state changes and handle the startup flow
            // For now, this minimal stub just logs the selection
        } catch (e: Exception) {
            Log.e(TAG, "Error selecting app: ${app.name}", e)
            displayGenericErrorDialogSafe(
                getString(R.string.error_app_selection_title),
                getString(R.string.error_app_selection, e.message ?: "Unknown error")
            )
        }
    }
}
