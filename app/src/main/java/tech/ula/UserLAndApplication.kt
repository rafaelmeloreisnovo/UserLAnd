package tech.ula

import android.app.Application
import android.util.Log
import tech.ula.crash.GlobalExceptionHandler
import tech.ula.utils.DeviceOptimizationHelper
import tech.ula.utils.MemoryMonitor
import tech.ula.utils.SafetyGuard

/**
 * UserLAnd Application class with safe initialization.
 * Installs global exception handler to prevent silent crashes.
 */
class UserLAndApplication : Application() {
    
    companion object {
        private const val TAG = "UserLAndApplication"
    }
    
    override fun onCreate() {
        // Install crash shield FIRST, before anything else
        GlobalExceptionHandler.install(this)
        
        try {
            super.onCreate()
            
            // Log device optimization status for Android 15 / RMX3834
            DeviceOptimizationHelper.logOptimizationStatus()
            
            // Log memory status at startup
            MemoryMonitor.logMemoryStatus(this, TAG)
            SafetyGuard.recordStartupModules(this)
            
            Log.i(TAG, "UserLAnd application initialized successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Error during application initialization", e)
            // Exception handler will catch this and show crash screen
            throw e
        }
    }
    
    override fun onLowMemory() {
        super.onLowMemory()
        Log.w(TAG, "System reports low memory")
        MemoryMonitor.logMemoryStatus(this, TAG)
    }
    
    override fun onTrimMemory(level: Int) {
        super.onTrimMemory(level)
        when (level) {
            TRIM_MEMORY_RUNNING_CRITICAL,
            TRIM_MEMORY_COMPLETE,
            TRIM_MEMORY_MODERATE -> {
                Log.w(TAG, "onTrimMemory called with level=$level")
                MemoryMonitor.logMemoryStatus(this, TAG)
            }
        }
    }
}
