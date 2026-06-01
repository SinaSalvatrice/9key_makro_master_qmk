package com.ninekey.configurator

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import com.ninekey.configurator.databinding.ActivityMainBinding
import org.json.JSONObject

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        val info = readKeyboardDefinition()
        binding.keyboardInfo.text = buildString {
            append("Keyboard: ")
            append(info.optString("displayName", "Unknown"))
            append("\n")
            append("Matrix: ")
            append(info.optInt("rows", 0))
            append("x")
            append(info.optInt("cols", 0))
            append("\n")
            append("Layers: ")
            append(info.optInt("layers", 0))
            append("\n")
            append("Transport: ")
            append(info.optString("transport", "n/a"))
        }

        binding.statusText.text = getString(
            R.string.status_template,
            "UI scaffold ready. USB Raw HID integration is next."
        )
    }

    private fun readKeyboardDefinition(): JSONObject {
        val raw = assets.open("keyboard-definition.json").bufferedReader().use { it.readText() }
        return JSONObject(raw)
    }
}
