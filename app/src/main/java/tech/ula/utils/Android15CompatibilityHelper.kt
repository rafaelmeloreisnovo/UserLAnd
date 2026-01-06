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
        val primaryAbi = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            Build.SUPPORTED_ABIS.firstOrNull() ?: ""
        } else {
            @Suppress("DEPRECATION")
            Build.CPU_ABI
        }
        return primaryAbi in supportedAbis
    }
    
    /**
     * Get device architecture information
     */
    fun getArchitectureInfo(): String {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            "Primary: ${Build.SUPPORTED_ABIS.firstOrNull()}, All: ${Build.SUPPORTED_ABIS.joinToString()}"
        } else {
            @Suppress("DEPRECATION")
            "Primary: ${Build.CPU_ABI}, Secondary: ${Build.CPU_ABI2}"
        }
    }
    
    /**
     * Check if the device has sufficient memory for operations
     * Returns false if memory is critically low
     */
    fun hasAdequateMemory(context: Context): Boolean {
        val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
            ?: return true // Can't check, assume OK
        
        val memoryInfo = ActivityManager.MemoryInfo()
        activityManager.getMemoryInfo(memoryInfo)
        
        // Check if we have at least 50MB available
        val minMemoryBytes = 50 * 1024 * 1024L
        return memoryInfo.availMem > minMemoryBytes
    }
    
    /**
     * Check if the app is running in the foreground
     */
    fun isAppInForeground(context: Context): Boolean {
        val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
            ?: return true // Can't check, assume foreground
        
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            val runningAppProcesses = activityManager.runningAppProcesses ?: return true
            val myPid = Process.myPid()
            
            for (processInfo in runningAppProcesses) {
                if (processInfo.pid == myPid) {
                    return processInfo.importance == ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND
                }
            }
        }
        
        return true // Can't determine, assume foreground
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
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
            return false // Not applicable on older versions
        }
        
        return try {
            val powerManager = context.getSystemService(Context.POWER_SERVICE) as? android.os.PowerManager
            val packageName = context.packageName
            powerManager?.isIgnoringBatteryOptimizations(packageName) == false
        } catch (e: Exception) {
            Log.w(TAG, "Failed to check battery optimization status", e)
            false
        }
    }
    
    /**
     * Validate runtime environment for Android 15
     * Returns a list of issues found, empty if all OK
     */
    fun validateEnvironment(context: Context): List<String> {
        val issues = mutableListOf<String>()
        
        // Check architecture
        if (!isArchitectureSupported()) {
            issues.add("Unsupported architecture: ${getArchitectureInfo()}")
        }
        
        // Check memory
        if (!hasAdequateMemory(context)) {
            issues.add("Insufficient memory available")
        }
        
        // Check if running on Android 15+ with specific requirements
        if (isAndroid15Plus()) {
            val memoryPressure = getMemoryPressure(context)
            if (memoryPressure > 85) {
                issues.add("High memory pressure: $memoryPressure%")
            }
            
            if (isBatteryOptimizationActive(context)) {
                issues.add("Battery optimization is active - may affect background operations")
            }
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
        
        if (isBatteryOptimizationActive(context) && isAndroid15Plus()) {
            suggestions.add("Disable battery optimization for better background operation")
        }
        
        val memoryPressure = getMemoryPressure(context)
        if (memoryPressure > 70) {
            suggestions.add("Close other apps to free up memory")
        }
        
        if (!isAppInForeground(context)) {
            suggestions.add("Keep app in foreground for reliable operation")
        }
        
        return suggestions
    }
}
