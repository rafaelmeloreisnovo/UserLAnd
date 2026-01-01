@file:Suppress("ktlint:standard:no-wildcard-imports")

package tech.ula.viewmodel

import androidx.lifecycle.LiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import kotlinx.coroutines.*
import tech.ula.model.entities.Filesystem
import tech.ula.model.entities.Session
import tech.ula.model.repositories.UlaDatabase
import kotlin.coroutines.CoroutineContext

class SessionEditViewModel(
    private val ulaDatabase: UlaDatabase,
) : ViewModel(),
    CoroutineScope {
    private val job = Job()
    override val coroutineContext: CoroutineContext
        get() = Dispatchers.Main + job

    override fun onCleared() {
        job.cancel()
        super.onCleared()
    }

    private val filesystems: LiveData<List<Filesystem>> by lazy {
        ulaDatabase.filesystemDao().getAllFilesystems()
    }

    fun getAllFilesystems(): LiveData<List<Filesystem>> = filesystems

    fun insertSession(
        session: Session,
        coroutineScope: CoroutineScope = this,
    ) = coroutineScope.launch {
        withContext(Dispatchers.IO) {
            ulaDatabase.sessionDao().insertSession(session)
        }
    }

    fun updateSession(
        session: Session,
        coroutineScope: CoroutineScope = this,
    ) = coroutineScope.launch {
        withContext(Dispatchers.IO) {
            ulaDatabase.sessionDao().updateSession(session)
        }
    }
}

class SessionEditViewmodelFactory(
    private val ulaDatabase: UlaDatabase,
) : ViewModelProvider.NewInstanceFactory() {
    @Suppress("UNCHECKED_CAST")
    override fun <T : ViewModel> create(modelClass: Class<T>): T = SessionEditViewModel(ulaDatabase) as T
}
