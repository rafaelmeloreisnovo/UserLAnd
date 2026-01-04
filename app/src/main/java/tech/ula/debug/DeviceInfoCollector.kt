package tech.ula.debug

import android.content.Context
import android.content.pm.PackageManager
import android.hardware.Sensor
import android.hardware.SensorManager
import android.os.Build
import android.os.Environment
import android.os.StatFs
import android.util.Log
import org.json.JSONArray
import org.json.JSONObject
import java.io.BufferedReader
import java.io.File
import java.io.FileReader

/**
 * Collects comprehensive device and hardware information for debugging.
 */
class DeviceInfoCollector(private val context: Context) {
    
    companion object {
        private const val TAG = "DeviceInfoCollector"
    }
    
    /**
     * Collect all device information as JSON.
     */
    fun collectFullReport(): JSONObject {
        return JSONObject().apply {
            put("timestamp", System.currentTimeMillis())
            put("device", collectDeviceInfo())
            put("os", collectOSInfo())
            put("cpu", collectCPUInfo())
            put("memory", collectMemoryInfo())
            put("storage", collectStorageInfo())
            put("gpu", collectGPUInfo())
            put("sensors", collectSensorInfo())
            put("kernel", collectKernelInfo())
            put("selinux", collectSELinuxInfo())
        }
    }
    
    /**
     * Collect all device information as formatted text.
     */
    fun collectFullReportText(): String {
        return buildString {
            appendLine("=== UserLAnd Device Report ===")
            appendLine()
            
            appendLine("DEVICE INFORMATION:")
            appendLine("  Manufacturer: ${Build.MANUFACTURER}")
            appendLine("  Model: ${Build.MODEL}")
            appendLine("  Device: ${Build.DEVICE}")
            appendLine("  Product: ${Build.PRODUCT}")
            appendLine("  Brand: ${Build.BRAND}")
            appendLine("  Board: ${Build.BOARD}")
            appendLine("  Hardware: ${Build.HARDWARE}")
            appendLine("  Fingerprint: ${Build.FINGERPRINT}")
            appendLine()
            
            appendLine("OS INFORMATION:")
            appendLine("  Android Version: ${Build.VERSION.RELEASE}")
            appendLine("  SDK: ${Build.VERSION.SDK_INT}")
            appendLine("  Codename: ${Build.VERSION.CODENAME}")
            appendLine("  Security Patch: ${Build.VERSION.SECURITY_PATCH}")
            appendLine()
            
            appendLine("CPU/ABI INFORMATION:")
            appendLine("  Supported ABIs: ${Build.SUPPORTED_ABIS.joinToString(", ")}")
            if (Build.VERSION.SDK_INT >= 21) {
                appendLine("  32-bit ABIs: ${Build.SUPPORTED_32_BIT_ABIS.joinToString(", ")}")
                appendLine("  64-bit ABIs: ${Build.SUPPORTED_64_BIT_ABIS.joinToString(", ")}")
            }
            appendLine("  Processors: ${Runtime.getRuntime().availableProcessors()}")
            appendLine()
            
            val cpuInfo = readCPUInfo()
            if (cpuInfo.isNotEmpty()) {
                appendLine("CPU DETAILS (from /proc/cpuinfo):")
                appendLine(cpuInfo)
                appendLine()
            }
            
            appendLine("MEMORY INFORMATION:")
            val runtime = Runtime.getRuntime()
            appendLine("  Max Memory: ${runtime.maxMemory() / (1024 * 1024)} MB")
            appendLine("  Total Memory: ${runtime.totalMemory() / (1024 * 1024)} MB")
            appendLine("  Free Memory: ${runtime.freeMemory() / (1024 * 1024)} MB")
            appendLine("  Used Memory: ${(runtime.totalMemory() - runtime.freeMemory()) / (1024 * 1024)} MB")
            appendLine()
            
            appendLine("STORAGE INFORMATION:")
            val storagePaths = listOf(
                "Internal" to Environment.getDataDirectory(),
                "External" to Environment.getExternalStorageDirectory(),
                "App Files" to context.filesDir,
                "App Cache" to context.cacheDir
            )
            storagePaths.forEach { (name, path) ->
                try {
                    val stat = StatFs(path.path)
                    val totalBytes = stat.blockCountLong * stat.blockSizeLong
                    val availBytes = stat.availableBlocksLong * stat.blockSizeLong
                    appendLine("  $name:")
                    appendLine("    Total: ${totalBytes / (1024 * 1024)} MB")
                    appendLine("    Available: ${availBytes / (1024 * 1024)} MB")
                } catch (e: Exception) {
                    appendLine("  $name: Error reading (${e.message})")
                }
            }
            appendLine()
            
            appendLine("GPU INFORMATION:")
            appendLine("  (GPU info requires OpenGL context)")
            appendLine()
            
            appendLine("SENSORS:")
            val sensorManager = context.getSystemService(Context.SENSOR_SERVICE) as? SensorManager
            val sensors = sensorManager?.getSensorList(Sensor.TYPE_ALL) ?: emptyList()
            sensors.forEach { sensor ->
                appendLine("  - ${sensor.name} (${sensor.vendor})")
            }
            appendLine()
            
            val kernelVersion = readKernelVersion()
            if (kernelVersion.isNotEmpty()) {
                appendLine("KERNEL VERSION:")
                appendLine(kernelVersion)
                appendLine()
            }
            
            val selinux = readSELinuxStatus()
            if (selinux.isNotEmpty()) {
                appendLine("SELINUX STATUS:")
                appendLine("  $selinux")
                appendLine()
            }
            
            appendLine("=== End of Report ===")
        }
    }
    
