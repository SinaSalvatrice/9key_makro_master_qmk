package com.ninekey.configurator.model

import android.content.Context
import android.content.SharedPreferences
import org.json.JSONArray
import org.json.JSONObject

private const val PREF_NAME = "ninekey_profile"
private const val PREF_KEY_PROFILE = "active_profile"

/**
 * Manages loading, saving, and exporting/importing the user's key assignment profile and LED settings.
 *
 * Persistence uses SharedPreferences backed by JSON. All 9 VIA-accessible layer slots are stored,
 * one entry per layer keyed by the firmware layer ID.
 *
 * This repository is read-only to the connection layer – it does NOT touch USB/HID/BLE code.
 */
class ProfileRepository(private val context: Context) {

    private val prefs: SharedPreferences =
        context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE)

    // ── Load ──────────────────────────────────────────────────────────────────

    /** Loads the active profile from SharedPreferences, falling back to the bundled default. */
    fun loadProfile(): AppProfile {
        val saved = prefs.getString(PREF_KEY_PROFILE, null)
        if (saved != null) {
            return try {
                parseProfile(saved)
            } catch (e: Exception) {
                loadDefault()
            }
        }
        return loadDefault()
    }

    /** Loads the bundled default profile from assets. */
    fun loadDefault(): AppProfile {
        val raw = context.assets.open("default-profile.json").bufferedReader().use { it.readText() }
        return try {
            parseProfile(raw)
        } catch (e: Exception) {
            AppProfile()
        }
    }

    /** Loads the bundled LED presets and returns a list of [LedLayerSettings] indexed by via slot. */
    fun loadLedPresets(): List<LedLayerSettings> {
        val raw = context.assets.open("led-presets.json").bufferedReader().use { it.readText() }
        return try {
            parseLedPresets(raw)
        } catch (e: Exception) {
            emptyList()
        }
    }

    // ── Save ──────────────────────────────────────────────────────────────────

    /** Persists the profile to SharedPreferences. */
    fun saveProfile(profile: AppProfile) {
        prefs.edit().putString(PREF_KEY_PROFILE, serializeProfile(profile)).apply()
    }

    // ── Import / Export ───────────────────────────────────────────────────────

    /** Serializes the profile to a JSON string for export. */
    fun exportToJson(profile: AppProfile): String = serializeProfile(profile)

    /** Parses a previously exported JSON string back into an [AppProfile]. */
    fun importFromJson(json: String): AppProfile = parseProfile(json)

    // ── Profile helpers ───────────────────────────────────────────────────────

    /**
     * Updates a single key assignment in the profile and returns the modified copy.
     */
    fun updateKey(
        profile: AppProfile,
        layerId: Int,
        keyIndex: Int,
        assignment: KeyAssignment
    ): AppProfile {
        val updatedLayers = profile.layers.map { layer ->
            if (layer.id == layerId) {
                val updatedKeys = layer.keys.copyOf()
                if (keyIndex in updatedKeys.indices) {
                    updatedKeys[keyIndex] = assignment
                }
                layer.copy(keys = updatedKeys)
            } else {
                layer
            }
        }
        return profile.copy(layers = updatedLayers)
    }

    /**
     * Updates the LED settings for a layer and returns the modified copy.
     */
    fun updateLed(
        profile: AppProfile,
        layerId: Int,
        led: LedLayerSettings
    ): AppProfile {
        val updatedLayers = profile.layers.map { layer ->
            if (layer.id == layerId) layer.copy(led = led) else layer
        }
        return profile.copy(layers = updatedLayers)
    }

    /**
     * Merges raw keycodes read from the VIA device into the profile, preserving existing
     * oledLabel and description values where the keycode matches, and updating labels otherwise.
     */
    fun mergeDeviceKeycodes(
        profile: AppProfile,
        layerId: Int,
        rawKeycodes: IntArray,
        catalog: KeycodeCatalog
    ): AppProfile {
        val updatedLayers = profile.layers.map { layer ->
            if (layer.id == layerId) {
                val updatedKeys = layer.keys.copyOf()
                rawKeycodes.forEachIndexed { i, code ->
                    if (i < updatedKeys.size) {
                        val existing = updatedKeys[i]
                        val label = catalog.displayLabel(code)
                        updatedKeys[i] = if (existing.code == code) {
                            // Code unchanged – keep custom labels
                            existing
                        } else {
                            KeyAssignment(
                                code = code,
                                label = label,
                                oledLabel = existing.oledLabel.takeIf { it.isNotBlank() }
                                    ?: label.take(5),
                                description = catalog.descriptionFor(code)
                            )
                        }
                    }
                }
                layer.copy(keys = updatedKeys)
            } else {
                layer
            }
        }
        return profile.copy(layers = updatedLayers)
    }

    // ── Serialization ─────────────────────────────────────────────────────────

    private fun serializeProfile(profile: AppProfile): String {
        val root = JSONObject()
        root.put("version", "1.0")
        root.put("name", profile.name)

        val layersArr = JSONArray()
        for (layer in profile.layers) {
            val lObj = JSONObject()
            lObj.put("id", layer.id)
            lObj.put("name", layer.name)

            val keysArr = JSONArray()
            for (key in layer.keys) {
                val kObj = JSONObject()
                kObj.put("code", key.code)
                kObj.put("label", key.label)
                kObj.put("oledLabel", key.oledLabel)
                kObj.put("description", key.description)
                keysArr.put(kObj)
            }
            lObj.put("keys", keysArr)

            val ledObj = JSONObject()
            lObj.put("led", ledObj)
            ledObj.put("key", serializeLedZone(layer.led.key))
            ledObj.put("gap", serializeLedZone(layer.led.gap))
            ledObj.put("frame", serializeLedZone(layer.led.frame))

            layersArr.put(lObj)
        }
        root.put("layers", layersArr)
        return root.toString(2)
    }

    private fun serializeLedZone(z: LedZoneSettings): JSONObject {
        val o = JSONObject()
        o.put("effect", z.effect)
        o.put("speed", z.speed)
        o.put("hue", z.hue)
        o.put("sat", z.sat)
        o.put("val", z.value)
        return o
    }

    private fun parseProfile(text: String): AppProfile {
        val root = JSONObject(text)
        val name = root.optString("name", "Profile")
        val layersArr = root.getJSONArray("layers")
        val layers = mutableListOf<LayerProfile>()
        for (i in 0 until layersArr.length()) {
            val lObj = layersArr.getJSONObject(i)
            val layerId = lObj.getInt("id")
            val layerName = lObj.getString("name")

            val keysArr = lObj.getJSONArray("keys")
            val keys = Array(keysArr.length()) { j ->
                val kObj = keysArr.getJSONObject(j)
                KeyAssignment(
                    code = kObj.getInt("code"),
                    label = kObj.optString("label", "---"),
                    oledLabel = kObj.optString("oledLabel", ""),
                    description = kObj.optString("description", "")
                )
            }

            val ledSettings = if (lObj.has("led")) {
                val ledObj = lObj.getJSONObject("led")
                LedLayerSettings(
                    key = parseLedZone(ledObj.optJSONObject("key")),
                    gap = parseLedZone(ledObj.optJSONObject("gap")),
                    frame = parseLedZone(ledObj.optJSONObject("frame"))
                )
            } else {
                LedLayerSettings()
            }

            layers.add(LayerProfile(layerId, layerName, keys, ledSettings))
        }
        return AppProfile(name, layers)
    }

    private fun parseLedZone(obj: JSONObject?): LedZoneSettings {
        obj ?: return LedZoneSettings()
        return LedZoneSettings(
            effect = obj.optInt("effect", 5),
            speed = obj.optInt("speed", 88),
            hue = obj.optInt("hue", 128),
            sat = obj.optInt("sat", 200),
            value = obj.optInt("val", 100)
        )
    }

    private fun parseLedPresets(text: String): List<LedLayerSettings> {
        val root = JSONObject(text)
        val slotsArr = root.getJSONArray("slots")
        return (0 until slotsArr.length()).map { i ->
            val s = slotsArr.getJSONObject(i)
            LedLayerSettings(
                key = parseLedZone(s.optJSONObject("key")),
                gap = parseLedZone(s.optJSONObject("gap")),
                frame = parseLedZone(s.optJSONObject("frame"))
            )
        }
    }
}
