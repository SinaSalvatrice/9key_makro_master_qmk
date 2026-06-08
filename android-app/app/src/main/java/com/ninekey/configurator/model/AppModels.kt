package com.ninekey.configurator.model

/**
 * A single keycode entry from the catalog.
 */
data class KeycodeEntry(
    val code: Int,
    val label: String,
    val display: String,
    val description: String,
    val category: String
)

/**
 * A category of keycodes in the catalog.
 */
data class KeycodeCategory(
    val id: String,
    val label: String,
    val keycodes: List<KeycodeEntry>
)

/**
 * Per-key assignment stored in a profile.
 */
data class KeyAssignment(
    val code: Int = 0,
    val label: String = "---",
    val oledLabel: String = "",
    val description: String = ""
)

/**
 * LED zone settings (key, gap, or frame) for one layer slot.
 */
data class LedZoneSettings(
    val effect: Int = 5,
    val speed: Int = 88,
    val hue: Int = 128,
    val sat: Int = 200,
    val value: Int = 100
)

/**
 * LED settings for all three zones for one layer slot.
 */
data class LedLayerSettings(
    val key: LedZoneSettings = LedZoneSettings(),
    val gap: LedZoneSettings = LedZoneSettings(effect = 10, speed = 80, hue = 128, sat = 180, value = 28),
    val frame: LedZoneSettings = LedZoneSettings(effect = 1, speed = 92, hue = 128, sat = 200, value = 86)
)

/**
 * A complete layer profile: 9 key assignments plus LED settings.
 */
data class LayerProfile(
    val id: Int,
    val name: String,
    val keys: Array<KeyAssignment> = Array(9) { KeyAssignment() },
    val led: LedLayerSettings = LedLayerSettings()
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false
        other as LayerProfile
        return id == other.id && name == other.name && keys.contentEquals(other.keys) && led == other.led
    }

    override fun hashCode(): Int {
        var result = id
        result = 31 * result + name.hashCode()
        result = 31 * result + keys.contentHashCode()
        result = 31 * result + led.hashCode()
        return result
    }
}

/**
 * An app-level configuration profile including all layers.
 */
data class AppProfile(
    val name: String = "Default",
    val layers: List<LayerProfile> = emptyList()
)

/**
 * Layer definition loaded from layer-definitions.json.
 */
data class LayerInfo(
    val id: Int,
    val name: String,
    val shortLabel: String,
    val displayName: String,
    val hue: Int,
    val sat: Int,
    val value: Int,
    val viaSlot: Int,
    val visible: Boolean,
    val keyLegends: List<String>,
    val keyFunctions: List<String>,
    val encoderFunction: String = ""
)

/**
 * Effect entry for LED selection.
 */
data class LedEffect(
    val id: Int,
    val label: String
)
