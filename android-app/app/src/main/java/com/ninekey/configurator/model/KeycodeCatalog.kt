package com.ninekey.configurator.model

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject

/**
 * Loads the keycode catalog and layer definitions from assets. Provides label lookup for
 * raw QMK 16-bit keycodes and decodes compound keycodes (LT, MO, mod combos, tap dance, etc.).
 */
class KeycodeCatalog private constructor(
    private val categories: List<KeycodeCategory>,
    val layers: List<LayerInfo>,
    val effects: List<LedEffect>,
    private val codeToEntry: Map<Int, KeycodeEntry>
) {

    // ── Label lookup ──────────────────────────────────────────────────────────

    /**
     * Returns a short human-readable display string for a raw 16-bit QMK keycode.
     * Falls back to "0x%04X" for unrecognized codes.
     */
    fun displayLabel(code: Int): String {
        if (code == 0x0000) return "---"
        if (code == 0x0001) return "▽"

        // Tap dance: 0x5700-0x57FF
        if (code in 0x5700..0x57FF) {
            codeToEntry[code]?.let { return it.display }
            return "TD(${code and 0xFF})"
        }

        // Layer-tap: 0x4000-0x4FFF  →  LT(layer, kc)
        if (code in 0x4000..0x4FFF) {
            val layer = (code shr 8) and 0x0F
            val baseKc = code and 0xFF
            val layerName = layers.firstOrNull { it.id == layer }?.shortLabel ?: "$layer"
            val baseName = basicKeyDisplay(baseKc) ?: displayLabel(baseKc)
            return "LT($layerName,${baseName})"
        }

        // Momentary layer (MO): 0x5100-0x51FF
        if (code in 0x5100..0x51FF) {
            val layer = code and 0xFF
            val layerName = layers.firstOrNull { it.id == layer }?.shortLabel ?: "$layer"
            return "MO($layerName)"
        }

        // Toggle layer (TG): 0x5300-0x53FF
        if (code in 0x5300..0x53FF) {
            val layer = code and 0xFF
            val layerName = layers.firstOrNull { it.id == layer }?.shortLabel ?: "$layer"
            return "TG($layerName)"
        }

        // Activate only layer (TO): 0x5000-0x50FF
        if (code in 0x5000..0x50FF) {
            val layer = code and 0xFF
            val layerName = layers.firstOrNull { it.id == layer }?.shortLabel ?: "$layer"
            return "TO($layerName)"
        }

        // Mod-tap (MT): 0x6000-0x7DFF
        if (code in 0x6000..0x7DFF) {
            val mod = (code shr 8) and 0x1F
            val baseKc = code and 0xFF
            val modStr = buildModString(mod)
            val baseName = basicKeyDisplay(baseKc) ?: "0x%02X".format(baseKc)
            return "MT($modStr,$baseName)"
        }

        // Modifier+key combos: 0x0100-0x1FFF
        if (code in 0x0100..0x1FFF) {
            val modByte = (code shr 8) and 0x1F
            val baseKc = code and 0xFF
            val modStr = buildModString(modByte)
            val baseName = basicKeyDisplay(baseKc) ?: "0x%02X".format(baseKc)
            return if (modStr.isEmpty()) "0x%04X".format(code) else "$modStr+$baseName"
        }

        // Direct catalog lookup (includes custom keycodes at 0x7E00+)
        codeToEntry[code]?.let { return it.display }

        // Basic key lookup (0x0000-0x00FF)
        basicKeyDisplay(code)?.let { return it }

        return "0x%04X".format(code)
    }

    /**
     * Returns a longer description for a keycode (used in the key editor).
     */
    fun descriptionFor(code: Int): String {
        codeToEntry[code]?.let { return it.description }

        if (code in 0x4000..0x4FFF) {
            val layer = (code shr 8) and 0x0F
            val baseKc = code and 0xFF
            val layerName = layers.firstOrNull { it.id == layer }?.displayName ?: "Layer $layer"
            val baseName = basicKeyDisplay(baseKc) ?: "0x%02X".format(baseKc)
            return "Layer-tap: tap = $baseName, hold = activate $layerName"
        }

        if (code in 0x0100..0x1FFF) {
            val modByte = (code shr 8) and 0x1F
            val baseKc = code and 0xFF
            val modStr = buildModStringLong(modByte)
            val baseName = basicKeyDisplay(baseKc) ?: "0x%02X".format(baseKc)
            return "$modStr + $baseName"
        }

        if (code == 0x0000) return "No operation (empty key)"
        if (code == 0x0001) return "Transparent – passes through to the layer below"

        return "Keycode 0x%04X".format(code)
    }

    /** Returns all categories for the keycode picker UI. */
    fun getCategories(): List<KeycodeCategory> = categories

    /** Returns all layer infos. */
    fun getLayers(): List<LayerInfo> = layers

    /** Returns all visible layers. */
    fun getVisibleLayers(): List<LayerInfo> = layers.filter { it.visible }

    /** Returns the layer info for a given firmware layer id, or null. */
    fun layerById(id: Int): LayerInfo? = layers.firstOrNull { it.id == id }

    // ── Internal helpers ──────────────────────────────────────────────────────

    private fun basicKeyDisplay(code: Int): String? {
        return when (code) {
            0x00 -> "---"
            0x01 -> "▽"
            in 0x04..0x1D -> ('A' + (code - 0x04)).toString()
            0x1E -> "1"; 0x1F -> "2"; 0x20 -> "3"; 0x21 -> "4"; 0x22 -> "5"
            0x23 -> "6"; 0x24 -> "7"; 0x25 -> "8"; 0x26 -> "9"; 0x27 -> "0"
            0x28 -> "Enter"; 0x29 -> "Esc"; 0x2A -> "⌫"; 0x2B -> "Tab"; 0x2C -> "Space"
            0x2D -> "-/_"; 0x2E -> "=/+"; 0x2F -> "[/{"; 0x30 -> "]/}"
            0x31 -> "\\|"; 0x33 -> ";/:"; 0x34 -> "'\""; 0x35 -> "`~"
            0x36 -> ",/<"; 0x37 -> "./>"; 0x38 -> "/?"; 0x39 -> "Caps"
            0x3A -> "F1"; 0x3B -> "F2"; 0x3C -> "F3"; 0x3D -> "F4"
            0x3E -> "F5"; 0x3F -> "F6"; 0x40 -> "F7"; 0x41 -> "F8"
            0x42 -> "F9"; 0x43 -> "F10"; 0x44 -> "F11"; 0x45 -> "F12"
            0x46 -> "PrtSc"; 0x47 -> "ScLk"; 0x48 -> "Pause"
            0x49 -> "Ins"; 0x4A -> "Home"; 0x4B -> "PgUp"
            0x4C -> "Del"; 0x4D -> "End"; 0x4E -> "PgDn"
            0x4F -> "→"; 0x50 -> "←"; 0x51 -> "↓"; 0x52 -> "↑"
            0x53 -> "NumLk"
            0xA0 -> "Mute"; 0xA1 -> "Vol+"; 0xA2 -> "Vol-"
            0xA3 -> "Next"; 0xA4 -> "Prev"; 0xA5 -> "FF"; 0xA6 -> "Rew"
            0xA7 -> "Play"; 0xA8 -> "Stop"
            0xE0 -> "LCtrl"; 0xE1 -> "LShift"; 0xE2 -> "LAlt"; 0xE3 -> "LWin"
            0xE4 -> "RCtrl"; 0xE5 -> "RShift"; 0xE6 -> "RAlt"; 0xE7 -> "RWin"
            else -> null
        }
    }

    private fun buildModString(modByte: Int): String {
        val parts = mutableListOf<String>()
        if (modByte and 0x01 != 0) parts.add("LCtrl")
        if (modByte and 0x02 != 0) parts.add("LShift")
        if (modByte and 0x04 != 0) parts.add("LAlt")
        if (modByte and 0x08 != 0) parts.add("LWin")
        if (modByte and 0x10 != 0) parts.add("RCtrl")
        if (modByte and 0x20 != 0) parts.add("RShift")
        return parts.joinToString("+")
    }

    private fun buildModStringLong(modByte: Int): String {
        val parts = mutableListOf<String>()
        if (modByte and 0x01 != 0) parts.add("Left Control")
        if (modByte and 0x02 != 0) parts.add("Left Shift")
        if (modByte and 0x04 != 0) parts.add("Left Alt")
        if (modByte and 0x08 != 0) parts.add("Left GUI/Win")
        if (modByte and 0x10 != 0) parts.add("Right Control")
        if (modByte and 0x20 != 0) parts.add("Right Shift")
        return if (parts.isEmpty()) "Unknown modifier" else parts.joinToString(" + ")
    }

    // ── Factory ───────────────────────────────────────────────────────────────

    companion object {
        fun load(context: Context): KeycodeCatalog {
            val catalogJson = context.assets.open("keycode-catalog.json")
                .bufferedReader().use { it.readText() }
            val layerJson = context.assets.open("layer-definitions.json")
                .bufferedReader().use { it.readText() }
            val ledJson = context.assets.open("led-presets.json")
                .bufferedReader().use { it.readText() }

            return parse(catalogJson, layerJson, ledJson)
        }

        private fun parse(catalogText: String, layerText: String, ledText: String): KeycodeCatalog {
            val catalogRoot = JSONObject(catalogText)
            val layerRoot = JSONObject(layerText)
            val ledRoot = JSONObject(ledText)

            val categories = mutableListOf<KeycodeCategory>()
            val codeMap = mutableMapOf<Int, KeycodeEntry>()

            val catArray = catalogRoot.getJSONArray("categories")
            for (i in 0 until catArray.length()) {
                val catObj = catArray.getJSONObject(i)
                val catId = catObj.getString("id")
                val catLabel = catObj.getString("label")
                val keycodeArr = catObj.optJSONArray("keycodes") ?: JSONArray()
                val entries = mutableListOf<KeycodeEntry>()
                for (j in 0 until keycodeArr.length()) {
                    val kObj = keycodeArr.getJSONObject(j)
                    val entry = KeycodeEntry(
                        code = kObj.getInt("code"),
                        label = kObj.getString("label"),
                        display = kObj.optString("display", kObj.getString("label")),
                        description = kObj.optString("description", ""),
                        category = catId
                    )
                    entries.add(entry)
                    codeMap[entry.code] = entry
                }
                categories.add(KeycodeCategory(catId, catLabel, entries))
            }

            val layerArray = layerRoot.getJSONArray("layers")
            val layers = mutableListOf<LayerInfo>()
            for (i in 0 until layerArray.length()) {
                val lObj = layerArray.getJSONObject(i)
                val legends = parseStringArray(lObj.getJSONArray("keyLegends"))
                val functions = parseStringArray(lObj.getJSONArray("keyFunctions"))
                layers.add(
                    LayerInfo(
                        id = lObj.getInt("id"),
                        name = lObj.getString("name"),
                        shortLabel = lObj.getString("shortLabel"),
                        displayName = lObj.getString("displayName"),
                        hue = lObj.getInt("hue"),
                        sat = lObj.getInt("sat"),
                        value = lObj.getInt("val"),
                        viaSlot = lObj.getInt("viaSlot"),
                        visible = lObj.getBoolean("visible"),
                        keyLegends = legends,
                        keyFunctions = functions,
                        encoderFunction = lObj.optString("encoderFunction", "")
                    )
                )
            }

            val effectArray = ledRoot.getJSONArray("effects")
            val effects = mutableListOf<LedEffect>()
            for (i in 0 until effectArray.length()) {
                val eObj = effectArray.getJSONObject(i)
                effects.add(LedEffect(eObj.getInt("id"), eObj.getString("label")))
            }

            return KeycodeCatalog(categories, layers, effects, codeMap)
        }

        private fun parseStringArray(arr: JSONArray): List<String> {
            return (0 until arr.length()).map { arr.getString(it) }
        }
    }
}
