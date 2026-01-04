package tech.ula.utils

import android.app.ActivityManager
import android.content.Context
import android.util.Log

/**
 * Helper to monitor and log memory conditions that could lead to crashes.
 */
object MemoryMonitor {
    private const val TAG = "MemoryMonitor"
    private const val LOW_MEMORY_THRESHOLD_MB = 50
    
    /**
     * Check if device is running low on memory.
     */
    fun isLowMemory(context: Context): Boolean {
        return try {
            val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
            activityManager?.let {
                val memoryInfo = ActivityManager.MemoryInfo()
                it.getMemoryInfo(memoryInfo)
                
                val availableMB = memoryInfo.availMem / (1024 * 1024)
                val isLow = memoryInfo.lowMemory || availableMB < LOW_MEMORY_THRESHOLD_MB
                
                if (isLow) {
                    Log.w(TAG, "Low memory detected: ${availableMB}MB available, threshold: ${memoryInfo.threshold / (1024 * 1024)}MB")
                }
                
                isLow
            } ?: false
        } catch (e: Exception) {
            Log.e(TAG, "Failed to check memory status", e)
            false
        }
    }
    
    /**
     * Log current memory status for debugging.
     */
    fun logMemoryStatus(context: Context, tag: String = TAG) {
        try {
            val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
            activityManager?.let {
                val memoryInfo = ActivityManager.MemoryInfo()
                it.getMemoryInfo(memoryInfo)
                
                val availableMB = memoryInfo.availMem / (1024 * 1024)
                val totalMB = memoryInfo.totalMem / (1024 * 1024)
                val thresholdMB = memoryInfo.threshold / (1024 * 1024)
                val usedMB = totalMB - availableMB
                val percentUsed = (usedMB.toFloat() / totalMB.toFloat() * 100).toInt()
                
                Log.i(tag, "Memory: ${usedMB}/${totalMB}MB (${percentUsed}% used), " +
                          "available: ${availableMB}MB, threshold: ${thresholdMB}MB, " +
                          "lowMemory: ${memoryInfo.lowMemory}")
                
                // Also log runtime memory
                val runtime = Runtime.getRuntime()
                val runtimeMaxMB = runtime.maxMemory() / (1024 * 1024)
                val runtimeTotalMB = runtime.totalMemory() / (1024 * 1024)
                val runtimeFreeMB = runtime.freeMemory() / (1024 * 1024)
                val runtimeUsedMB = runtimeTotalMB - runtimeFreeMB
                
                Log.i(tag, "Runtime: ${runtimeUsedMB}/${runtimeMaxMB}MB " +
                          "(${runtimeUsedMB * 100 / runtimeMaxMB}% used)")
            }
        } catch (e: Exception) {
            Log.e(tag, "Failed to log memory status", e)
        }
    }
    
    /**
     * Request garbage collection if memory is low.
     */
    fun requestGCIfNeeded(context: Context) {
        if (isLowMemory(context)) {
            Log.i(TAG, "Requesting garbage collection due to low memory")
            System.gc()
        }
    }
}
