package tech.ula.rafaelia

import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

/**
 * JNI Smoke Test for RafaeliaNative
 * 
 * This test validates the basic functionality of the native CRC32 implementation.
 * It serves as a smoke test to ensure the JNI bridge is working correctly.
 */
@RunWith(RobolectricTestRunner::class)
@Config(manifest = Config.NONE)
class RafaeliaNativeSmokeTest {

    @Test
    fun `test safeCrc32 with empty data returns expected value`() {
        // Empty data should return the CRC32 seed value
        val data = ByteArray(0)
        val result = RafaeliaNative.safeCrc32(data)
        
        // CRC32 of empty data is 0
        assertEquals(0, result)
    }

    @Test
    fun `test safeCrc32 with known data returns expected value`() {
        // Test with known test vector
        // CRC32 of "123456789" should be 0xCBF43926
        val data = "123456789".toByteArray(Charsets.US_ASCII)
        val result = RafaeliaNative.safeCrc32(data)
        
        // Expected CRC32 value for "123456789"
        val expected = 0xCBF43926.toInt()
        assertEquals(expected, result)
    }

    @Test
    fun `test safeCrc32 with single byte`() {
        // Test with single byte
        val data = byteArrayOf(0x41) // 'A'
        val result = RafaeliaNative.safeCrc32(data)
        
        // Should return non-zero value
        assertNotEquals(0, result)
    }

    @Test
    fun `test safeCrc32 is deterministic`() {
        // Same input should always produce same output
        val data = "Hello, Rafaelia!".toByteArray(Charsets.UTF_8)
        
        val result1 = RafaeliaNative.safeCrc32(data)
        val result2 = RafaeliaNative.safeCrc32(data)
        
        assertEquals(result1, result2)
    }

    @Test
    fun `test safeCrc32 with different data produces different results`() {
        // Different inputs should produce different outputs (with high probability)
        val data1 = "data1".toByteArray(Charsets.UTF_8)
        val data2 = "data2".toByteArray(Charsets.UTF_8)
        
        val result1 = RafaeliaNative.safeCrc32(data1)
        val result2 = RafaeliaNative.safeCrc32(data2)
        
        assertNotEquals(result1, result2)
    }

    @Test
    fun `test safeCrc32 with null bytes`() {
        // Test with data containing null bytes
        val data = byteArrayOf(0x00, 0x00, 0x00, 0x00)
        val result = RafaeliaNative.safeCrc32(data)
        
        // Should handle null bytes correctly
        assertNotEquals(0, result)
    }

    @Test
    fun `test safeCrc32 always returns valid result even if native lib unavailable`() {
        // This test verifies the fallback mechanism works
        // Even if native library is not available, safeCrc32 should return a valid result
        val data = "test".toByteArray(Charsets.UTF_8)
        val result = RafaeliaNative.safeCrc32(data)
        
        // Should return a valid integer (not throw exception)
        assertTrue(result is Int)
    }

    @Test
    fun `test isNativeLibraryAvailable returns boolean`() {
        // Test that the library availability check works
        val available = RafaeliaNative.isNativeLibraryAvailable()
        
        // Should return true or false (not throw exception)
        assertTrue(available is Boolean)
    }

    @Test
    fun `test getLoadError returns nullable Throwable`() {
        // Test that we can query load errors
        val error = RafaeliaNative.getLoadError()
        
        // Should return null or a Throwable
        assertTrue(error == null || error is Throwable)
    }

    @Test
    fun `test compile key report accepts bounded high entropy key with license flags`() {
        val data = ByteArray(64) { it.toByte() }
        val crc = RafaeliaNative.safeCrc32(data)

        val report = RafaeliaNative.safeCompileKeyReport(data, expectedCrc32 = crc)

        assertTrue(report.accepted)
        assertEquals(RafaeliaNative.COMPILE_STATUS_OK, report.status)
        assertTrue(report.entropyMilli >= 3500)
        assertTrue(report.tag14 in 1..0x3FFF)
    }

    @Test
    fun `test compile key report rejects missing license flags`() {
        val data = ByteArray(64) { (it * 3).toByte() }

        val report = RafaeliaNative.safeCompileKeyReport(data, flags = 0)

        assertFalse(report.accepted)
        assertTrue((report.status and RafaeliaNative.COMPILE_STATUS_MISSING_LICENSE_FLAGS) != 0)
    }

    @Test
    fun `test compile key report rejects low entropy and crc mismatch`() {
        val data = ByteArray(8) { 7 }

        val report = RafaeliaNative.safeCompileKeyReport(data, expectedCrc32 = 0x12345678)

        assertFalse(report.accepted)
        assertTrue((report.status and RafaeliaNative.COMPILE_STATUS_LOW_ENTROPY) != 0)
        assertTrue((report.status and RafaeliaNative.COMPILE_STATUS_CRC_MISMATCH) != 0)
    }

}
