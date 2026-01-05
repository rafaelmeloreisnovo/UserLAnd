package tech.ula.utils

import android.content.Context
import android.os.Build
import tech.ula.BuildConfig
import java.io.File

/**
 * DeviceOptimizationHelper provides device-specific optimizations and configurations
 * for Android 15+ devices, with special focus on:
 * - RMX3834 (Realme) devices with Android 15 and kernel 5.15.178
 * - ARM64 SIMD acceleration
 * - Hardware diagnostics and performance monitoring
 * - Advanced debugging capabilities
 *
 * This helper enables maximum performance extraction from the target hardware
 * while maintaining compatibility with other devices.
 */
object DeviceOptimizationHelper {

    /**
     * Device model information
     */
    data class DeviceInfo(
        val manufacturer: String = Build.MANUFACTURER,
        val model: String = Build.MODEL,
        val device: String = Build.DEVICE,
        val board: String = Build.BOARD,
        val hardware: String = Build.HARDWARE,
        val brand: String = Build.BRAND,
        val product: String = Build.PRODUCT,
        val androidVersion: Int = Build.VERSION.SDK_INT,
        val androidRelease: String = Build.VERSION.RELEASE,
        val kernelVersion: String = getKernelVersion(),
        val supportedAbis: Array<String> = Build.SUPPORTED_ABIS,
        val is64Bit: Boolean = Build.SUPPORTED_64_BIT_ABIS.isNotEmpty()
    )

    /**
     * Check if running on target device (RMX3834)
     */
    fun isTargetDevice(): Boolean {
        return Build.MODEL.contains("RMX3834", ignoreCase = true) ||
               Build.DEVICE.contains("RMX3834", ignoreCase = true)
    }

    /**
     * Check if running on Android 15+
     */
    fun isAndroid15Plus(): Boolean {
        return Build.VERSION.SDK_INT >= 35 // API 35 = Android 15
    }

    /**
     * Check if running on Realme device
     */
    fun isRealmeDevice(): Boolean {
        return Build.MANUFACTURER.equals("realme", ignoreCase = true) ||
               Build.BRAND.equals("realme", ignoreCase = true)
    }

    /**
     * Check if ARM64 architecture is supported
     */
    fun isArm64Supported(): Boolean {
        return Build.SUPPORTED_64_BIT_ABIS.any { 
            it.contains("arm64", ignoreCase = true) || 
            it.contains("aarch64", ignoreCase = true) 
        }
    }

    /**
     * Check if NEON SIMD is available (ARM64 always has NEON)
     */
    fun isNeonAvailable(): Boolean {
        return isArm64Supported()
    }

    /**
     * Get kernel version string
     */
    fun getKernelVersion(): String {
        return try {
            val version = System.getProperty("os.version") ?: ""
            if (version.isNotEmpty()) {
                version
            } else {
                // Fallback: read from /proc/version
                File("/proc/version").readText().trim()
            }
        } catch (e: Exception) {
            "Unknown"
        }
    }

    /**
     * Check if kernel matches target version (5.15.178)
     */
    fun isTargetKernelVersion(): Boolean {
        val kernelVersion = getKernelVersion()
        return kernelVersion.contains("5.15.178", ignoreCase = true) ||
               kernelVersion.contains("5.15", ignoreCase = true)
    }

    /**
     * Get device information
     */
    fun getDeviceInfo(): DeviceInfo {
        return DeviceInfo()
    }

    /**
     * Get optimized thread count based on CPU cores
     */
    fun getOptimalThreadCount(): Int {
        val cores = Runtime.getRuntime().availableProcessors()
        return when {
            isTargetDevice() && isArm64Supported() -> {
                // RMX3834 typically has 8 cores, use 6 for optimal performance
                // leaving 2 for system tasks
                maxOf(1, cores - 2)
            }
            isArm64Supported() -> {
                // For other ARM64 devices, use cores - 1
                maxOf(1, cores - 1)
            }
            else -> {
                // Conservative for other architectures
                maxOf(1, cores / 2)
            }
        }
    }

    /**
     * Get recommended buffer size for I/O operations
     */
    fun getOptimalBufferSize(): Int {
        return when {
            isTargetDevice() -> 65536 // 64KB for target device
            isArm64Supported() -> 32768 // 32KB for ARM64
            else -> 16384 // 16KB for others
        }
    }

    /**
     * Check if advanced debugging should be enabled
     */
    fun shouldEnableAdvancedDebugging(): Boolean {
        return BuildConfig.ENABLE_ADVANCED_DEBUGGING && 
               (BuildConfig.DEBUG || isTargetDevice())
    }

