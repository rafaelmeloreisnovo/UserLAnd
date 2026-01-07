@file:Suppress("ktlint:standard:no-wildcard-imports")

package tech.ula

/*
 * This file is part of UserLAnd.
 *
 * UserLAnd is free software: you can redistribute it and/or modify
 * it underunderterms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * UserLAnd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with UserLAnd.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Modifications (hardened service, timeout, debug, concurrency):
 *   © 2025 Rafael Melo Reis (∆RafaelVerboΩ)
 *   Summary: safer session lifecycle, bounded startup polling, optional debug logging,
 *            and thread-safe tracking of active sessions to reduce crashes, freezes and lag.
 */

import android.app.Service
import android.content.ActivityNotFoundException
import android.content.Intent
import android.net.Uri
import android.os.IBinder
import android.os.SystemClock
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import kotlinx.coroutines.*
import tech.ula.model.entities.App
import tech.ula.model.entities.ServiceType
import tech.ula.model.entities.Session
import tech.ula.model.repositories.UlaDatabase
import tech.ula.utils.*
import java.util.concurrent.ConcurrentHashMap

class ServerService : Service() {
    companion object {
        const val SERVER_SERVICE_RESULT: String = "tech.ula.ServerService.RESULT"

        // Stability / latency settings
        private const val SESSION_START_TIMEOUT_MILLIS = 30_000L
        private const val POLLING_INTERVAL_MILLIS = 100L
        private const val START_FAILURE_WINDOW_MILLIS = 120_000L
        private const val MAX_START_FAILURES = 3

        // Default client packages (can be overridden via SharedPreferences)
        const val DEFAULT_VNC_PACKAGE = "com.iiordanov.freebVNC"
        const val DEFAULT_XSDL_PACKAGE = "x.org.server"

        // Preference keys
        const val KEY_DEV_DEBUG_ENABLED = "dev_debugger_enabled"
        const val KEY_FORCE_OLD_COMPAT = "force_old_compatibility"
        const val KEY_SAFE_MODE_ENABLED = "safety_guard_enabled"
    }

    // Service coroutine scope
    private val serviceJob = SupervisorJob()
    private val serviceScope = CoroutineScope(Dispatchers.IO + serviceJob)

    // Thread-safe map of active sessions
    private val activeSessions: MutableMap<Long, Session> = ConcurrentHashMap()
    private val startFailures: MutableMap<Long, ArrayDeque<Long>> = ConcurrentHashMap()
    private val startInProgress: MutableSet<Long> = ConcurrentHashMap.newKeySet()

    private lateinit var broadcaster: LocalBroadcastManager

    private val notificationManager: NotificationConstructor by lazy {
        NotificationConstructor(this)
    }

    private val busyboxExecutor by lazy {
        val ulaFiles = UlaFiles(this, this.applicationInfo.nativeLibraryDir)
        val prootDebugLogger = ProotDebugLogger(this.defaultSharedPreferences, ulaFiles)
        BusyboxExecutor(ulaFiles, prootDebugLogger)
    }

    private val safetyStartupGuard by lazy {
        SafetyStartupGuard(this.defaultSharedPreferences)
    }

    private val dnsConfigHelper by lazy {
        DnsConfigHelper(this)
    }

    private val localServerManager by lazy {
        LocalServerManager(
            applicationFilesDirPath = this.filesDir.path,
            busyboxExecutor = busyboxExecutor,
            context = this,
            dnsConfigHelper = dnsConfigHelper,
        )
    }

