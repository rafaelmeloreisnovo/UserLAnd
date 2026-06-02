package tech.ula.rafaelia

import android.util.Log

object RafaeliaNative {
    private const val TAG = "RafaeliaNative"
    private const val COMPILE_KEY_MIN_LEN = 8
    private const val COMPILE_KEY_MAX_LEN = 64
    private const val COMPILE_KEY_MIN_ENTROPY_MILLI = 3500

    const val COMPILE_FLAG_LICENSE_ACK = 0x00000001
    const val COMPILE_FLAG_COPYRIGHT_ACK = 0x00000002
    const val COMPILE_FLAG_NO_PRIVILEGE_ESCALATION = 0x00000004
    const val COMPILE_FLAG_NO_EXTERNAL_NETWORK = 0x00000008
    const val COMPILE_FLAGS_REQUIRED =
        COMPILE_FLAG_LICENSE_ACK or COMPILE_FLAG_COPYRIGHT_ACK or COMPILE_FLAG_NO_PRIVILEGE_ESCALATION

    const val COMPILE_STATUS_OK = 0x00000000
    const val COMPILE_STATUS_NULL_INPUT = 0x00000001
    const val COMPILE_STATUS_LENGTH_SHORT = 0x00000002
    const val COMPILE_STATUS_LENGTH_LONG = 0x00000004
    const val COMPILE_STATUS_LOW_ENTROPY = 0x00000008
    const val COMPILE_STATUS_ZERO_TAG14 = 0x00000010
    const val COMPILE_STATUS_CRC_MISMATCH = 0x00000020
    const val COMPILE_STATUS_MISSING_LICENSE_FLAGS = 0x00000040

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

    data class CompileKeyReport(
        val status: Int,
        val entropyMilli: Int,
        val tag14: Int,
    ) {
        val accepted: Boolean = status == COMPILE_STATUS_OK
    }

    fun isNativeLibraryAvailable(): Boolean = nativeLibraryLoaded

    fun getLoadError(): Throwable? = loadError

    external fun crc32(data: ByteArray): Int

    private external fun compileKeyReportNative(data: ByteArray, flags: Int, expectedCrc32: Int): Long

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
     * Validates a compile key without storing secrets, allocating native heap, or requesting root.
     * The required flags explicitly acknowledge license/copyright boundaries and reject privilege escalation.
     */
    fun safeCompileKeyReport(
        data: ByteArray,
        flags: Int = COMPILE_FLAGS_REQUIRED,
        expectedCrc32: Int = 0,
    ): CompileKeyReport {
        val packed = if (nativeLibraryLoaded) {
            try {
                compileKeyReportNative(data, flags, expectedCrc32)
            } catch (e: Exception) {
                Log.w(TAG, "Native compile-key validation failed, using fallback", e)
                fallbackCompileKeyPacked(data, flags, expectedCrc32)
            }
        } else {
            fallbackCompileKeyPacked(data, flags, expectedCrc32)
        }
        return unpackCompileKeyReport(packed)
    }

    /**
     * Fallback Java implementation of CRC32.
     */
    private fun fallbackCrc32(data: ByteArray): Int {
        val crc = java.util.zip.CRC32()
        crc.update(data)
        return crc.value.toInt()
    }

    private fun fallbackCompileKeyPacked(data: ByteArray, flags: Int, expectedCrc32: Int): Long {
        var status = COMPILE_STATUS_OK
        if (data.size < COMPILE_KEY_MIN_LEN) status = status or COMPILE_STATUS_LENGTH_SHORT
        if (data.size > COMPILE_KEY_MAX_LEN) status = status or COMPILE_STATUS_LENGTH_LONG
        if ((flags and COMPILE_FLAGS_REQUIRED) != COMPILE_FLAGS_REQUIRED) {
            status = status or COMPILE_STATUS_MISSING_LICENSE_FLAGS
        }

        val entropyMilli = compileKeyEntropyMilli(data)
        val tag14 = compileKeyTag14(data)
        val crc32 = fallbackCrc32(data)

        if (entropyMilli < COMPILE_KEY_MIN_ENTROPY_MILLI) status = status or COMPILE_STATUS_LOW_ENTROPY
        if (tag14 == 0) status = status or COMPILE_STATUS_ZERO_TAG14
        if (expectedCrc32 != 0 && crc32 != expectedCrc32) status = status or COMPILE_STATUS_CRC_MISMATCH

        return (status.toLong() shl 32) or (entropyMilli.toLong() shl 16) or tag14.toLong()
    }

    private fun compileKeyEntropyMilli(data: ByteArray): Int {
        if (data.isEmpty()) return 0
        val seen = BooleanArray(256)
        var unique = 0
        var transitions = 0
        var previous = data[0]
        data.forEachIndexed { index, byte ->
            val unsigned = byte.toInt() and 0xFF
            if (!seen[unsigned]) {
                unique++
                seen[unsigned] = true
            }
            if (index != 0 && byte != previous) transitions++
            previous = byte
        }
        val transitionScore = if (data.size > 1) (transitions * 2000) / (data.size - 1) else 0
        return ((unique * 6000) / 256 + transitionScore).coerceAtMost(8000)
    }

    private fun compileKeyTag14(data: ByteArray): Int {
        if (data.isEmpty()) return 0
        var tag = 0x2A5A
        data.forEachIndexed { index, byte ->
            tag = tag xor ((byte.toInt() and 0xFF) + (index * 0x45D9))
            tag = ((tag shl 5) or (tag ushr 9)) and 0x3FFF
            tag = tag xor (tag ushr 7)
            tag = tag and 0x3FFF
        }
        return tag
    }

    private fun unpackCompileKeyReport(packed: Long): CompileKeyReport {
        return CompileKeyReport(
            status = (packed ushr 32).toInt(),
            entropyMilli = ((packed ushr 16) and 0xFFFF).toInt(),
            tag14 = (packed and 0x3FFF).toInt(),
        )
    }
}
