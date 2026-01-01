@file:Suppress("ktlint:standard:no-wildcard-imports")

package tech.ula.ui

import android.app.Activity
import android.content.Context
import android.os.Build
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProvider
import androidx.navigation.fragment.navArgs
import tech.ula.databinding.FragAppDetailsBinding
import tech.ula.model.repositories.UlaDatabase
import tech.ula.utils.*
import tech.ula.viewmodel.AppDetailsEvent
import tech.ula.viewmodel.AppDetailsViewModel
import tech.ula.viewmodel.AppDetailsViewState
import tech.ula.viewmodel.AppDetailsViewmodelFactory

class AppDetailsFragment : Fragment() {
    @Suppress("ktlint:standard:backing-property-naming")
    private var _binding: FragAppDetailsBinding? = null
    private val binding get() = _binding!!

    private lateinit var activityContext: Activity

    private val args: AppDetailsFragmentArgs by navArgs()
    private val app by lazy { args.app!! }

    private val viewModel by lazy {
        val sessionDao = UlaDatabase.getInstance(activityContext).sessionDao()
        val appDetails = AppDetails(activityContext.filesDir.path, activityContext.resources)
        val buildVersion = Build.VERSION.SDK_INT
        val factory =
            AppDetailsViewmodelFactory(
                sessionDao,
                appDetails,
                buildVersion,
                activityContext.getSharedPreferences("apps", Context.MODE_PRIVATE),
            )
        ViewModelProvider(this, factory).get(AppDetailsViewModel::class.java)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        _binding = FragAppDetailsBinding.inflate(inflater, container, false)
        return binding.root
    }

    @Suppress("DEPRECATION")
    override fun onActivityCreated(savedInstanceState: Bundle?) {
        super.onActivityCreated(savedInstanceState)

        activityContext = activity!!
        viewModel.viewState.observe(
            viewLifecycleOwner,
            Observer<AppDetailsViewState> { viewState ->
                viewState?.let {
                    handleViewStateChange(viewState)
                }
            },
        )
        viewModel.submitEvent(AppDetailsEvent.SubmitApp(app))
        setupPreferredServiceTypeRadioGroup()
        setupAutoStartCheckbox()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun handleViewStateChange(viewState: AppDetailsViewState) {
        binding.appsIcon.setImageURI(viewState.appIconUri)
        binding.appsTitle.text = viewState.appTitle
        binding.appsDescription.text = viewState.appDescription
        handleEnableRadioButtons(viewState)
        handleShowStateHint(viewState)

        if (viewState.selectedServiceTypeButton != null) {
            binding.appsServiceTypePreferences.check(viewState.selectedServiceTypeButton)
        }

        binding.checkboxAutoStart.isChecked = viewState.autoStartEnabled
    }

    private fun handleEnableRadioButtons(viewState: AppDetailsViewState) {
        binding.appsSshPreference.isEnabled = viewState.sshEnabled
        binding.appsVncPreference.isEnabled = viewState.vncEnabled

        if (viewState.xsdlEnabled) {
            binding.appsXsdlPreference.isEnabled = true
        } else {
            // Xsdl is unavailable on Android 9 and greater
            binding.appsXsdlPreference.isEnabled = false
            binding.appsXsdlPreference.alpha = 0.5f

            binding.textXsdlVersionSupportedDescription.visibility = View.VISIBLE
        }
    }

    private fun handleShowStateHint(viewState: AppDetailsViewState) {
        if (viewState.describeStateHintEnabled) {
            binding.textDescribeState.visibility = View.VISIBLE
            binding.textDescribeState.setText(viewState.describeStateText!!)
        } else {
            binding.textDescribeState.visibility = View.GONE
        }
    }

    private fun setupPreferredServiceTypeRadioGroup() {
        binding.appsServiceTypePreferences.setOnCheckedChangeListener { _, checkedId ->
            viewModel.submitEvent(AppDetailsEvent.ServiceTypeChanged(checkedId, app))
        }
    }

    private fun setupAutoStartCheckbox() {
        binding.checkboxAutoStart.setOnCheckedChangeListener { _, checked ->
            viewModel.submitEvent(AppDetailsEvent.AutoStartChanged(checked, app))
        }
    }
}
