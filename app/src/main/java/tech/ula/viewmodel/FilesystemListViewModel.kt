@file:Suppress("ktlint:standard:no-wildcard-imports")

package tech.ula.viewmodel

import android.content.ContentResolver
import android.net.Uri
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import kotlinx.coroutines.*
import tech.ula.R
import tech.ula.model.daos.FilesystemDao
import tech.ula.model.daos.SessionDao
import tech.ula.model.entities.Filesystem
import tech.ula.model.entities.Session
import tech.ula.utils.ExecutionResult
import tech.ula.utils.FailedExecution
import tech.ula.utils.FilesystemManager
import java.io.File
import java.io.IOException
import java.lang.Exception
import kotlin.coroutines.CoroutineContext

sealed class FilesystemListViewState

sealed class FilesystemExportState : FilesystemListViewState() {
    data class Update(
        val details: String,
    ) : FilesystemExportState()

    object Success : FilesystemExportState()

    data class Failure(
        val reason: Int,
        val details: String = "",
    ) : FilesystemExportState()
}

sealed class FilesystemDeleteState : FilesystemListViewState() {
    object InProgress : FilesystemDeleteState()

    object Success : FilesystemDeleteState()

    object Failure : FilesystemDeleteState()
}

class FilesystemListViewModel(
    private val filesystemDao: FilesystemDao,
    private val sessionDao: SessionDao,
    private val filesystemManager: FilesystemManager,
) : ViewModel(),
    CoroutineScope {
    private val job = Job()
    override val coroutineContext: CoroutineContext
        get() = Dispatchers.Main + job

    override fun onCleared() {
        job.cancel()
        super.onCleared()
    }

    private val viewState = MutableLiveData<FilesystemListViewState>()
    private val exportUpdateListener: (String) -> Unit = { details ->
        viewState.postValue(FilesystemExportState.Update(details))
    }

    private val unselectedFilesystem = Filesystem(id = -1, name = "UNSELECTED")
    private var filesystemToBackup = unselectedFilesystem

    fun setFilesystemToBackup(filesystem: Filesystem) {
        filesystemToBackup = filesystem
    }

    fun getViewState(): LiveData<FilesystemListViewState> = viewState

    private val filesystems: LiveData<List<Filesystem>> by lazy {
        filesystemDao.getAllFilesystems()
    }

    fun getAllFilesystems(): LiveData<List<Filesystem>> = filesystems

    private val activeSessions: LiveData<List<Session>> by lazy {
        sessionDao.findActiveSessions()
    }

    fun getAllActiveSessions(): LiveData<List<Session>> = activeSessions

    fun deleteFilesystemById(
        id: Long,
        coroutineScope: CoroutineScope = this,
    ) = coroutineScope.launch {
        withContext(Dispatchers.IO) {
            viewState.postValue(FilesystemDeleteState.InProgress)

            try {
                filesystemManager.deleteFilesystem(id)
            } catch (err: IOException) {
                viewState.postValue(FilesystemDeleteState.Failure)
                return@withContext
            }
            filesystemDao.deleteFilesystemById(id)
            viewState.postValue(FilesystemDeleteState.Success)
        }
    }

    fun getFilesystemBackupName(filesystem: Filesystem): String = "${filesystem.name}-${filesystem.distributionType}-rootfs.tar.gz"

    fun startExport(
        filesDir: File,
        publicExternalUri: Uri,
        contentResolver: ContentResolver,
        coroutineScope: CoroutineScope = this,
    ) = coroutineScope.launch {
        when {
            activeSessions.value!!.isNotEmpty() -> {
                viewState.postValue(FilesystemExportState.Failure(R.string.deactivate_sessions))
                return@launch
            }
            filesystemToBackup == unselectedFilesystem -> {
                viewState.postValue(FilesystemExportState.Failure(R.string.error_export_filesystem_not_found))
                return@launch
            }
            else -> {
                compressFilesystemAndExportToStorage(filesDir, publicExternalUri, contentResolver)
            }
        }
    }

    private suspend fun compressFilesystemAndExportToStorage(
        filesDir: File,
        publicExternalUri: Uri,
        contentResolver: ContentResolver,
    ) {
        withContext(Dispatchers.IO) {
            viewState.postValue(FilesystemExportState.Update("Starting export"))
            val tempBackupName = getFilesystemBackupName(filesystemToBackup)
            val localBackup = File(filesDir, tempBackupName)

            val result = filesystemManager.compressFilesystem(filesystemToBackup, localBackup, exportUpdateListener)
            if (localBackupFailed(localBackup, result)) return@withContext

            try {
                localBackup.inputStream().use { inputStream ->
                    contentResolver.openOutputStream(publicExternalUri, "w")?.use { outputStream ->
                        inputStream.copyTo(outputStream)
                    }
                }
            } catch (err: Exception) {
                filesystemToBackup = unselectedFilesystem
                viewState.postValue(FilesystemExportState.Failure(R.string.error_export_copy_public_external_failure))
                return@withContext
            }

            filesystemToBackup = unselectedFilesystem
            localBackup.delete()
            viewState.postValue(FilesystemExportState.Success)
        }
    }

    private fun localBackupFailed(
        localBackup: File,
        result: ExecutionResult,
    ): Boolean {
        if (result is FailedExecution) {
            filesystemToBackup = unselectedFilesystem
            viewState.postValue(FilesystemExportState.Failure(R.string.error_export_execution_failure, result.reason))
            return true
        }

        if (!localBackup.exists() || localBackup.length() <= 0) {
            filesystemToBackup = unselectedFilesystem
            viewState.postValue(FilesystemExportState.Failure(R.string.error_export_local_failure))
            return true
        }

        return false
    }
}

class FilesystemListViewmodelFactory(
    private val filesystemDao: FilesystemDao,
    private val sessionDao: SessionDao,
    private val filesystemManager: FilesystemManager,
) : ViewModelProvider.NewInstanceFactory() {
    @Suppress("UNCHECKED_CAST")
    override fun <T : ViewModel> create(modelClass: Class<T>): T =
        FilesystemListViewModel(filesystemDao, sessionDao, filesystemManager) as T
}
