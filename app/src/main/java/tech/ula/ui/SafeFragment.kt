package tech.ula.ui

import android.os.Bundle
import android.util.Log
import androidx.fragment.app.Fragment
import androidx.navigation.NavController
import androidx.navigation.fragment.findNavController

/**
 * Base fragment with safe navigation helpers.
 * Prevents crashes from navigation when fragment is not in proper lifecycle state.
 */
abstract class SafeFragment : Fragment() {
    
    companion object {
        private const val TAG = "SafeFragment"
    }
    
    /**
     * Safe navigation helper that checks fragment lifecycle state.
     * Returns true if navigation was successful, false otherwise.
     */
    protected fun safeNavigate(
        actionId: Int,
        args: Bundle? = null,
        logTag: String = TAG
    ): Boolean {
        return try {
            // Check if fragment is properly attached
            if (!isAdded || view == null) {
                Log.w(logTag, "Cannot navigate - fragment not attached or view is null")
                return false
            }
            
            // Check if we're in proper lifecycle state
            if (!lifecycle.currentState.isAtLeast(androidx.lifecycle.Lifecycle.State.STARTED)) {
                Log.w(logTag, "Cannot navigate - fragment not in started state")
                return false
            }
            
            // Attempt navigation
            val navController = findNavController()
            if (args != null) {
                navController.navigate(actionId, args)
            } else {
                navController.navigate(actionId)
            }
            true
        } catch (e: IllegalStateException) {
            Log.e(logTag, "Navigation failed - illegal state", e)
            false
        } catch (e: Exception) {
            Log.e(logTag, "Navigation failed - unexpected error", e)
            false
        }
    }
    
    /**
     * Get NavController safely, returns null if not available.
     */
    protected fun getNavControllerSafely(): NavController? {
        return try {
            if (!isAdded || view == null) {
                null
            } else {
                findNavController()
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to get NavController", e)
            null
        }
    }
}
