package com.ninekey.configurator.ui

import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.os.Bundle
import android.view.View
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.SeekBar
import android.widget.Spinner
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.ninekey.configurator.R
import com.ninekey.configurator.model.KeycodeCatalog
import com.ninekey.configurator.model.LedEffect
import com.ninekey.configurator.model.LedLayerSettings
import com.ninekey.configurator.model.LedZoneSettings
import com.ninekey.configurator.model.ProfileRepository

/**
 * Activity for editing per-layer LED settings (key LEDs, gap LEDs, frame LEDs).
 * Supports hue, saturation, brightness, effect, and speed for all three zones.
 * Settings are persisted via [ProfileRepository].
 */
class LedSettingsActivity : AppCompatActivity() {

    private lateinit var catalog: KeycodeCatalog
    private lateinit var repo: ProfileRepository

    private var layerId: Int = 0
    private lateinit var effects: List<LedEffect>
    private var currentSettings: LedLayerSettings = LedLayerSettings()

    // References to zone views (key / gap / frame)
    private data class ZoneViews(
        val label: TextView,
        val preview: View,
        val effectSpinner: Spinner,
        val hueBar: SeekBar, val hueVal: TextView,
        val satBar: SeekBar, val satVal: TextView,
        val valBar: SeekBar, val valValText: TextView,
        val speedBar: SeekBar, val speedVal: TextView
    )

