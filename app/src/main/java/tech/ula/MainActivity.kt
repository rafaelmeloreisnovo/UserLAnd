package tech.ula

import android.app.DownloadManager
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.StatFs
import android.util.Log
import android.view.Gravity
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.RadioGroup
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProvider
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.setupWithNavController
import tech.ula.consent.ConsentLedger
import tech.ula.consent.ConsentReviewActivity
import tech.ula.databinding.ActivityMainBinding
import tech.ula.debug.DebugPanelActivity
import tech.ula.model.entities.App
import tech.ula.model.entities.ServiceType
import tech.ula.model.entities.Session
import tech.ula.model.remote.GithubApiClient
import tech.ula.model.repositories.AssetRepository
import tech.ula.model.repositories.DownloadMetadata
import tech.ula.model.repositories.UlaDatabase
import tech.ula.model.state.AppsStartupFsm
import tech.ula.model.state.SessionStartupFsm
import tech.ula.ui.AppsListFragment
import tech.ula.ui.SessionListFragment
import tech.ula.viewmodel.MainActivityViewModel
import tech.ula.viewmodel.MainActivityViewModelFactory
import tech.ula.viewmodel.State
import tech.ula.utils.Android15CompatibilityHelper
import tech.ula.utils.AssetDownloader
import tech.ula.utils.BusyboxExecutor
import tech.ula.utils.CredentialValidator
import tech.ula.utils.DownloadManagerWrapper
import tech.ula.utils.FilesystemManager
import tech.ula.utils.IllegalStateHandler
import tech.ula.utils.MemoryMonitor
import tech.ula.utils.ProotDebugLogger
import tech.ula.utils.SafetyGuard
import tech.ula.utils.StorageCalculator
import tech.ula.utils.UlaFiles
import tech.ula.utils.defaultSharedPreferences
import tech.ula.utils.preferences.AssetPreferences
import java.util.Locale

/**
 * Main entry point for app/session startup, navigation, safety status, and CI-verifiable APK builds.
 */
