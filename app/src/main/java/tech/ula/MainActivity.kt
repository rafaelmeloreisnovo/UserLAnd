package tech.ula

import android.app.ProgressDialog
import android.content.Intent
import android.os.Bundle
import android.util.Log
import android.view.Menu
import android.view.MenuItem
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

    companion object {
        private const val TAG = "MainActivity"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        try {
            super.onCreate(savedInstanceState)
            
            // Check for pending consents first
            val consentLedger = ConsentLedger(this)
            if (consentLedger.hasPendingConsents() && !consentLedger.wasConsentReviewShown()) {
                // Redirect to consent review
                val intent = Intent(this, ConsentReviewActivity::class.java)
                startActivity(intent)
                finish()
                return
            }
            
            binding = ActivityMainBinding.inflate(layoutInflater)
            setContentView(binding.root)
            setSupportActionBar(binding.toolbar)
            Log.i(TAG, "MainActivity initialized successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Error during MainActivity initialization", e)
            // Re-throw to let global exception handler catch it
            throw e
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
        AlertDialog
            .Builder(this)
            .setTitle(titleResId)
            .setMessage(messageResId)
            .setPositiveButton(R.string.button_ok) { dialog, _ -> dialog.dismiss() }
            .create()
            .show()
    }

    override fun sessionHasBeenSelected(session: Session) {
        // TODO: Implement session selection handling
    }

    override fun appHasBeenSelected(
        app: App,
        autoStart: Boolean,
    ) {
        // TODO: Implement app selection handling
    }
}
