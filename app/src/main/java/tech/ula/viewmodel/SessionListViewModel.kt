@file:Suppress("ktlint:standard:no-wildcard-imports")

package tech.ula.viewmodel

import androidx.lifecycle.LiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import tech.ula.model.entities.Filesystem
import tech.ula.model.entities.Session
import tech.ula.model.repositories.UlaDatabase
import tech.ula.utils.*

class SessionListViewModel(
    private val ulaDatabase: UlaDatabase,
) : ViewModel() {
    private val sessions: LiveData<List<Session>> by lazy {
        ulaDatabase.sessionDao().getAllSessions()
    }

    private val filesystems: LiveData<List<Filesystem>> by lazy {
        ulaDatabase.filesystemDao().getAllFilesystems()
    }

    fun getSessionsAndFilesystems(): LiveData<Pair<List<Session>, List<Filesystem>>> = zipLiveData(sessions, filesystems)

    fun deleteSessionById(id: Long) {
        GlobalScope.launch { ulaDatabase.sessionDao().deleteSessionById(id) }
    }
}

class SessionListViewModelFactory(
    private val ulaDatabase: UlaDatabase,
) : ViewModelProvider.NewInstanceFactory() {
    override fun <T : ViewModel> create(modelClass: Class<T>): T {
        @Suppress("UNCHECKED_CAST")
        return SessionListViewModel(ulaDatabase) as T
    }
}