class MainActivity :
    AppCompatActivity(),
    SessionListFragment.SessionSelection,
    AppsListFragment.AppSelection {
    private lateinit var binding: ActivityMainBinding
    private var progressDialog: AlertDialog? = null
    private var progressMsgView: TextView? = null
    private var progressBarHorizontal: ProgressBar? = null
    private val uiHandler = Handler(Looper.getMainLooper())
    private val credentialValidator = CredentialValidator()

    private val ulaFiles: UlaFiles by lazy { UlaFiles(this, applicationInfo.nativeLibraryDir) }
    private val prootDebugLogger: ProotDebugLogger by lazy { ProotDebugLogger(defaultSharedPreferences, ulaFiles) }
    private val busyboxExecutor: BusyboxExecutor by lazy { BusyboxExecutor(ulaFiles, prootDebugLogger) }
    private val filesystemManager: FilesystemManager by lazy { FilesystemManager(ulaFiles, busyboxExecutor) }
    private val assetPreferences: AssetPreferences by lazy { AssetPreferences(this) }
    private val assetRepository: AssetRepository by lazy {
        AssetRepository(filesDir.path, assetPreferences, GithubApiClient(ulaFiles))
    }
    private val assetDownloader: AssetDownloader by lazy {
        val downloadManager = getSystemService(Context.DOWNLOAD_SERVICE) as DownloadManager
        AssetDownloader(assetPreferences, DownloadManagerWrapper(downloadManager), ulaFiles)
    }
    private val storageCalculator: StorageCalculator by lazy { StorageCalculator(StatFs(filesDir.path)) }
    private val viewModel: MainActivityViewModel by lazy {
        val database = UlaDatabase.getInstance(this)
        val appsStartupFsm = AppsStartupFsm(database, filesystemManager, ulaFiles)
        val sessionStartupFsm = SessionStartupFsm(database, assetRepository, filesystemManager, assetDownloader, storageCalculator)
        ViewModelProvider(this, MainActivityViewModelFactory(appsStartupFsm, sessionStartupFsm))
            .get(MainActivityViewModel::class.java)
    }

    private val downloadCompleteReceiver =
        object : BroadcastReceiver() {
            override fun onReceive(context: Context?, intent: Intent?) {
                if (intent?.action != DownloadManager.ACTION_DOWNLOAD_COMPLETE) return
                val downloadId = intent.getLongExtra(DownloadManager.EXTRA_DOWNLOAD_ID, -1L)
                if (downloadId >= 0L) viewModel.submitCompletedDownloadId(downloadId)
            }
        }
    private var downloadReceiverRegistered = false

    companion object {
        private const val TAG = "MainActivity"
        private const val PREF_DEFAULT_NAV_LOCATION = "pref_default_nav_location"
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

            // Set up navigation
            setupNavigation()
            observeStartupState()

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
        viewModel.handleOnResume()
        
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

    override fun onStart() {
        super.onStart()
        if (!downloadReceiverRegistered) {
            val filter = IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                registerReceiver(downloadCompleteReceiver, filter, Context.RECEIVER_NOT_EXPORTED)
            } else {
                @Suppress("DEPRECATION")
                registerReceiver(downloadCompleteReceiver, filter)
            }
            downloadReceiverRegistered = true
        }
    }

    override fun onStop() {
        if (downloadReceiverRegistered) {
            unregisterReceiver(downloadCompleteReceiver)
            downloadReceiverRegistered = false
        }
        super.onStop()
    }
    
    override fun onDestroy() {
        super.onDestroy()
        Log.i(TAG, "MainActivity.onDestroy()")
    }

    private fun setupNavigation() {
        try {
            // Get the NavHostFragment and NavController
            val navHostFragment = supportFragmentManager
                .findFragmentById(R.id.nav_host_fragment) as? NavHostFragment
            
            if (navHostFragment == null) {
                Log.e(TAG, "NavHostFragment not found")
                return
            }
            
            val navController = navHostFragment.navController
            
            // Get the navigation graph
            val navGraph = navController.navInflater.inflate(R.navigation.nav_graph)
            
            // Determine the start destination based on user preference
            val prefs = defaultSharedPreferences
            val defaultLandingPage = prefs.getString(PREF_DEFAULT_NAV_LOCATION, getString(R.string.apps))
            
            // Set the start destination based on preference
            val startDestination = when (defaultLandingPage) {
                getString(R.string.sessions) -> R.id.session_list_fragment
                else -> R.id.app_list_fragment // Default to Apps
            }
            
            navGraph.setStartDestination(startDestination)
            navController.graph = navGraph
            
            // Set up bottom navigation with NavController
            binding.bottomNavView.setupWithNavController(navController)
            
            Log.i(TAG, "Navigation setup completed with start destination: $defaultLandingPage")
        } catch (e: Exception) {
            Log.e(TAG, "Error setting up navigation", e)
        }
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

    fun updateFilesystemExportProgress(details: String) {
        if (progressDialog == null) {
            progressDialog = buildHorizontalProgressDialog(getString(R.string.progress_exporting_filesystem))
            progressDialog?.show()
        }
        progressMsgView?.text = details
    }

    fun stopProgressFromFilesystemList() {
        progressDialog?.dismiss()
        progressDialog = null
        progressMsgView = null
        progressBarHorizontal = null
    }

    fun updateFilesystemDeleteProgress() {
        if (progressDialog == null) {
            progressDialog = buildSpinnerProgressDialog(getString(R.string.progress_deleting_filesystem))
            progressDialog?.show()
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

    private fun observeStartupState() {
        viewModel.getState().observe(
            this,
            Observer { state ->
                if (state != null) handleStartupState(state)
            }
        )
    }

    private fun handleStartupState(state: State) {
        when (val action = MainActivityStateActionMapper.from(state)) {
            MainActivityStateAction.None -> Unit
            is MainActivityStateAction.ShowToast -> Toast.makeText(this, action.messageResId, Toast.LENGTH_LONG).show()
            is MainActivityStateAction.StartSession -> startPreparedSession(action.session, action.type)
            MainActivityStateAction.RequestFilesystemCredentials -> showFilesystemCredentialsDialog()
            MainActivityStateAction.RequestAppServiceType -> showAppServiceTypeDialog()
            is MainActivityStateAction.ConfirmLargeDownload -> showLargeDownloadDialog(action.downloadRequirements)
            MainActivityStateAction.ConfirmLowStorage -> showLowStorageDialog()
            MainActivityStateAction.RequireInactiveSessions -> displayGenericErrorDialogSafe(
                getString(R.string.general_error_title),
                getString(R.string.alert_storage_low_message)
            )
            is MainActivityStateAction.ShowProgress -> showProgress(action.messageResId)
            is MainActivityStateAction.ShowExtractionProgress -> {
                showProgress(getString(R.string.progress_extraction_details, action.extractionTarget))
            }
            is MainActivityStateAction.ShowDownloadProgress -> showDownloadProgress(action.numComplete, action.numTotal)
            MainActivityStateAction.CompleteProgress -> stopProgressFromFilesystemList()
            is MainActivityStateAction.ShowIllegalState -> {
                showIllegalState(IllegalStateHandler.getLocalizationData(action.state).getString(this))
            }
        }
    }

    private fun showProgress(messageResId: Int) = showProgress(getString(messageResId))

    private fun showProgress(message: String) {
        if (progressDialog == null) {
            progressDialog = buildSpinnerProgressDialog(message)
            progressDialog?.show()
        } else {
            progressMsgView?.text = message
        }
    }

    private fun showDownloadProgress(
        complete: Int,
        total: Int,
    ) {
        val message = getString(R.string.progress_downloading_out_of, complete, total)
        val safeTotal = total.coerceAtLeast(1)
        if (progressDialog == null || progressBarHorizontal == null) {
            progressDialog?.dismiss()
            progressDialog = buildHorizontalProgressDialog(message, safeTotal)
            progressDialog?.show()
        }
        progressMsgView?.text = message
        progressBarHorizontal?.apply {
            max = safeTotal
            progress = complete.coerceAtLeast(0)
        }
    }

    private fun buildSpinnerProgressDialog(message: String): AlertDialog {
        val dp = resources.displayMetrics.density
        val pad = (20 * dp).toInt()
        val gap = (12 * dp).toInt()
        progressMsgView = TextView(this).apply { text = message }
        progressBarHorizontal = null
        val spinner = ProgressBar(this)
        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(pad, pad, pad, pad)
            gravity = Gravity.CENTER_VERTICAL
            addView(spinner, LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply { marginEnd = gap })
            addView(progressMsgView, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
        }
        return AlertDialog.Builder(this).setView(layout).setCancelable(false).create()
    }

    private fun buildHorizontalProgressDialog(message: String, maxValue: Int = 100): AlertDialog {
        val dp = resources.displayMetrics.density
        val pad = (20 * dp).toInt()
        val gap = (8 * dp).toInt()
        progressMsgView = TextView(this).apply { text = message }
        progressBarHorizontal = ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal).apply {
            isIndeterminate = false
            max = maxValue
        }
        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(pad, pad, pad, pad)
            addView(progressMsgView, LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply { bottomMargin = gap })
            addView(progressBarHorizontal, LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ))
        }
        return AlertDialog.Builder(this).setView(layout).setCancelable(false).create()
    }

    private fun showIllegalState(messageResId: Int) = showIllegalState(getString(messageResId))

    private fun showIllegalState(message: String) {
        stopProgressFromFilesystemList()
        displayGenericErrorDialogSafe(
            getString(R.string.illegal_state_title),
            getString(R.string.illegal_state_github_message, message)
        )
    }

    private fun showFilesystemCredentialsDialog() {
        val view = layoutInflater.inflate(R.layout.dia_app_credentials, null)
        val usernameInput = view.findViewById<EditText>(R.id.text_input_username)
        val passwordInput = view.findViewById<EditText>(R.id.text_input_password)
        val vncPasswordInput = view.findViewById<EditText>(R.id.text_input_vnc_password)

        AlertDialog.Builder(this)
            .setTitle(R.string.prompt_filesystem)
            .setMessage(R.string.filesystem_credentials_reasoning)
            .setView(view)
            .setPositiveButton(R.string.button_ok, null)
            .setNegativeButton(R.string.button_cancel) { dialog, _ ->
                viewModel.handleUserInputCancelled()
                dialog.dismiss()
            }
            .create()
            .apply {
                setOnShowListener {
                    getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener {
                        val username = usernameInput.text?.toString().orEmpty()
                        val password = passwordInput.text?.toString().orEmpty()
                        val vncPassword = vncPasswordInput.text?.toString().orEmpty()
                        if (credentialsAreValid(username, password, vncPassword)) {
                            viewModel.submitFilesystemCredentials(username, password, vncPassword)
                            dismiss()
                        }
                    }
                }
            }
            .show()
    }

    private fun credentialsAreValid(
        username: String,
        password: String,
        vncPassword: String,
    ): Boolean {
        val blacklist = resources.getStringArray(R.array.blacklisted_usernames)
        val validations = listOf(
            credentialValidator.validateUsername(username, blacklist),
            credentialValidator.validatePassword(password),
            credentialValidator.validateVncPassword(vncPassword)
        )
        val firstError = validations.firstOrNull { !it.credentialIsValid } ?: return true
        Toast.makeText(this, firstError.errorMessageId, Toast.LENGTH_LONG).show()
        return false
    }

    private fun showAppServiceTypeDialog() {
        val view = layoutInflater.inflate(R.layout.dia_app_select_client, null)
        val radioGroup = view.findViewById<RadioGroup>(R.id.radio_apps_service_type_preference)
        AlertDialog.Builder(this)
            .setView(view)
            .setPositiveButton(R.string.button_ok) { dialog, _ ->
                val serviceType =
                    when (radioGroup.checkedRadioButtonId) {
                        R.id.vnc_radio_button -> ServiceType.Vnc
                        R.id.xsdl_radio_button -> ServiceType.Xsdl
                        else -> ServiceType.Ssh
                    }
                viewModel.submitAppServiceType(serviceType)
                dialog.dismiss()
            }
            .setNegativeButton(R.string.button_cancel) { dialog, _ ->
                viewModel.handleUserInputCancelled()
                dialog.dismiss()
            }
            .show()
    }

    private fun showLargeDownloadDialog(downloadRequirements: List<DownloadMetadata>) {
        AlertDialog.Builder(this)
            .setTitle(R.string.alert_wifi_disabled_title)
            .setMessage(R.string.alert_wifi_disabled_message)
            .setPositiveButton(R.string.alert_wifi_disabled_continue_button) { dialog, _ ->
                viewModel.startAssetDownloads(downloadRequirements)
                dialog.dismiss()
            }
            .setNegativeButton(R.string.alert_wifi_disabled_cancel_button) { dialog, _ ->
                viewModel.handleUserInputCancelled()
                dialog.dismiss()
            }
            .show()
    }

    private fun showLowStorageDialog() {
        AlertDialog.Builder(this)
            .setTitle(R.string.alert_storage_low_title)
            .setMessage(R.string.alert_storage_low_message)
            .setPositiveButton(R.string.button_ok) { dialog, _ ->
                viewModel.lowAvailableStorageAcknowledged()
                dialog.dismiss()
            }
            .setNegativeButton(R.string.button_cancel) { dialog, _ ->
                viewModel.handleUserInputCancelled()
                dialog.dismiss()
            }
            .show()
    }

    private fun startPreparedSession(
        session: Session,
        type: String,
    ) {
        showProgress(R.string.progress_starting)
        val serviceIntent = Intent(this, ServerService::class.java).apply {
            putExtra("type", type)
            putExtra("session", session)
        }
        startService(serviceIntent)
        viewModel.handleSessionHasBeenActivated()
        stopProgressFromFilesystemList()
    }

    override fun sessionHasBeenSelected(session: Session) {
        try {
            Log.i(TAG, "Session selected: ${session.name}, active: ${session.active}")
            viewModel.submitSessionSelection(session)
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
            Log.i(TAG, "App selected: ${app.name}, autoStart: $autoStart")
            viewModel.submitAppSelection(app, autoStart)
        } catch (e: Exception) {
            Log.e(TAG, "Error selecting app: ${app.name}", e)
            displayGenericErrorDialogSafe(
                getString(R.string.error_app_selection_title),
                getString(R.string.error_app_selection, e.message ?: "Unknown error")
            )
        }
    }
}
