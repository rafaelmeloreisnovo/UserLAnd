package tech.ula.utils

import android.content.Context
import android.os.Build
import android.system.Os
import java.io.File
import java.lang.NullPointerException

class UlaFiles(
    context: Context,
    libDirPath: String,
    private val symlinker: Symlinker = Symlinker(),
) {
    val filesDir: File = context.filesDir
    val libDir: File = File(libDirPath)
    val supportDir: File = File(filesDir, "support")
    val emulatedScopedDir = context.getExternalFilesDir(null)!!
    val emulatedUserDir = File(emulatedScopedDir, "storage")

    val sdCardScopedDir: File? = resolveSdCardScopedStorage(context)
    val sdCardUserDir: File? =
        if (sdCardScopedDir != null) {
            File(sdCardScopedDir, "storage")
        } else {
            null
        }

    val busybox = File(supportDir, "busybox")
    val proot = File(supportDir, "proot")

    init {
        emulatedUserDir.mkdirs()
        sdCardUserDir?.mkdirs()

        setupLinks()
    }

    fun makePermissionsUsable(
        containingDirectoryPath: String,
        filename: String,
    ) {
        val commandToRun = arrayListOf("chmod", "0777", filename)

        val containingDirectory = File(containingDirectoryPath)
        containingDirectory.mkdirs()

        val pb = ProcessBuilder(commandToRun)
        pb.directory(containingDirectory)

        val process = pb.start()
        process.waitFor()
    }

    private fun resolveSdCardScopedStorage(context: Context): File? {
        // Allegedly returns at most 2 elements, if there is a physical external storage device,
        // according to https://developer.android.com/training/data-storage/files at
        // 'Select between multiple storage locations'
        val externals = context.getExternalFilesDirs(null)
        return if (externals.size > 1) {
            externals[1]
        } else {
            null
        }
    }

    // Lib files must start with 'lib' and end with '.so.'
    private fun String.toSupportName(): String = this.substringAfter("lib_").substringBeforeLast(".so")

    @Throws(NullPointerException::class, NoSuchFileException::class, Exception::class)
    private fun setupLinks() {
        supportDir.mkdirs()

        // Regex to remove versioned suffixes like ".a10.so", ".a11.so", ".a10a.so", etc.
        val versionedSoRegex = Regex("""\\.a[0-9]+[a-zA-Z0-9\-]*\\.so$""")

        libDir.listFiles()!!.forEach { libFile ->
            var libFileName = libFile.name

            // Normalize versioned suffixes (e.g., lib_proot.a10.so -> lib_proot.so)
            if (libFileName.contains(".a") && libFileName.endsWith(".so")) {
                libFileName = libFileName.replace(versionedSoRegex, ".so")
            }

            // Maintain compatibility with previous behavior for certain libs
            if (libFileName.startsWith("lib_proot.") ||
                libFileName.startsWith("lib_libtalloc") ||
                libFileName.startsWith("lib_loader") ) {
                // Already normalized above; special case: if result doesn't end in .so, skip
                if (!libFileName.endsWith(".so")) {
                    libFileName = libFile.name
                }
            }

            val name = libFileName.toSupportName()
            val linkFile = File(supportDir, name)
            linkFile.delete()

            try {
                // Try to create symlink with the project's Symlinker
                symlinker.createSymlink(libFile.path, linkFile.path)
            } catch (symlinkErr: Exception) {
                // Fallbacks: android.system.Os.symlink (API21+) or copy file
                try {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                        try {
                            Os.symlink(libFile.path, linkFile.path)
                        } catch (osErr: Exception) {
                            // Last resort: copy the file
                            libFile.copyTo(linkFile, overwrite = true)
                        }
                    } else {
                        libFile.copyTo(linkFile, overwrite = true)
                    }
                } catch (fallbackErr: Exception) {
                    // If all fails, log and continue (don't throw to avoid interrupting init)
                    android.util.Log.w("UlaFiles", "setupLinks: fallback failed for ${libFile.path}: ${fallbackErr.message}")
                }
            }

            // Try to set executable (important for busybox/proot)
            try {
                if (linkFile.exists()) {
                    linkFile.setExecutable(true, false)
                }
                if (libFile.exists()) {
                    libFile.setExecutable(true, false)
                }
            } catch (_: Exception) {
            }
        }
    }

    fun getArchType(): String {
        val usedABI = File(libDir, "lib_arch.so").readText()
        return translateABI(usedABI)
    }

    private fun translateABI(abi: String): String =
        when (abi) {
            "arm64-v8a" -> "arm64"
            "armeabi-v7a" -> "arm"
            "x86_64" -> "x86_64"
            "x86" -> "x86"
            else -> ""
        }
}

class Symlinker {
    fun createSymlink(
        targetPath: String,
        linkPath: String,
    ) {
        Os.symlink(targetPath, linkPath)
    }
}
