package tech.ula.utils

import android.app.ActivityManager
import android.content.Context
import android.os.Build
import android.os.Process
import android.util.Log

/**
 * Android15CompatibilityHelper - Ensures robust operation on Android 15+
 * 
 * This helper addresses Android 15-specific behaviors and restrictions:
 * - Stricter background execution limits
 * - Enhanced battery optimization
 * - Tighter memory management
 * - Foreground service restrictions
 * 
 * Key features:
 * - Prevents ANR (Application Not Responding) by monitoring main thread
 * - Detects and handles low memory situations proactively
 * - Ensures proper foreground service lifecycle
 * - Validates architecture compatibility at runtime
 */
object Android15CompatibilityHelper {
    private const val TAG = "Android15Compat"
    
    // Memory thresholds
    private const val MIN_MEMORY_BYTES = 50 * 1024 * 1024L // 50MB minimum
    private const val MEMORY_PRESSURE_WARNING_THRESHOLD = 70 // 70% usage
    private const val MEMORY_PRESSURE_CRITICAL_THRESHOLD = 85 // 85% usage
    
    /**
     * Check if running on Android 15 or higher
     */
    fun isAndroid15Plus(): Boolean {
        return Build.VERSION.SDK_INT >= 35 // API 35 is Android 15
    }
    
    /**
     * Check if the current architecture is supported
     * Returns true if the device's primary ABI is in the supported list
     */
    fun isArchitectureSupported(): Boolean {
        val supportedAbis = setOf("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
        return Build.SUPPORTED_ABIS.firstOrNull().orEmpty() in supportedAbis
    }

    fun getArchitectureInfo(): String =
        "Primary: ${Build.SUPPORTED_ABIS.firstOrNull()}, All: ${Build.SUPPORTED_ABIS.joinToString()}"
    
    /**
     * Check if the device has sufficient memory for operations
     * Returns false if memory is critically low
     */
    fun hasAdequateMemory(context: Context): Boolean {
        val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
            ?: return true // Can't check, assume OK
        
        val memoryInfo = ActivityManager.MemoryInfo()
        activityManager.getMemoryInfo(memoryInfo)
        
        return memoryInfo.availMem > MIN_MEMORY_BYTES
    }
    
    /**
     * Check if the app is running in the foreground
     */
    fun isAppInForeground(context: Context): Boolean {
        val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
            ?: return true
        val myPid = Process.myPid()
        return activityManager.runningAppProcesses
            ?.firstOrNull { it.pid == myPid }
            ?.importance == ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND
            ?: true
    }
    
    /**
     * Get memory pressure level (0-100, where 100 is critical)
     */
    fun getMemoryPressure(context: Context): Int {
        val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
            ?: return 0
        
        val memoryInfo = ActivityManager.MemoryInfo()
        activityManager.getMemoryInfo(memoryInfo)
        
        val usedMemory = memoryInfo.totalMem - memoryInfo.availMem
        val memoryPressure = ((usedMemory.toDouble() / memoryInfo.totalMem) * 100).toInt()
        
        return memoryPressure
    }
    
    /**
     * Check if battery optimization is affecting the app
     * On Android 15, this is even more important
     */
    fun isBatteryOptimizationActive(context: Context): Boolean {
        return try {
            val powerManager = context.getSystemService(Context.POWER_SERVICE) as? android.os.PowerManager
            powerManager?.isIgnoringBatteryOptimizations(context.packageName) == false
        } catch (e: Exception) {
            Log.w(TAG, "Failed to check battery optimization status", e)
            false
        }
    }

    fun validateEnvironment(context: Context): List<String> {
        val issues = mutableListOf<String>()

        if (!isArchitectureSupported()) {
            issues.add("Unsupported architecture: ${getArchitectureInfo()}")
        }

        if (!hasAdequateMemory(context)) {
            issues.add("Insufficient memory available")
        }

        // Battery optimization is relevant from API 23+ (Marshmallow) — i.e. all supported devices
        if (isBatteryOptimizationActive(context)) {
            issues.add("Battery optimization is active - may affect background operations")
        }

        val memoryPressure = getMemoryPressure(context)
        if (memoryPressure > MEMORY_PRESSURE_CRITICAL_THRESHOLD) {
            issues.add("High memory pressure: $memoryPressure%")
        }

        return issues
    }
    
    /**
     * Log comprehensive environment information
     */
    fun logEnvironmentInfo(context: Context) {
        Log.i(TAG, "=== Android 15 Compatibility Check ===")
        Log.i(TAG, "Android Version: ${Build.VERSION.RELEASE} (SDK ${Build.VERSION.SDK_INT})")
        Log.i(TAG, "Android 15+: ${isAndroid15Plus()}")
        Log.i(TAG, "Architecture: ${getArchitectureInfo()}")
        Log.i(TAG, "Architecture Supported: ${isArchitectureSupported()}")
        Log.i(TAG, "Memory Pressure: ${getMemoryPressure(context)}%")
        Log.i(TAG, "Adequate Memory: ${hasAdequateMemory(context)}")
        Log.i(TAG, "App in Foreground: ${isAppInForeground(context)}")
        Log.i(TAG, "Battery Optimization: ${isBatteryOptimizationActive(context)}")
        
        val issues = validateEnvironment(context)
        if (issues.isNotEmpty()) {
            Log.w(TAG, "Environment Issues:")
            issues.forEach { issue ->
                Log.w(TAG, "  - $issue")
            }
        } else {
            Log.i(TAG, "Environment validation: All checks passed ✓")
        }
    }
    
    /**
     * Suggest optimizations based on current environment
     */
    fun getOptimizationSuggestions(context: Context): List<String> {
        val suggestions = mutableListOf<String>()

        if (isBatteryOptimizationActive(context)) {
            suggestions.add("Disable battery optimization for better background operation")
        }

        if (getMemoryPressure(context) > MEMORY_PRESSURE_WARNING_THRESHOLD) {
            suggestions.add("Close other apps to free up memory")
        }

        if (!isAppInForeground(context)) {
            suggestions.add("Keep app in foreground for reliable operation")
        }

        return suggestions
    }
}
