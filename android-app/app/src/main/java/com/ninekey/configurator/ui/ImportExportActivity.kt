package com.ninekey.configurator.ui

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.View
import android.widget.EditText
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.ninekey.configurator.R
import com.ninekey.configurator.model.AppProfile
import com.ninekey.configurator.model.ProfileRepository

/**
 * Activity for importing and exporting the profile as JSON.
 * Users can copy the JSON text or paste it to restore a saved profile.
 */
class ImportExportActivity : AppCompatActivity() {

    private lateinit var repo: ProfileRepository
    private lateinit var jsonEdit: EditText
    private lateinit var statusText: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_import_export)

        supportActionBar?.setDisplayHomeAsUpEnabled(true)
        title = "Import / Export Profile"

        repo = ProfileRepository(this)
        jsonEdit = findViewById(R.id.importExportJson)
        statusText = findViewById(R.id.importExportStatus)

        val profile = repo.loadProfile()
        jsonEdit.setText(repo.exportToJson(profile))

        findViewById<View>(R.id.exportButton).setOnClickListener { doExport() }
        findViewById<View>(R.id.importButton).setOnClickListener { doImport() }
        findViewById<View>(R.id.resetButton).setOnClickListener { doReset() }
    }

    override fun onSupportNavigateUp(): Boolean {
        finish()
        return true
    }

    private fun doExport() {
        val profile = repo.loadProfile()
        val json = repo.exportToJson(profile)
        jsonEdit.setText(json)
        statusText.text = "Profile exported – copy the JSON above to save it."
    }

    private fun doImport() {
        val text = jsonEdit.text.toString()
        try {
            val profile: AppProfile = repo.importFromJson(text)
            repo.saveProfile(profile)
            statusText.text = "Profile imported and saved (${profile.layers.size} layers)."
        } catch (e: Exception) {
            statusText.text = "Import failed: ${e.message}"
        }
    }

    private fun doReset() {
        val defaults = repo.loadDefault()
        repo.saveProfile(defaults)
        jsonEdit.setText(repo.exportToJson(defaults))
        statusText.text = "Profile reset to firmware defaults."
    }

    companion object {
        fun start(context: Context) {
            context.startActivity(Intent(context, ImportExportActivity::class.java))
        }
    }
}
