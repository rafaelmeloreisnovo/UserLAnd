package tech.ula

import android.app.Application
import android.util.Log
import tech.ula.crash.GlobalExceptionHandler

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
            Log.i(TAG, "UserLAnd application initialized successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Error during application initialization", e)
            // Exception handler will catch this and show crash screen
            throw e
        }
    }
}
