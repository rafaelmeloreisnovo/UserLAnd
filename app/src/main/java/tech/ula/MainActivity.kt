package tech.ula

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import tech.ula.databinding.ActivityMainBinding

/**
 * Minimal stub MainActivity for CI builds.
 * TODO: Full ViewBinding migration required - the original code references
 * kotlin-android-extensions synthetics which are no longer supported in Kotlin 1.8+.
 */
class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        setSupportActionBar(binding.toolbar)
    }
}
