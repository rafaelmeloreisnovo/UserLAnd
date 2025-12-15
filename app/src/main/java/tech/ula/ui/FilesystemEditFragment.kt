package tech.ula.ui

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.*
import android.widget.*
import androidx.fragment.app.Fragment
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProvider
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.fragment.navArgs
import tech.ula.R
import tech.ula.databinding.FragFilesystemEditBinding
import tech.ula.model.entities.Filesystem
import tech.ula.model.repositories.UlaDatabase
import tech.ula.utils.*
import tech.ula.viewmodel.*

private const val FILESYSTEM_IMPORT_REQUEST_CODE = 6

class FilesystemEditFragment : Fragment() {

    private var _binding: FragFilesystemEditBinding? = null
    private val binding get() = _binding!!

    private lateinit var activityContext: Activity
    private val args: FilesystemEditFragmentArgs by navArgs()
    private val filesystem: Filesystem by lazy { args.filesystem!! }
    private val editExisting by lazy { args.editExisting }

    private val filesystemEditViewModel: FilesystemEditViewModel by lazy {
        val ulaDatabase = UlaDatabase.getInstance(activityContext)
        val ulaFiles = UlaFiles(activityContext, activityContext.applicationInfo.nativeLibraryDir)
        val prootDebugLogger = ProotDebugLogger(activityContext.defaultSharedPreferences, ulaFiles)
        val busyboxExecutor = BusyboxExecutor(ulaFiles, prootDebugLogger)
        val filesystemManager = FilesystemManager(ulaFiles, busyboxExecutor)
        ViewModelProvider(this, FilesystemEditViewmodelFactory(ulaDatabase.filesystemDao(), filesystemManager))
            .get(FilesystemEditViewModel::class.java)
    }

    private val viewStateObserver = Observer<FilesystemEditViewState> { viewState ->
        viewState?.let { handleViewStateChange(it) }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        _binding = FragFilesystemEditBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        super.onCreateOptionsMenu(menu, inflater)
        inflater.inflate(R.menu.menu_edit, menu)
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return if (item.itemId == R.id.menu_item_add) {
            insertFilesystem()
            true
        } else super.onOptionsItemSelected(item)
    }

    @Suppress("DEPRECATION")
    override fun onActivityCreated(savedInstanceState: Bundle?) {
        super.onActivityCreated(savedInstanceState)
        activityContext = activity!!
        filesystemEditViewModel.getViewState().observe(viewLifecycleOwner, viewStateObserver)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        setupFilesystemNameInput()
        setupFilesystemTypeSpinner()
        setupCredentialInputs()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun setupFilesystemNameInput() {
        binding.textInputFilesystemName.setText(filesystem.name)
        binding.textInputFilesystemName.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(p0: Editable?) { filesystem.name = p0.toString() }
            override fun beforeTextChanged(p0: CharSequence?, p1: Int, p2: Int, p3: Int) {}
            override fun onTextChanged(p0: CharSequence?, p1: Int, p2: Int, p3: Int) {}
        })
    }

    private fun setupFilesystemTypeSpinner() {
        binding.spinnerFilesystemType.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onNothingSelected(parent: AdapterView<*>?) {}
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                filesystem.distributionType = parent?.getItemAtPosition(position).toString()
            }
        }
    }

    private fun setupCredentialInputs() {
        binding.textInputUsername.setText(filesystem.defaultUsername)
        binding.textInputUsername.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(p0: Editable?) { filesystem.defaultUsername = p0.toString() }
            override fun beforeTextChanged(p0: CharSequence?, p1: Int, p2: Int, p3: Int) {}
            override fun onTextChanged(p0: CharSequence?, p1: Int, p2: Int, p3: Int) {}
        })

        binding.textInputPassword.setText(filesystem.defaultPassword)
        binding.textInputPassword.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(p0: Editable?) { filesystem.defaultPassword = p0.toString() }
            override fun beforeTextChanged(p0: CharSequence?, p1: Int, p2: Int, p3: Int) {}
            override fun onTextChanged(p0: CharSequence?, p1: Int, p2: Int, p3: Int) {}
        })

        binding.textInputVncPassword.setText(filesystem.defaultVncPassword)
        binding.textInputVncPassword.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(p0: Editable?) { filesystem.defaultVncPassword = p0.toString() }
            override fun beforeTextChanged(p0: CharSequence?, p1: Int, p2: Int, p3: Int) {}
            override fun onTextChanged(p0: CharSequence?, p1: Int, p2: Int, p3: Int) {}
        })
    }

    private fun handleViewStateChange(viewState: FilesystemEditViewState) {
        when (viewState) {
            is FilesystemEditViewState.Success -> {
                NavHostFragment.findNavController(this).popBackStack()
            }
            is FilesystemEditViewState.ImportSuccess -> {
                NavHostFragment.findNavController(this).popBackStack()
            }
            else -> {}
        }
    }

    private fun insertFilesystem() {
        if (filesystem.name.isEmpty()) {
            binding.textInputFilesystemName.error = getString(R.string.error_filesystem_name)
            return
        }
        if (filesystem.defaultUsername.isEmpty()) {
            binding.textInputUsername.error = getString(R.string.error_empty_field)
            return
        }
        if (filesystem.defaultPassword.isEmpty()) {
            binding.textInputPassword.error = getString(R.string.error_empty_field)
            return
        }
        if (filesystem.defaultVncPassword.isEmpty()) {
            binding.textInputVncPassword.error = getString(R.string.error_empty_field)
            return
        }

        if (editExisting) filesystemEditViewModel.updateFilesystem(filesystem)
        else filesystemEditViewModel.insertFilesystem(filesystem)
    }
}
