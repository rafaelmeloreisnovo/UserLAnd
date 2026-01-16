package tech.ula.utils

import android.util.Log
import java.util.concurrent.Semaphore
import java.util.concurrent.TimeUnit

/**
 * Limits concurrent subprocesses to stay under Android 15 phantom process constraints.
 * Thread-based concurrency should be used for scale beyond this limit.
 */
object ProcessLimiter {
    private const val TAG = "ProcessLimiter"

    // Android 15+ enforces tighter background/phantom process limits (effectively ~32).
    // Keep a safety margin to avoid system-side pressure.
    private const val MAX_CONCURRENT_PROCESSES = 28
    private val semaphore = Semaphore(MAX_CONCURRENT_PROCESSES, true)

    fun acquirePermit(
        owner: String,
        timeoutMillis: Long,
    ): Boolean {
        val acquired = semaphore.tryAcquire(timeoutMillis, TimeUnit.MILLISECONDS)
        if (!acquired) {
            Log.w(
                TAG,
                "Process permit timeout for $owner after ${timeoutMillis}ms (limit=$MAX_CONCURRENT_PROCESSES)",
            )
        }
        return acquired
    }

    fun releasePermit(owner: String) {
        semaphore.release()
        Log.d(TAG, "Released process permit for $owner")
    }
}
