package tech.ula.ui

import android.app.Activity
import androidx.lifecycle.Observer
import androidx.lifecycle.ViewModelProvider
import android.graphics.Color
import android.os.Bundle
import androidx.fragment.app.Fragment
import android.text.Editable
import android.text.TextWatcher
import android.view.* // ktlint-disable no-wildcard-imports
import android.widget.* // ktlint-disable no-wildcard-imports
import androidx.core.os.bundleOf
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import tech.ula.R
import tech.ula.databinding.FragSessionEditBinding
import tech.ula.model.entities.Filesystem
import tech.ula.model.entities.ServiceType
import tech.ula.model.entities.Session
import tech.ula.model.entities.toServiceType
import tech.ula.model.repositories.UlaDatabase
import tech.ula.viewmodel.SessionEditViewModel
import tech.ula.viewmodel.SessionEditViewmodelFactory

class SessionEditFragment : Fragment() {

    private var _binding: FragSessionEditBinding? = null
    private val binding get() = _binding!!

    private lateinit var activityContext: Activity
    private val args: SessionEditFragmentArgs by navArgs()
    private val session: Session by lazy { args.session!! }
    private val editExisting by lazy { args.editExisting }

    private var filesystemList: List<Filesystem> = emptyList()

    private val sessionEditViewModel: SessionEditViewModel by lazy {
        val ulaDatabase = UlaDatabase.getInstance(activityContext)
        ViewModelProvider(this, SessionEditViewmodelFactory(ulaDatabase)).get(SessionEditViewModel::class.java)
    }

    private val filesystemChangeObserver = Observer<List<Filesystem>> {
        it?.let { newList ->
            val spinnerList = augmentFilesystemList(newList)
            getListDifferenceAndSetNewFilesystem(filesystemList, it)
            val filesystemAdapter = ArrayAdapter(activityContext, android.R.layout.simple_spinner_dropdown_item, spinnerList)
            val filesystemNamePosition = it.indexOfFirst { filesystem -> filesystem.id == session.filesystemId }
            val usedPosition = if (filesystemNamePosition < 0) 0 else filesystemNamePosition
            binding.spinnerFilesystemList.adapter = filesystemAdapter
            binding.spinnerFilesystemList.setSelection(usedPosition)
            filesystemList = it
        }
    }

    internal sealed class FilesystemDropdownItem {
        data class NonFilesystemItem(val text: String) : FilesystemDropdownItem() {
            override fun toString(): String = text
        }
        data class FilesystemItem(val filesystem: Filesystem) : FilesystemDropdownItem() {
            override fun toString(): String = "${filesystem.name}: ${filesystem.distributionType.replaceFirstChar { it.uppercase() }}"
        }
    }

    private fun augmentFilesystemList(filesystems: List<Filesystem>): List<FilesystemDropdownItem> {
        val listBuilder = mutableListOf<FilesystemDropdownItem>()
        if (filesystems.isEmpty()) listBuilder.add(FilesystemDropdownItem.NonFilesystemItem(""))
        listBuilder.addAll(filesystems.map { FilesystemDropdownItem.FilesystemItem(it) })
        listBuilder.add(FilesystemDropdownItem.NonFilesystemItem("Create new"))
        return listBuilder.toList()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setHasOptionsMenu(true)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        _binding = FragSessionEditBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onCreateOptionsMenu(menu: Menu, inflater: MenuInflater) {
        super.onCreateOptionsMenu(menu, inflater)
        inflater.inflate(R.menu.menu_edit, menu)
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return if (item.itemId == R.id.menu_item_add) {
            insertSession()
            true
        } else super.onOptionsItemSelected(item)
    }

    @Suppress("DEPRECATION")
    override fun onActivityCreated(savedInstanceState: Bundle?) {
        super.onActivityCreated(savedInstanceState)
        activityContext = activity!!
        sessionEditViewModel.getAllFilesystems().observe(viewLifecycleOwner, filesystemChangeObserver)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        binding.textInputSessionName.setText(session.name)
        if (session.isAppsSession) {
            binding.textInputSessionName.isEnabled = false
        }
        binding.textInputSessionName.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(p0: Editable?) { session.name = p0.toString() }
            override fun beforeTextChanged(p0: CharSequence?, p1: Int, p2: Int, p3: Int) {}
            override fun onTextChanged(p0: CharSequence?, p1: Int, p2: Int, p3: Int) {}
        })

        binding.spinnerFilesystemList.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onNothingSelected(parent: AdapterView<*>?) {}
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                parent?.let {
                    when (val item = parent.getItemAtPosition(position) as FilesystemDropdownItem) {
                        is FilesystemDropdownItem.NonFilesystemItem -> {
                            if (item.text == "Create new") {
                                val bundle = bundleOf("filesystem" to Filesystem(0), "editExisting" to false)
                                this@SessionEditFragment.findNavController().navigate(R.id.filesystem_edit_fragment, bundle)
                            }
                        }
                        is FilesystemDropdownItem.FilesystemItem -> {
                            updateFilesystemDetailsForSession(item.filesystem)
                            binding.textInputUsername.setText(item.filesystem.defaultUsername)
                        }
                    }
                }
            }
        }

        binding.spinnerSessionServiceType.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onNothingSelected(parent: AdapterView<*>?) {}
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                session.serviceType = parent?.getItemAtPosition(position).toString().toServiceType()
            }
        }

        binding.textInputUsername.isEnabled = false
        binding.textInputUsername.addTextChangedListener(object : TextWatcher {
            override fun afterTextChanged(p0: Editable?) { session.username = p0.toString() }
            override fun beforeTextChanged(p0: CharSequence?, p1: Int, p2: Int, p3: Int) {}
            override fun onTextChanged(p0: CharSequence?, p1: Int, p2: Int, p3: Int) {}
        })
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun insertSession() {
        val navController = NavHostFragment.findNavController(this)
        if (session.name == "") binding.textInputSessionName.error = getString(R.string.error_session_name)
        if (session.filesystemName == "") {
            val errorText = binding.spinnerFilesystemList.selectedView as TextView
            errorText.error = ""
            errorText.setTextColor(Color.RED)
            errorText.text = getString(R.string.error_filesystem_name)
        }
        if (session.name == "" || session.username == "" || session.filesystemName == "") {
            Toast.makeText(activityContext, R.string.error_empty_field, Toast.LENGTH_LONG).show()
            return
        }
        if (editExisting) sessionEditViewModel.updateSession(session)
        else sessionEditViewModel.insertSession(session)
        navController.popBackStack()
    }

    private fun getListDifferenceAndSetNewFilesystem(prevFilesystems: List<Filesystem>, currentFilesystems: List<Filesystem>) {
        val uniqueFilesystems = currentFilesystems.subtract(prevFilesystems.toSet())
        if (prevFilesystems.isNotEmpty() && uniqueFilesystems.isNotEmpty()) {
            updateFilesystemDetailsForSession(uniqueFilesystems.first())
        }
    }

    private fun updateFilesystemDetailsForSession(filesystem: Filesystem) {
        session.filesystemName = filesystem.name
        session.username = filesystem.defaultUsername
        session.password = filesystem.defaultPassword
        session.vncPassword = filesystem.defaultVncPassword
        session.filesystemId = filesystem.id
    }
}