    private lateinit var keyZone: ZoneViews
    private lateinit var gapZone: ZoneViews
    private lateinit var frameZone: ZoneViews

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_led_settings)

        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        layerId = intent.getIntExtra(EXTRA_LAYER_ID, 0)
        catalog = KeycodeCatalog.load(this)
        repo = ProfileRepository(this)
        effects = catalog.effects

        val layerName = catalog.layerById(layerId)?.displayName ?: "Layer $layerId"
        title = "LED Settings – $layerName"

        val profile = repo.loadProfile()
        val layerProfile = profile.layers.firstOrNull { it.id == layerId }
        currentSettings = layerProfile?.led ?: LedLayerSettings()

        keyZone = bindZone(
            label = R.id.ledKeyLabel,
            preview = R.id.ledKeyPreview,
            effectSpinner = R.id.ledKeyEffect,
            hueBar = R.id.ledKeyHue, hueVal = R.id.ledKeyHueVal,
            satBar = R.id.ledKeySat, satVal = R.id.ledKeySatVal,
            valBar = R.id.ledKeyVal, valValText = R.id.ledKeyValVal,
            speedBar = R.id.ledKeySpeed, speedVal = R.id.ledKeySpeedVal
        )
        gapZone = bindZone(
            label = R.id.ledGapLabel,
            preview = R.id.ledGapPreview,
            effectSpinner = R.id.ledGapEffect,
            hueBar = R.id.ledGapHue, hueVal = R.id.ledGapHueVal,
            satBar = R.id.ledGapSat, satVal = R.id.ledGapSatVal,
            valBar = R.id.ledGapVal, valValText = R.id.ledGapValVal,
            speedBar = R.id.ledGapSpeed, speedVal = R.id.ledGapSpeedVal
        )
        frameZone = bindZone(
            label = R.id.ledFrameLabel,
            preview = R.id.ledFramePreview,
            effectSpinner = R.id.ledFrameEffect,
            hueBar = R.id.ledFrameHue, hueVal = R.id.ledFrameHueVal,
            satBar = R.id.ledFrameSat, satVal = R.id.ledFrameSatVal,
            valBar = R.id.ledFrameVal, valValText = R.id.ledFrameValVal,
            speedBar = R.id.ledFrameSpeed, speedVal = R.id.ledFrameSpeedVal
        )

        populateZone(keyZone, "Key LEDs", currentSettings.key)
        populateZone(gapZone, "Gap LEDs", currentSettings.gap)
        populateZone(frameZone, "Frame LEDs", currentSettings.frame)

        setupZoneListeners(keyZone) { updateKey() }
        setupZoneListeners(gapZone) { updateGap() }
        setupZoneListeners(frameZone) { updateFrame() }

        findViewById<View>(R.id.ledSaveButton).setOnClickListener { saveAndFinish() }
        findViewById<View>(R.id.ledResetButton).setOnClickListener { resetToDefaults() }
    }

    override fun onSupportNavigateUp(): Boolean {
        finish()
        return true
    }

    // ── Zone binding helpers ──────────────────────────────────────────────────

    private fun bindZone(
        label: Int, preview: Int,
        effectSpinner: Int,
        hueBar: Int, hueVal: Int,
        satBar: Int, satVal: Int,
        valBar: Int, valValText: Int,
        speedBar: Int, speedVal: Int
    ) = ZoneViews(
        label = findViewById(label),
        preview = findViewById(preview),
        effectSpinner = findViewById(effectSpinner),
        hueBar = findViewById(hueBar), hueVal = findViewById(hueVal),
        satBar = findViewById(satBar), satVal = findViewById(satVal),
        valBar = findViewById(valBar), valValText = findViewById(valValText),
        speedBar = findViewById(speedBar), speedVal = findViewById(speedVal)
    )

    private fun populateZone(zone: ZoneViews, title: String, settings: LedZoneSettings) {
        zone.label.text = title

        val effectNames = effects.map { it.label }
        val adapter = ArrayAdapter(this, android.R.layout.simple_spinner_item, effectNames)
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        zone.effectSpinner.adapter = adapter
        zone.effectSpinner.setSelection(
            settings.effect.coerceIn(0, effects.size - 1)
        )

        zone.hueBar.max = 255
        zone.hueBar.progress = settings.hue
        zone.hueVal.text = settings.hue.toString()

        zone.satBar.max = 255
        zone.satBar.progress = settings.sat
        zone.satVal.text = settings.sat.toString()

        zone.valBar.max = 255
        zone.valBar.progress = settings.value
        zone.valValText.text = settings.value.toString()

        zone.speedBar.max = 255
        zone.speedBar.progress = settings.speed
        zone.speedVal.text = settings.speed.toString()

        updatePreview(zone, settings)
    }

    private fun setupZoneListeners(zone: ZoneViews, onChange: () -> Unit) {
        val seekListener = object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(bar: SeekBar?, progress: Int, fromUser: Boolean) {
                zone.hueVal.text = zone.hueBar.progress.toString()
                zone.satVal.text = zone.satBar.progress.toString()
                zone.valValText.text = zone.valBar.progress.toString()
                zone.speedVal.text = zone.speedBar.progress.toString()
                updatePreview(zone, readZone(zone))
                onChange()
            }
            override fun onStartTrackingTouch(bar: SeekBar?) {}
            override fun onStopTrackingTouch(bar: SeekBar?) {}
        }
        zone.hueBar.setOnSeekBarChangeListener(seekListener)
        zone.satBar.setOnSeekBarChangeListener(seekListener)
        zone.valBar.setOnSeekBarChangeListener(seekListener)
        zone.speedBar.setOnSeekBarChangeListener(seekListener)
        zone.effectSpinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(p: AdapterView<*>?, v: View?, pos: Int, id: Long) = onChange()
            override fun onNothingSelected(p: AdapterView<*>?) {}
        }
    }

    private fun readZone(zone: ZoneViews) = LedZoneSettings(
        effect = zone.effectSpinner.selectedItemPosition,
        speed = zone.speedBar.progress,
        hue = zone.hueBar.progress,
        sat = zone.satBar.progress,
        value = zone.valBar.progress
    )

    private fun updatePreview(zone: ZoneViews, s: LedZoneSettings) {
        val h = s.hue / 255f * 360f
        val sat = s.sat / 255f
        val value = s.value / 255f
        zone.preview.setBackgroundColor(Color.HSVToColor(floatArrayOf(h, sat, value)))
    }

    private fun updateKey() {
        currentSettings = currentSettings.copy(key = readZone(keyZone))
    }

    private fun updateGap() {
        currentSettings = currentSettings.copy(gap = readZone(gapZone))
    }

    private fun updateFrame() {
        currentSettings = currentSettings.copy(frame = readZone(frameZone))
    }

    private fun saveAndFinish() {
        updateKey(); updateGap(); updateFrame()
        val profile = repo.loadProfile()
        repo.saveProfile(repo.updateLed(profile, layerId, currentSettings))
        setResult(RESULT_OK)
        finish()
    }

    private fun resetToDefaults() {
        val presets = repo.loadLedPresets()
        val layerInfo = catalog.layerById(layerId)
        val viaSlot = layerInfo?.viaSlot ?: -1
        val defaults = if (viaSlot in 0 until presets.size) presets[viaSlot] else LedLayerSettings()
        currentSettings = defaults
        populateZone(keyZone, "Key LEDs", defaults.key)
        populateZone(gapZone, "Gap LEDs", defaults.gap)
        populateZone(frameZone, "Frame LEDs", defaults.frame)
    }

    companion object {
        const val EXTRA_LAYER_ID = "layer_id"

        fun start(context: Context, layerId: Int) {
            context.startActivity(
                Intent(context, LedSettingsActivity::class.java)
                    .putExtra(EXTRA_LAYER_ID, layerId)
            )
        }
    }
}
