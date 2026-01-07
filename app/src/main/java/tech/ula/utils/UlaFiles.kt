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
                // BUG FIX: SYMLINK_HELL - Enhanced fallback mechanism for filesystems that don't support symlinks
                // Android filesystems (especially emulated storage) may not support symbolic links.
                // This fallback ensures reliability by copying files physically when symlinks fail.
                android.util.Log.w("UlaFiles", "Symlink creation failed for ${libFile.name}: ${symlinkErr.message}. Attempting fallback...")
                try {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                        try {
                            Os.symlink(libFile.path, linkFile.path)
                            android.util.Log.d("UlaFiles", "Symlink created successfully using Os.symlink for ${libFile.name}")
                        } catch (osErr: Exception) {
                            // Last resort: copy the file physically
                            // This costs disk space but guarantees functionality
                            android.util.Log.w("UlaFiles", "Os.symlink also failed for ${libFile.name}: ${osErr.message}. Copying file physically...")
                            libFile.copyTo(linkFile, overwrite = true)
                            android.util.Log.i("UlaFiles", "File copied successfully as fallback for ${libFile.name}")
                        }
                    } else {
                        libFile.copyTo(linkFile, overwrite = true)
                        android.util.Log.i("UlaFiles", "File copied (API < 21) for ${libFile.name}")
                    }
                } catch (fallbackErr: Exception) {
                    // If all fails, log detailed error and continue (don't throw to avoid interrupting init)
                    android.util.Log.e("UlaFiles", "CRITICAL: All symlink fallback methods failed for ${libFile.path}. File: ${libFile.name}, Error: ${fallbackErr.message}", fallbackErr)
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
