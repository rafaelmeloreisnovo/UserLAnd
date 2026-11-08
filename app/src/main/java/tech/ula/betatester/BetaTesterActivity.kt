package tech.ula.betatester

import android.app.ActivityManager
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.FileProvider
import tech.ula.BuildConfig
import org.json.JSONObject
import java.io.File
import java.util.Random
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Opt-in, debug/beta-only, one-tap bounded diagnostics. No guest or production
 * session is launched, no network requests are performed, and no raw PII is logged.
 * Report receipts are local and untrusted until verified on the physical device.
 */
class BetaTesterActivity : AppCompatActivity() {
    private data class Scenario(val id: String, val op: String, val critical: Boolean)
    private data class Outcome(val state: String, val evidence: String)
    private lateinit var status: TextView
    private lateinit var start: Button
    private lateinit var share: Button
    private val running = AtomicBoolean(false)
    private var lastReport: File? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (BuildConfig.BUILD_TYPE != "debug" && BuildConfig.BUILD_TYPE != "beta") {
            finish()
            return
        }
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(24, 24, 24, 24)
        }
        start = Button(this).apply {
            text = "Executar bateria sequencial"
            setOnClickListener { runBattery() }
        }
        share = Button(this).apply {
            text = "Compartilhar receipt"
            isEnabled = false
            setOnClickListener { shareReceipt() }
        }
        status = TextView(this).apply {
            text = "UserLAnd BetaTester\nSem testes executados.\nSomente sandbox e leituras de ambiente."
            textSize = 15f
        }
        root.addView(start)
        root.addView(share)
        root.addView(ScrollView(this).apply { addView(status) })
        setContentView(root)
    }

    private fun loadScenarios(): List<Scenario> {
        val lines = assets.open("betatester/scenarios.csv").bufferedReader().use { it.readLines() }
        require(lines.firstOrNull() == "id;operation;critical") { "Invalid scenario schema" }
        val result = lines.drop(1).filter { it.isNotBlank() && !it.startsWith("#") }.map {
            val parts = it.split(';')
            require(parts.size == 3 && parts[0].matches(Regex("[A-Z0-9_]{2,32}")) &&
                parts[2] in listOf("true", "false")) { "Invalid scenario row" }
            Scenario(parts[0], parts[1], parts[2] == "true")
        }
        require(result.size in 1..32 && result.map { it.id }.distinct().size == result.size) {
            "Scenario count/IDs invalid"
        }
        return result
    }

    private fun runBattery() {
        if (!running.compareAndSet(false, true)) return
        start.isEnabled = false
        share.isEnabled = false
        val seed = System.currentTimeMillis() xor (Build.VERSION.SDK_INT.toLong() shl 32)
        status.text = "Bateria iniciada; seed=" + seed + "\n"
        Thread {
            var writer: java.io.BufferedWriter? = null
            try {
                val scenarios = loadScenarios()
                val file = File(filesDir, "betatester-" + System.currentTimeMillis() + ".jsonl")
                writer = file.bufferedWriter()
                val header = JSONObject().put("type", "header").put("schema", 1)
                    .put("build_type", BuildConfig.BUILD_TYPE)
                    .put("version", BuildConfig.VERSION_NAME)
                    .put("android_sdk", Build.VERSION.SDK_INT)
                    .put("model", Build.MODEL.take(80))
                    .put("abi", Build.SUPPORTED_ABIS.firstOrNull() ?: "UNKNOWN")
                    .put("seed", seed)
                    .put("scope", "sandbox_and_read_only_preflight")
                writer.write(header.toString() + "\n")
                var pass = 0
                var fail = 0
                var blocked = 0
                scenarios.forEachIndexed { index, scenario ->
                    val t0 = System.nanoTime()
                    val result = try {
                        execute(scenario, seed)
                    } catch (t: Throwable) {
                        Outcome("FAIL", "EXCEPTION_" + t.javaClass.simpleName.take(60))
                    }
                    when (result.state) {
                        "PASS" -> pass++
                        "FAIL" -> fail++
                        else -> blocked++
                    }
                    val elapsed = (System.nanoTime() - t0) / 1000000L
                    val event = JSONObject().put("type", "case")
                        .put("index", index).put("case_id", scenario.id)
                        .put("operation", scenario.op).put("critical", scenario.critical)
                        .put("state", result.state).put("evidence", result.evidence)
                        .put("elapsed_ms", elapsed).put("seed", seed)
                    writer.write(event.toString() + "\n")
                    writer.flush()
                    runOnUiThread {
                        status.append(scenario.id + " = " + result.state + " (" + result.evidence + ")\n")
                    }
                }
                writer.write(JSONObject().put("type", "summary").put("pass", pass)
                    .put("fail", fail).put("blocked", blocked)
                    .put("physical_runtime", "UNVERIFIED_UNTIL_OPERATOR_REVIEW")
                    .put("engine_e2e", "NOT_RUN").toString() + "\n")
                writer.flush()
                lastReport = file
                runOnUiThread {
                    status.append("\nPASS=" + pass + " FAIL=" + fail + " BLOCKED=" + blocked +
                        "\nReceipt interno: " + file.name + "\nSem claim E2E.")
                    share.isEnabled = true
                }
            } catch (t: Throwable) {
                runOnUiThread { status.append("\nSUITE_BLOCKED: " + t.javaClass.simpleName + "\n") }
            } finally {
                try { writer?.close() } catch (_: Exception) {}
                running.set(false)
                runOnUiThread { start.isEnabled = true }
            }
        }.start()
    }

    private fun execute(s: Scenario, seed: Long): Outcome {
        return when (s.op) {
            "abi" -> {
                val abis = Build.SUPPORTED_ABIS
                if (abis.isNotEmpty() && abis[0].isNotBlank()) Outcome("PASS", "ABI_PRESENT")
                else Outcome("FAIL", "ABI_EMPTY")
            }
            "memory" -> {
                val am = getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
                val info = ActivityManager.MemoryInfo()
                am.getMemoryInfo(info)
                if (info.availMem > 0 && info.totalMem >= info.availMem)
                    Outcome("PASS", "MEMORY_COUNTERS_VALID")
                else Outcome("FAIL", "MEMORY_COUNTERS_INVALID")
            }
            "package" -> {
                val actual = packageManager.getPackageInfo(packageName, 0).packageName
                if (actual == packageName) Outcome("PASS", "PACKAGE_MATCH")
                else Outcome("FAIL", "PACKAGE_MISMATCH")
            }
            "native_proot" -> {
                val libs = File(applicationInfo.nativeLibraryDir).listFiles()
                if (libs == null) Outcome("BLOCKED", "NATIVE_LIB_DIR_UNREADABLE")
                else if (libs.any { it.name.startsWith("lib_proot") && it.name.endsWith(".so") })
                    Outcome("PASS", "PROOT_LIBRARY_PRESENT_NOT_EXECUTED")
                else Outcome("BLOCKED", "PROOT_LIBRARY_NOT_FOUND")
            }
            "sandbox_io" -> {
                val f = File(cacheDir, "betatester-scratch.tmp")
                val bytes = byteArrayOf(0, 1, 3, 7, 15)
                f.writeBytes(bytes)
                val equal = f.readBytes().contentEquals(bytes)
                if (!f.delete()) Outcome("FAIL", "SCRATCH_CLEANUP_FAIL")
                else if (equal) Outcome("PASS", "SANDBOX_ROUNDTRIP")
                else Outcome("FAIL", "SANDBOX_BYTES_MISMATCH")
            }
            "bundle_state" -> {
                val b = Bundle()
                b.putInt("phase", 1)
                b.putInt("phase", 2)
                if (b.getInt("phase") == 2) Outcome("PASS", "FRAMEWORK_STATE_TRANSITION")
                else Outcome("FAIL", "FRAMEWORK_TRANSITION_MISMATCH")
            }
            "permutations" -> {
                // Bounded seeded paths: 24 attempts, not an exhaustive app event graph.
                val random = Random(seed)
                val observed = HashSet<String>()
                repeat(24) {
                    val order = mutableListOf("a", "b", "c")
                    order.shuffle(random)
                    val b = Bundle()
                    order.forEachIndexed { i, key -> b.putInt(key, i) }
                    order.forEachIndexed { i, key ->
                        if (b.getInt(key, -1) != i) return Outcome("FAIL", "REPLAY_MISMATCH")
                    }
                    observed.add(order.joinToString(""))
                }
                Outcome("PASS", "SAMPLED_24_DISTINCT_" + observed.size)
            }
            "guest_e2e" -> Outcome("BLOCKED", "GUEST_NOT_STARTED_NO_CONSENT")
            else -> Outcome("FAIL", "UNKNOWN_OPERATION")
        }
    }

    private fun shareReceipt() {
        val report = lastReport ?: return
        try {
            // FileProvider is deliberately configured for cache-path only.
            val shareCopy = File(cacheDir, "betatester-share.jsonl")
            report.copyTo(shareCopy, overwrite = true)
            val uri = FileProvider.getUriForFile(this, packageName + ".fileprovider", shareCopy)
            val intent = Intent(Intent.ACTION_SEND).apply {
                type = "application/x-ndjson"
                putExtra(Intent.EXTRA_STREAM, uri)
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
            startActivity(Intent.createChooser(intent, "Compartilhar receipt local"))
        } catch (t: Exception) {
            status.append("\nEXPORT_BLOCKED: " + t.javaClass.simpleName)
        }
    }
}