    private fun collectDeviceInfo(): JSONObject {
        return JSONObject().apply {
            put("manufacturer", Build.MANUFACTURER)
            put("model", Build.MODEL)
            put("device", Build.DEVICE)
            put("product", Build.PRODUCT)
            put("brand", Build.BRAND)
            put("board", Build.BOARD)
            put("hardware", Build.HARDWARE)
            put("fingerprint", Build.FINGERPRINT)
        }
    }
    
    private fun collectOSInfo(): JSONObject {
        return JSONObject().apply {
            put("android_version", Build.VERSION.RELEASE)
            put("sdk_int", Build.VERSION.SDK_INT)
            put("codename", Build.VERSION.CODENAME)
            put("security_patch", Build.VERSION.SECURITY_PATCH)
        }
    }
    
    private fun collectCPUInfo(): JSONObject {
        return JSONObject().apply {
            put("supported_abis", JSONArray(Build.SUPPORTED_ABIS))
            if (Build.VERSION.SDK_INT >= 21) {
                put("supported_32bit_abis", JSONArray(Build.SUPPORTED_32_BIT_ABIS))
                put("supported_64bit_abis", JSONArray(Build.SUPPORTED_64_BIT_ABIS))
            }
            put("processors", Runtime.getRuntime().availableProcessors())
            val cpuInfo = readCPUInfo()
            if (cpuInfo.isNotEmpty()) {
                put("cpuinfo_details", cpuInfo)
            }
        }
    }
    
    private fun collectMemoryInfo(): JSONObject {
        val runtime = Runtime.getRuntime()
        return JSONObject().apply {
            put("max_memory_mb", runtime.maxMemory() / (1024 * 1024))
            put("total_memory_mb", runtime.totalMemory() / (1024 * 1024))
            put("free_memory_mb", runtime.freeMemory() / (1024 * 1024))
            put("used_memory_mb", (runtime.totalMemory() - runtime.freeMemory()) / (1024 * 1024))
        }
    }
    
    private fun collectStorageInfo(): JSONObject {
        val storage = JSONObject()
        
        val paths = mapOf(
            "internal" to Environment.getDataDirectory(),
            "external" to Environment.getExternalStorageDirectory(),
            "app_files" to context.filesDir,
            "app_cache" to context.cacheDir
        )
        
        paths.forEach { (name, path) ->
            try {
                val stat = StatFs(path.path)
                storage.put(name, JSONObject().apply {
                    put("total_mb", stat.blockCountLong * stat.blockSizeLong / (1024 * 1024))
                    put("available_mb", stat.availableBlocksLong * stat.blockSizeLong / (1024 * 1024))
                })
            } catch (e: Exception) {
                Log.w(TAG, "Failed to read storage info for $name", e)
            }
        }
        
        return storage
    }
    
    private fun collectGPUInfo(): JSONObject {
        return JSONObject().apply {
            put("note", "GPU info requires OpenGL context")
        }
    }
    
    private fun collectSensorInfo(): JSONArray {
        val sensors = JSONArray()
        try {
            val sensorManager = context.getSystemService(Context.SENSOR_SERVICE) as? SensorManager
            sensorManager?.getSensorList(Sensor.TYPE_ALL)?.forEach { sensor ->
                sensors.put(JSONObject().apply {
                    put("name", sensor.name)
                    put("vendor", sensor.vendor)
                    put("type", sensor.type)
                    put("version", sensor.version)
                })
            }
        } catch (e: Exception) {
            Log.w(TAG, "Failed to collect sensor info", e)
        }
        return sensors
    }
    
    private fun collectKernelInfo(): JSONObject {
        return JSONObject().apply {
            val version = readKernelVersion()
            put("version", version)
        }
    }
    
    private fun collectSELinuxInfo(): JSONObject {
        return JSONObject().apply {
            val status = readSELinuxStatus()
            put("status", status)
        }
    }
    
    private fun readCPUInfo(): String {
        return try {
            val file = File("/proc/cpuinfo")
            if (file.exists()) {
                // Limit to first 50 lines and 8KB for safety
                val maxSize = 8192
                val content = file.readText(Charsets.UTF_8).take(maxSize)
                content.lines().take(50).joinToString("\n")
            } else {
                ""
            }
        } catch (e: Exception) {
            Log.w(TAG, "Failed to read /proc/cpuinfo", e)
            ""
        }
    }
    
    private fun readKernelVersion(): String {
        return try {
            val file = File("/proc/version")
            if (file.exists()) {
                file.readText().trim()
            } else {
                ""
            }
        } catch (e: Exception) {
            Log.w(TAG, "Failed to read /proc/version", e)
            ""
        }
    }
    
    private fun readSELinuxStatus(): String {
        return try {
            val file = File("/sys/fs/selinux/enforce")
            if (file.exists()) {
                val enforce = file.readText().trim()
                if (enforce == "1") "Enforcing" else "Permissive"
            } else {
                "Disabled or not available"
            }
        } catch (e: Exception) {
            Log.w(TAG, "Failed to read SELinux status", e)
            "Unknown"
        }
    }
}
