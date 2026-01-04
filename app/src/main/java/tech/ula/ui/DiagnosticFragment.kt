package tech.ula.ui

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.core.content.FileProvider
import androidx.fragment.app.Fragment
import tech.ula.R
import tech.ula.databinding.FragDiagnosticBinding
import tech.ula.utils.DiagnosticChecker
import tech.ula.utils.PermissionHandler
import java.io.File

/**
 * DiagnosticFragment - System diagnostics and troubleshooting UI
 * 
 * Provides users with:
 * - System information and compatibility checks
 * - Permission status
 * - Native library verification
 * - Export diagnostic report (without PII)
 */
class DiagnosticFragment : Fragment() {
    
    private var _binding: FragDiagnosticBinding? = null
    private val binding get() = _binding!!
    
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        _binding = FragDiagnosticBinding.inflate(inflater, container, false)
        return binding.root
    }
    
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        
        binding.btnRunDiagnostics.setOnClickListener {
            runDiagnostics()
        }
        
        binding.btnCopyReport.setOnClickListener {
            copyReportToClipboard()
        }
        
        binding.btnShareReport.setOnClickListener {
            shareReport()
        }
        
        binding.btnRequestNotificationPermission.setOnClickListener {
            PermissionHandler.requestNotificationPermission(requireActivity())
        }
        
        // Auto-run diagnostics on load
        runDiagnostics()
    }
    
    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
    
    private fun runDiagnostics() {
        binding.progressBar.visibility = View.VISIBLE
        binding.btnCopyReport.isEnabled = false
        binding.btnShareReport.isEnabled = false
        
        try {
            val checker = DiagnosticChecker(requireContext())
            val diagnosticInfo = checker.performDiagnostics()
            val report = diagnosticInfo.toReport()
            
            binding.tvDiagnosticReport.text = report
            binding.btnCopyReport.isEnabled = true
            binding.btnShareReport.isEnabled = true
            
            // Show notification permission button if needed
            if (diagnosticInfo.notificationPermissionStatus == tech.ula.utils.PermissionStatus.DENIED) {
                binding.btnRequestNotificationPermission.visibility = View.VISIBLE
            } else {
                binding.btnRequestNotificationPermission.visibility = View.GONE
            }
            
        } catch (e: Exception) {
            binding.tvDiagnosticReport.text = getString(R.string.diagnostic_error, e.message)
        } finally {
            binding.progressBar.visibility = View.GONE
        }
    }
    
    private fun copyReportToClipboard() {
        val report = binding.tvDiagnosticReport.text.toString()
        val clipboard = requireContext().getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val clip = ClipData.newPlainText("UserLAnd Diagnostic Report", report)
        clipboard.setPrimaryClip(clip)
        Toast.makeText(requireContext(), R.string.diagnostic_copied, Toast.LENGTH_SHORT).show()
    }
    
    private fun shareReport() {
        try {
            val report = binding.tvDiagnosticReport.text.toString()
            
            // Create a temporary file for sharing
            val cacheDir = requireContext().cacheDir
            val reportFile = File(cacheDir, "userland_diagnostic_report.txt")
            reportFile.writeText(report)
            
            // Use FileProvider for secure file sharing
            val fileUri = FileProvider.getUriForFile(
                requireContext(),
                "${requireContext().packageName}.fileprovider",
                reportFile,
            )
            
            val shareIntent = Intent(Intent.ACTION_SEND).apply {
                type = "text/plain"
                putExtra(Intent.EXTRA_STREAM, fileUri)
                putExtra(Intent.EXTRA_SUBJECT, "UserLAnd Diagnostic Report")
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
            
            startActivity(Intent.createChooser(shareIntent, getString(R.string.diagnostic_share_title)))
        } catch (e: Exception) {
            Toast.makeText(requireContext(), getString(R.string.diagnostic_share_error, e.message), Toast.LENGTH_LONG).show()
        }
    }
}
