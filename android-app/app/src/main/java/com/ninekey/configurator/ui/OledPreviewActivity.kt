package com.ninekey.configurator.ui

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.View
import android.widget.EditText
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.ninekey.configurator.R
import com.ninekey.configurator.model.KeyAssignment
import com.ninekey.configurator.model.KeycodeCatalog
import com.ninekey.configurator.model.ProfileRepository

/**
 * Activity for viewing and editing OLED labels and descriptions for all 9 keys of a layer.
 * Also shows a simple ASCII-art OLED preview of the current legend.
 */
class OledPreviewActivity : AppCompatActivity() {

    private lateinit var catalog: KeycodeCatalog
    private lateinit var repo: ProfileRepository

    private var layerId: Int = 0

    /** Holds one [EditText] per key for the OLED label field. */
    private val oledInputs = mutableListOf<EditText>()
    /** Holds one [EditText] per key for the description field. */
    private val descInputs = mutableListOf<EditText>()
    /** Header labels showing the key's current keycode. */
    private val keyLabels = mutableListOf<TextView>()

    private lateinit var previewText: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_oled_preview)

        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        layerId = intent.getIntExtra(EXTRA_LAYER_ID, 0)
        catalog = KeycodeCatalog.load(this)
        repo = ProfileRepository(this)

        val layerName = catalog.layerById(layerId)?.displayName ?: "Layer $layerId"
        title = "OLED Labels – $layerName"

        previewText = findViewById(R.id.oledPreviewText)

        // Bind per-key views
        val keyViewIds = listOf(
            Triple(R.id.oledKeyLabel0, R.id.oledInput0, R.id.oledDesc0),
            Triple(R.id.oledKeyLabel1, R.id.oledInput1, R.id.oledDesc1),
            Triple(R.id.oledKeyLabel2, R.id.oledInput2, R.id.oledDesc2),
            Triple(R.id.oledKeyLabel3, R.id.oledInput3, R.id.oledDesc3),
            Triple(R.id.oledKeyLabel4, R.id.oledInput4, R.id.oledDesc4),
            Triple(R.id.oledKeyLabel5, R.id.oledInput5, R.id.oledDesc5),
            Triple(R.id.oledKeyLabel6, R.id.oledInput6, R.id.oledDesc6),
            Triple(R.id.oledKeyLabel7, R.id.oledInput7, R.id.oledDesc7),
            Triple(R.id.oledKeyLabel8, R.id.oledInput8, R.id.oledDesc8)
        )

        val profile = repo.loadProfile()
        val layerProfile = profile.layers.firstOrNull { it.id == layerId }
        val layerInfo = catalog.layerById(layerId)

        for ((i, ids) in keyViewIds.withIndex()) {
            val header = findViewById<TextView>(ids.first)
            val oledInput = findViewById<EditText>(ids.second)
            val descInput = findViewById<EditText>(ids.third)

            keyLabels.add(header)
            oledInputs.add(oledInput)
            descInputs.add(descInput)

            val assignment = layerProfile?.keys?.getOrNull(i) ?: KeyAssignment()
            val firmwareLegend = layerInfo?.keyLegends?.getOrNull(i) ?: ""

            header.text = "Key ${i + 1}: ${assignment.label}  (firmware: $firmwareLegend)"
            oledInput.setText(assignment.oledLabel.ifBlank { firmwareLegend.take(5) })
            descInput.setText(
                assignment.description.ifBlank {
                    layerInfo?.keyFunctions?.getOrNull(i) ?: ""
                }
            )

            // Update OLED preview whenever any field changes
            val watcher = object : TextWatcher {
                override fun beforeTextChanged(s: CharSequence?, st: Int, c: Int, a: Int) {}
                override fun onTextChanged(s: CharSequence?, st: Int, c: Int, a: Int) = updatePreview()
                override fun afterTextChanged(e: Editable?) {}
            }
            oledInput.addTextChangedListener(watcher)
        }

        updatePreview()

        findViewById<View>(R.id.oledSaveButton).setOnClickListener { saveAndFinish() }
        findViewById<View>(R.id.oledResetButton).setOnClickListener { resetToFirmware() }
    }

    override fun onSupportNavigateUp(): Boolean {
        finish()
        return true
    }

    private fun updatePreview() {
        val labels = oledInputs.map { it.text.toString().take(5).padEnd(5) }
        val row0 = "${labels[0]}|${labels[1]}|${labels[2]}"
        val row1 = "${labels[3]}|${labels[4]}|${labels[5]}"
        val row2 = "${labels[6]}|${labels[7]}|${labels[8]}"
        val sep = "-----+-----+-----"
        previewText.text = buildString {
            append("OLED Preview\n")
            append("┌─────┬─────┬─────┐\n")
            append("│${labels[0]}│${labels[1]}│${labels[2]}│\n")
            append("├─────┼─────┼─────┤\n")
            append("│${labels[3]}│${labels[4]}│${labels[5]}│\n")
            append("├─────┼─────┼─────┤\n")
            append("│${labels[6]}│${labels[7]}│${labels[8]}│\n")
            append("└─────┴─────┴─────┘")
        }
    }

    private fun saveAndFinish() {
        var profile = repo.loadProfile()
        val layerProfile = profile.layers.firstOrNull { it.id == layerId } ?: return

        val updatedKeys = layerProfile.keys.copyOf()
        for (i in updatedKeys.indices) {
            if (i < oledInputs.size) {
                updatedKeys[i] = updatedKeys[i].copy(
                    oledLabel = oledInputs[i].text.toString().trim().take(5),
                    description = descInputs[i].text.toString().trim()
                )
            }
        }
        val updatedLayer = layerProfile.copy(keys = updatedKeys)
        val updatedLayers = profile.layers.map { if (it.id == layerId) updatedLayer else it }
        profile = profile.copy(layers = updatedLayers)
        repo.saveProfile(profile)
        setResult(RESULT_OK)
        finish()
    }

    private fun resetToFirmware() {
        val layerInfo = catalog.layerById(layerId) ?: return
        for (i in oledInputs.indices) {
            oledInputs[i].setText(layerInfo.keyLegends.getOrNull(i)?.take(5) ?: "")
            descInputs[i].setText(layerInfo.keyFunctions.getOrNull(i) ?: "")
        }
        updatePreview()
    }

    companion object {
        const val EXTRA_LAYER_ID = "layer_id"

        fun start(context: Context, layerId: Int) {
            context.startActivity(
                Intent(context, OledPreviewActivity::class.java)
                    .putExtra(EXTRA_LAYER_ID, layerId)
            )
        }
    }
}
