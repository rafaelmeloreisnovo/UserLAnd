package tech.ula

import android.app.ProgressDialog
import android.os.Bundle
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import tech.ula.databinding.ActivityMainBinding
import tech.ula.model.entities.Session
import tech.ula.ui.SessionListFragment

/**
 * Minimal stub MainActivity for CI builds.
 * TODO: Full ViewBinding migration required - the original code references
 * kotlin-android-extensions synthetics which are no longer supported in Kotlin 1.8+.
 */
class MainActivity : AppCompatActivity(), SessionListFragment.SessionSelection {

    private lateinit var binding: ActivityMainBinding
    private var progressDialog: ProgressDialog? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        setSupportActionBar(binding.toolbar)
    }

    @Suppress("DEPRECATION")
    fun updateFilesystemExportProgress(details: String) {
        if (progressDialog == null) {
            progressDialog = ProgressDialog(this).apply {
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
            progressDialog = ProgressDialog(this).apply {
                setMessage(getString(R.string.progress_deleting_filesystem))
                setCancelable(false)
                setProgressStyle(ProgressDialog.STYLE_SPINNER)
                show()
            }
        }
    }

    fun displayGenericErrorDialog(titleResId: Int, messageResId: Int) {
        AlertDialog.Builder(this)
            .setTitle(titleResId)
            .setMessage(messageResId)
            .setPositiveButton(R.string.button_ok) { dialog, _ -> dialog.dismiss() }
            .create()
            .show()
    }

    override fun sessionHasBeenSelected(session: Session) {
        // TODO: Implement session selection handling
    }
}