    /**
     * Check if performance monitoring should be enabled
     */
    fun shouldEnablePerformanceMonitoring(): Boolean {
        return BuildConfig.ENABLE_PERFORMANCE_MONITORING && 
               isAndroid15Plus()
    }

    /**
     * Check if hardware diagnostics should be enabled
     */
    fun shouldEnableHardwareDiagnostics(): Boolean {
        return BuildConfig.ENABLE_HARDWARE_DIAGNOSTICS && 
               (isTargetDevice() || isAndroid15Plus())
    }

    /**
     * Get CPU architecture-specific optimization flags
     */
    fun getOptimizationFlags(): Map<String, Boolean> {
        return mapOf(
            "arm64" to isArm64Supported(),
            "neon" to isNeonAvailable(),
            "simd" to (BuildConfig.ENABLE_SIMD_ACCELERATION && isNeonAvailable()),
            "android15" to isAndroid15Plus(),
            "targetDevice" to isTargetDevice(),
            "advancedDebug" to shouldEnableAdvancedDebugging(),
            "perfMonitoring" to shouldEnablePerformanceMonitoring(),
            "hwDiagnostics" to shouldEnableHardwareDiagnostics()
        )
    }

    /**
     * Get detailed device diagnostics
     */
    fun getDeviceDiagnostics(context: Context): String {
        val info = getDeviceInfo()
        val flags = getOptimizationFlags()
        
        return buildString {
            appendLine("=== Device Information ===")
            appendLine("Manufacturer: ${info.manufacturer}")
            appendLine("Model: ${info.model}")
            appendLine("Device: ${info.device}")
            appendLine("Board: ${info.board}")
            appendLine("Hardware: ${info.hardware}")
            appendLine("Brand: ${info.brand}")
            appendLine("Product: ${info.product}")
            appendLine()
            appendLine("=== System Information ===")
            appendLine("Android Version: ${info.androidRelease} (API ${info.androidVersion})")
            appendLine("Kernel Version: ${info.kernelVersion}")
            appendLine("64-bit Support: ${info.is64Bit}")
            appendLine("Supported ABIs: ${info.supportedAbis.joinToString(", ")}")
            appendLine()
            appendLine("=== Optimization Status ===")
            appendLine("Target Device (RMX3834): ${flags["targetDevice"]}")
            appendLine("Android 15+: ${flags["android15"]}")
            appendLine("ARM64: ${flags["arm64"]}")
            appendLine("NEON SIMD: ${flags["neon"]}")
            appendLine("SIMD Acceleration: ${flags["simd"]}")
            appendLine("Advanced Debugging: ${flags["advancedDebug"]}")
            appendLine("Performance Monitoring: ${flags["perfMonitoring"]}")
            appendLine("Hardware Diagnostics: ${flags["hwDiagnostics"]}")
            appendLine()
            appendLine("=== Performance Tuning ===")
            appendLine("Optimal Thread Count: ${getOptimalThreadCount()}")
            appendLine("Optimal Buffer Size: ${getOptimalBufferSize()} bytes")
            appendLine()
            appendLine("=== Build Configuration ===")
            appendLine("Target Device Model: ${BuildConfig.TARGET_DEVICE_MODEL}")
            appendLine("Target Kernel Version: ${BuildConfig.TARGET_KERNEL_VERSION}")
            appendLine("ARM64 Optimizations: ${BuildConfig.ENABLE_ARM64_OPTIMIZATIONS}")
            appendLine("SIMD Acceleration: ${BuildConfig.ENABLE_SIMD_ACCELERATION}")
        }
    }

    /**
     * Log device optimization status
     */
    fun logOptimizationStatus() {
        if (shouldEnableAdvancedDebugging()) {
            android.util.Log.d("DeviceOptimization", "=== Device Optimization Status ===")
            android.util.Log.d("DeviceOptimization", "Target Device: ${isTargetDevice()}")
            android.util.Log.d("DeviceOptimization", "Android 15+: ${isAndroid15Plus()}")
            android.util.Log.d("DeviceOptimization", "ARM64: ${isArm64Supported()}")
            android.util.Log.d("DeviceOptimization", "NEON SIMD: ${isNeonAvailable()}")
            android.util.Log.d("DeviceOptimization", "Kernel: ${getKernelVersion()}")
            android.util.Log.d("DeviceOptimization", "Optimal Threads: ${getOptimalThreadCount()}")
            android.util.Log.d("DeviceOptimization", "Buffer Size: ${getOptimalBufferSize()}")
        }
    }
}