    override fun onCreate() {
        super.onCreate()
        broadcaster = LocalBroadcastManager.getInstance(this)
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(
        intent: Intent?,
        flags: Int,
        startId: Int,
    ): Int {
        super.onStartCommand(intent, flags, startId)

        val type = intent?.getStringExtra("type")
        val isDevDebugEnabled = this.defaultSharedPreferences.getBoolean(KEY_DEV_DEBUG_ENABLED, false)
        val isSafetyGuardEnabled = this.defaultSharedPreferences.getBoolean(KEY_SAFE_MODE_ENABLED, true)

        if (isDevDebugEnabled) {
            busyboxExecutor.logDebug("START_COMMAND received: type=$type")
        }

        when (type) {
            "start" -> {
                val session: Session? = intent.getParcelableExtra("session")
                if (session == null) {
                    busyboxExecutor.logError("Missing 'session' extra for start command")
                    sendDialogBroadcast("missingSessionExtra")
                } else {
                    if (isSafetyGuardEnabled && safetyStartupGuard.isSafeModeActive()) {
                        busyboxExecutor.logError("Safety guard active, refusing to start ${session.name}")
                        sendDialogBroadcast("safetyGuardActive")
                    } else {
                        serviceScope.launch { startSession(session, isDevDebugEnabled, isSafetyGuardEnabled) }
                    }
                }
            }

            "stopApp" -> {
                val app: App? = intent.getParcelableExtra("app")
                if (app == null) {
                    busyboxExecutor.logError("Missing 'app' extra for stopApp command")
                } else {
                    stopApp(app)
                }
            }

            "restartRunningSession" -> {
                val session: Session? = intent.getParcelableExtra("session")
                if (session == null) {
                    busyboxExecutor.logError("Missing 'session' extra for restartRunningSession")
                } else {
                    startClient(session)
                }
            }

            "kill" -> {
                val session: Session? = intent.getParcelableExtra("session")
                if (session == null) {
                    busyboxExecutor.logError("Missing 'session' extra for kill command")
                } else {
                    killSession(session, isSafetyGuardEnabled)
                }
            }

            "filesystemIsBeingDeleted" -> {
                val filesystemId: Long = intent.getLongExtra("filesystemId", -1)
                if (filesystemId == -1L) {
                    busyboxExecutor.logError("Missing 'filesystemId' for filesystemIsBeingDeleted")
                } else {
                    serviceScope.launch { cleanUpFilesystem(filesystemId) }
                }
            }

            "stopAll" -> {
                serviceScope.launch {
                    activeSessions.values.toList().forEach { stopSessionInternal(it, false) }
                }
            }

            else -> {
                busyboxExecutor.logError("Unknown command type='$type'")
            }
        }

        return START_STICKY
    }

    // Used in conjunction with manifest attribute `android:stopWithTask="true"`
    // to clean up when app is swiped away.
    override fun onTaskRemoved(rootIntent: Intent?) {
        super.onTaskRemoved(rootIntent)
        // Try to gracefully stop all running sessions
        serviceScope.launch {
            activeSessions.values.toList().forEach { stopSessionInternal(it, false) }
        }
        stopForeground(true)
        stopSelf()
    }

    override fun onDestroy() {
        super.onDestroy()
        // Cancel all pending coroutines for this service
        serviceJob.cancel()
    }

    private fun stopServiceIfIdle() {
        if (activeSessions.isEmpty()) {
            stopForeground(true)
            stopSelf()
        }
    }

    private fun shouldBlockSessionStart(session: Session): Boolean {
        val now = SystemClock.elapsedRealtime()
        val failures = startFailures[session.id] ?: return false
        synchronized(failures) {
            pruneStartFailures(failures, now)
            return failures.size >= MAX_START_FAILURES
        }
    }

    private fun recordStartFailure(
        session: Session,
        reason: String,
    ) {
        val now = SystemClock.elapsedRealtime()
        val failures =
            startFailures.getOrPut(session.id) {
                ArrayDeque()
            }
        synchronized(failures) {
            failures.addLast(now)
            pruneStartFailures(failures, now)
        }
        busyboxExecutor.logError("Start guard recorded failure for ${session.name}: $reason")
    }

    private fun recordStartSuccess(session: Session) {
        startFailures.remove(session.id)
    }

    private fun pruneStartFailures(
        failures: ArrayDeque<Long>,
        now: Long,
    ) {
        while (failures.isNotEmpty() && now - failures.first() > START_FAILURE_WINDOW_MILLIS) {
            failures.removeFirst()
        }
    }

    private fun removeSession(session: Session) {
        activeSessions.remove(session.pid)
        stopServiceIfIdle()
    }

    private fun updateSession(session: Session) =
        serviceScope.launch {
            try {
                UlaDatabase.getInstance(this@ServerService).sessionDao().updateSession(session)
            } catch (e: Exception) {
                busyboxExecutor.logError("Failed to update session ${session.name}: ${e.message}")
            }
        }

    private fun killSession(
        session: Session,
        registerFailure: Boolean,
    ) {
        serviceScope.launch {
            stopSessionInternal(session, registerFailure)
        }
    }

    /**
     * Encapsulates session shutdown logic with error handling.
     */
    private suspend fun stopSessionInternal(
        session: Session,
        registerFailure: Boolean,
    ) {
        try {
            localServerManager.stopService(session)
        } catch (e: Exception) {
            busyboxExecutor.logError("Error stopping service for ${session.name}: ${e.message}")
        } finally {
            session.active = false
            updateSession(session)
            removeSession(session)
            if (registerFailure) {
                registerSafetyFailure("stop_failure")
            }
        }
    }

    /**
     * Starts a session with:
     * - foreground notification
     * - server startup
     * - bounded polling with timeout
     * - client activation
     */
    private suspend fun startSession(
        session: Session,
        isDevDebugEnabled: Boolean,
        isSafetyGuardEnabled: Boolean,
    ) {
        if (!startInProgress.add(session.id)) {
            busyboxExecutor.logError("Start already in progress for ${session.name}; skipping duplicate start")
            return
        }

        if (shouldBlockSessionStart(session)) {
            busyboxExecutor.logError("Start guard blocked session ${session.name} after repeated failures")
            startInProgress.remove(session.id)
            return
        }

        try {
            if (isDevDebugEnabled) {
                busyboxExecutor.enableDebug()
            }

            val guardEnabled = this.defaultSharedPreferences.getBoolean(SafetyGuard.KEY_ENABLED, true)
            if (guardEnabled) {
                val guardState = SafetyGuard.evaluate(this)
                if (guardState.safeModeActive) {
                    SafetyGuard.recordEvent(this, guardState, "safe_mode_boot")
                    busyboxExecutor.logError("SafetyGuard safe mode active - skipping session start for ${session.name}")
                    stopServiceIfIdle()
                    return
                }

                if (guardState.isTriggered) {
                    SafetyGuard.recordEvent(this, guardState, "start_session")
                    if (guardState.delayMillis > 0L) {
                        delay(guardState.delayMillis)
                    }
                }
            }

            startForeground(
                NotificationConstructor.SERVICE_NOTIFICATION_ID,
                notificationManager.buildPersistentServiceNotification(),
            )

            val pid =
                try {
                    localServerManager.startServer(session)
                } catch (e: Exception) {
                    busyboxExecutor.logCritical("startServer failed for ${session.name}: ${e.message}")
                    recordStartFailure(session, "startServer exception: ${e.message}")
                    sendDialogBroadcast("sessionStartFailed")
                    if (isSafetyGuardEnabled) {
                        registerSafetyFailure("start_server_exception")
                    }
                    stopServiceIfIdle()
                    return
                }

            if (pid <= 0L) {
                busyboxExecutor.logCritical(
                    "startServer returned non-positive pid $pid for ${session.name}",
                )
                recordStartFailure(session, "non-positive pid: $pid")
                sendDialogBroadcast("sessionStartFailed")
                if (isSafetyGuardEnabled) {
                    registerSafetyFailure("start_server_invalid_pid")
                }
                stopServiceIfIdle()
                return
            }

            session.pid = pid

            val executionResult =
                withTimeoutOrNull(SESSION_START_TIMEOUT_MILLIS) {
                    while (!localServerManager.isServerRunning(session)) {
                        delay(POLLING_INTERVAL_MILLIS)
                    }
                    true
                }

            if (executionResult == null) {
                busyboxExecutor.logError(
                    "Session startup timed out after $SESSION_START_TIMEOUT_MILLIS ms for ${session.name}",
                )
                recordStartFailure(session, "startup timeout")
                stopSessionInternal(session, registerFailure = true)
                sendDialogBroadcast("sessionTimeoutError")
                return
            }

            session.active = true
            updateSession(session)
            activeSessions[session.pid] = session
            recordStartSuccess(session)

            startClient(session)
        } catch (e: Exception) {
            busyboxExecutor.logCritical("CRITICAL ERROR in startSession for ${session.name}: ${e.message}")
            recordStartFailure(session, "critical start error: ${e.message}")
            stopSessionInternal(session, registerFailure = true)
            sendDialogBroadcast("criticalServiceError")
        } finally {
            startInProgress.remove(session.id)
            if (isDevDebugEnabled) {
                busyboxExecutor.disableDebug()
            }
        }
    }

    private fun stopApp(app: App) {
        val appSessions =
            activeSessions.filter { (_, session) ->
                session.name == app.name
            }
        appSessions.forEach { (_, session) ->
            killSession(session, false)
        }
    }

    private fun registerSafetyFailure(reason: String) {
        val activation = safetyStartupGuard.recordFailure(reason)
        if (activation != null) {
            sendDialogBroadcast("safetyGuardActivated")
        }
    }

    private fun startClient(session: Session) {
        when (session.serviceType) {
            ServiceType.Ssh -> startSshClient(session)
            ServiceType.Vnc -> startVncClient(session)
            ServiceType.Xsdl -> startXsdlClient(session)
            else -> sendDialogBroadcast("unhandledSessionServiceType")
        }
        sendSessionActivatedBroadcast()
    }

    private fun startSshClient(session: Session) {
        val connectBotIntent =
            Intent().apply {
                action = Intent.ACTION_VIEW
                data = Uri.parse("ssh://${session.username}@localhost:2022/#userland")
                flags = Intent.FLAG_ACTIVITY_NEW_TASK
            }

        try {
            startActivity(connectBotIntent)
        } catch (e: ActivityNotFoundException) {
            sendDialogBroadcast("sshClientMissing")
        } catch (e: Exception) {
            busyboxExecutor.logError("Failed to start SSH client: ${e.message}")
            sendDialogBroadcast("sshClientError")
        }
    }

    private fun startVncClient(session: Session) {
        val packageName =
            this.defaultSharedPreferences
                .getString("pref_vnc_package", DEFAULT_VNC_PACKAGE)!!

        val bVncIntent =
            Intent().apply {
                action = Intent.ACTION_VIEW
                type = "application/vnd.vnc"
                data =
                    Uri.parse(
                        "vnc://127.0.0.1:5951/?VncUsername=${session.username}&VncPassword=${session.vncPassword}",
                    )
                flags = Intent.FLAG_ACTIVITY_NEW_TASK
            }

        try {
            if (clientIsPresent(bVncIntent)) {
                startActivity(bVncIntent)
            } else {
                getClient(packageName)
            }
        } catch (e: ActivityNotFoundException) {
            sendDialogBroadcast("vncClientMissing")
        } catch (e: Exception) {
            busyboxExecutor.logError("Failed to start VNC client: ${e.message}")
            sendDialogBroadcast("vncClientError")
        }
    }

    private fun startXsdlClient(session: Session) {
        val packageName =
            this.defaultSharedPreferences
                .getString("pref_xsdl_package", DEFAULT_XSDL_PACKAGE)!!

        val xsdlIntent =
            Intent().apply {
                flags = Intent.FLAG_ACTIVITY_NEW_TASK
                data = Uri.parse("x11://give.me.display:4721")
            }

        try {
            if (clientIsPresent(xsdlIntent)) {
                startActivity(xsdlIntent)
            } else {
                getClient(packageName)
            }
        } catch (e: ActivityNotFoundException) {
            sendDialogBroadcast("xsdlClientMissing")
        } catch (e: Exception) {
            busyboxExecutor.logError("Failed to start XSDL client: ${e.message}")
            sendDialogBroadcast("xsdlClientError")
        }
    }

    private fun clientIsPresent(intent: Intent): Boolean {
        val activities = packageManager.queryIntentActivities(intent, 0)
        return activities.isNotEmpty()
    }

    private fun getClient(packageName: String) {
        val intent =
            Intent(Intent.ACTION_VIEW, Uri.parse("market://details?id=$packageName")).apply {
                flags = Intent.FLAG_ACTIVITY_NEW_TASK
            }
        try {
            startActivity(intent)
        } catch (err: ActivityNotFoundException) {
            sendDialogBroadcast("playStoreMissingForClient")
        }
    }

    private fun cleanUpFilesystem(filesystemId: Long) {
        activeSessions.values
            .filter { it.filesystemId == filesystemId }
            .forEach { session ->
                killSession(session, false)
            }
    }

    private fun sendSessionActivatedBroadcast() {
        val intent =
            Intent(SERVER_SERVICE_RESULT)
                .putExtra("type", "sessionActivated")
        broadcaster.sendBroadcast(intent)
    }

    private fun sendDialogBroadcast(type: String) {
        val intent =
            Intent(SERVER_SERVICE_RESULT)
                .putExtra("type", "dialog")
                .putExtra("dialogType", type)
        broadcaster.sendBroadcast(intent)
    }
}
