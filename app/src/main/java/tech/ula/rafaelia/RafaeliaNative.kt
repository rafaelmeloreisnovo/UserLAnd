package tech.ula.rafaelia

import android.util.Log

object RafaeliaNative {
    private const val TAG = "RafaeliaNative"
    private var nativeLibraryLoaded = false
    private var loadError: Throwable? = null

    init {
        try {
            System.loadLibrary("rafaelia")
            nativeLibraryLoaded = true
            Log.i(TAG, "Native library 'rafaelia' loaded successfully")
        } catch (e: UnsatisfiedLinkError) {
            Log.w(TAG, "Failed to load native library 'rafaelia': ${e.message}")
            Log.w(TAG, "App will run in compatibility mode without native functionality")
            loadError = e
            nativeLibraryLoaded = false
        } catch (e: Exception) {
            Log.e(TAG, "Unexpected error loading native library", e)
            loadError = e
            nativeLibraryLoaded = false
        }
    }

    fun isNativeLibraryAvailable(): Boolean = nativeLibraryLoaded

    fun getLoadError(): Throwable? = loadError

    external fun crc32(data: ByteArray): Int

    /**
     * Safe wrapper for crc32 that handles native library unavailability.
     * Returns a fallback CRC32 implementation if native library is not available.
     */
    fun safeCrc32(data: ByteArray): Int {
        if (!nativeLibraryLoaded) {
            Log.d(TAG, "Using Java fallback CRC32 implementation")
            return fallbackCrc32(data)
        }
        return try {
            crc32(data)
        } catch (e: Exception) {
            Log.w(TAG, "Native CRC32 call failed, using fallback", e)
            fallbackCrc32(data)
        }
    }

    /**
     * Fallback Java implementation of CRC32.
     */
    private fun fallbackCrc32(data: ByteArray): Int {
        val crc = java.util.zip.CRC32()
        crc.update(data)
        return crc.value.toInt()
    }
}
