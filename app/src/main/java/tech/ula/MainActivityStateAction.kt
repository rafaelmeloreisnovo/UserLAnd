package tech.ula

import androidx.annotation.StringRes
import tech.ula.model.entities.Session
import tech.ula.model.repositories.DownloadMetadata
import tech.ula.viewmodel.ActiveSessionsMustBeDeactivated
import tech.ula.viewmodel.AppServiceTypePreferenceRequired
import tech.ula.viewmodel.CanOnlyStartSingleSession
import tech.ula.viewmodel.CheckingForAssetsUpdates
import tech.ula.viewmodel.ClearingSupportFiles
import tech.ula.viewmodel.CopyingDownloads
import tech.ula.viewmodel.DownloadProgress
import tech.ula.viewmodel.FetchingAssetLists
import tech.ula.viewmodel.FilesystemCredentialsRequired
import tech.ula.viewmodel.FilesystemExtractionStep
import tech.ula.viewmodel.IllegalState
import tech.ula.viewmodel.LargeDownloadRequired
import tech.ula.viewmodel.LowStorageAcknowledgementRequired
import tech.ula.viewmodel.ProgressBarOperationComplete
import tech.ula.viewmodel.SessionCanBeRestarted
import tech.ula.viewmodel.SessionCanBeStarted
import tech.ula.viewmodel.StartingSetup
import tech.ula.viewmodel.State
import tech.ula.viewmodel.VerifyingAvailableStorage
import tech.ula.viewmodel.VerifyingFilesystem
import tech.ula.viewmodel.WaitingForInput

sealed class MainActivityStateAction {
    object None : MainActivityStateAction()

    data class ShowToast(
        @StringRes val messageResId: Int,
    ) : MainActivityStateAction()

    data class StartSession(
        val session: Session,
        val type: String,
    ) : MainActivityStateAction()

    object RequestFilesystemCredentials : MainActivityStateAction()

    object RequestAppServiceType : MainActivityStateAction()

    data class ConfirmLargeDownload(
        val downloadRequirements: List<DownloadMetadata>,
    ) : MainActivityStateAction()

    object ConfirmLowStorage : MainActivityStateAction()

    object RequireInactiveSessions : MainActivityStateAction()

    data class ShowProgress(
        @StringRes val messageResId: Int,
    ) : MainActivityStateAction()

    data class ShowExtractionProgress(
        val extractionTarget: String,
    ) : MainActivityStateAction()

    data class ShowDownloadProgress(
        val numComplete: Int,
        val numTotal: Int,
    ) : MainActivityStateAction()

    object CompleteProgress : MainActivityStateAction()

    data class ShowIllegalState(
        val state: IllegalState,
    ) : MainActivityStateAction()
}

object MainActivityStateActionMapper {
    fun from(state: State): MainActivityStateAction =
        when (state) {
            WaitingForInput -> MainActivityStateAction.None
            CanOnlyStartSingleSession -> MainActivityStateAction.ShowToast(R.string.single_session_supported)
            is SessionCanBeStarted -> MainActivityStateAction.StartSession(state.session, START_SESSION)
            is SessionCanBeRestarted -> MainActivityStateAction.StartSession(state.session, RESTART_RUNNING_SESSION)
            FilesystemCredentialsRequired -> MainActivityStateAction.RequestFilesystemCredentials
            AppServiceTypePreferenceRequired -> MainActivityStateAction.RequestAppServiceType
            is LargeDownloadRequired -> MainActivityStateAction.ConfirmLargeDownload(state.downloadRequirements)
            LowStorageAcknowledgementRequired -> MainActivityStateAction.ConfirmLowStorage
            ActiveSessionsMustBeDeactivated -> MainActivityStateAction.RequireInactiveSessions
            StartingSetup -> MainActivityStateAction.ShowProgress(R.string.progress_start_step)
            FetchingAssetLists -> MainActivityStateAction.ShowProgress(R.string.progress_fetching_asset_lists)
            CheckingForAssetsUpdates -> MainActivityStateAction.ShowProgress(R.string.progress_checking_for_required_updates)
            is DownloadProgress -> MainActivityStateAction.ShowDownloadProgress(state.numComplete, state.numTotal)
            CopyingDownloads -> MainActivityStateAction.ShowProgress(R.string.progress_copying_downloads)
            VerifyingFilesystem -> MainActivityStateAction.ShowProgress(R.string.progress_verifying_assets)
            VerifyingAvailableStorage -> MainActivityStateAction.ShowProgress(R.string.progress_verifying_sufficient_storage)
            is FilesystemExtractionStep -> MainActivityStateAction.ShowExtractionProgress(state.extractionTarget)
            ClearingSupportFiles -> MainActivityStateAction.ShowProgress(R.string.progress_clearing_support_files)
            ProgressBarOperationComplete -> MainActivityStateAction.CompleteProgress
            is IllegalState -> MainActivityStateAction.ShowIllegalState(state)
        }

    const val START_SESSION = "start"
    const val RESTART_RUNNING_SESSION = "restartRunningSession"
}
