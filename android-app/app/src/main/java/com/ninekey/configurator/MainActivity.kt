package com.ninekey.configurator

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import android.os.Build
import android.os.Bundle
import android.view.View
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.Button
import androidx.appcompat.app.AppCompatActivity
import com.ninekey.configurator.databinding.ActivityMainBinding
import com.ninekey.configurator.model.AppProfile
import com.ninekey.configurator.model.KeyAssignment
import com.ninekey.configurator.model.KeycodeCatalog
import com.ninekey.configurator.model.LayerInfo
import com.ninekey.configurator.model.ProfileRepository
import com.ninekey.configurator.ui.ImportExportActivity
import com.ninekey.configurator.ui.KeyEditorSheet
import com.ninekey.configurator.ui.LedSettingsActivity
import com.ninekey.configurator.ui.OledPreviewActivity
import org.json.JSONObject

class MainActivity : AppCompatActivity() {

    private val usbPermissionAction = "com.ninekey.configurator.USB_PERMISSION"

    private lateinit var binding: ActivityMainBinding
    private lateinit var usbManager: UsbManager
    private lateinit var definition: KeyboardDefinition
    private lateinit var keyButtons: List<Button>

    private lateinit var catalog: KeycodeCatalog
    private lateinit var repo: ProfileRepository
    private lateinit var viaLayers: List<LayerInfo>
    private var activeProfile: AppProfile = AppProfile()

    private var protocolClient: RawHidProtocolClient? = null
    private var connectedDevice: UsbDevice? = null
    private var currentLayer: Int = 0
    private var receiverRegistered: Boolean = false

    private var keycodes: Array<Array<IntArray>> = emptyArray()

    private val usbReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            if (intent == null) return

            when (intent.action) {
                usbPermissionAction -> {
                    val device = intent.usbDeviceExtra()
                    val granted = intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)
                    if (granted) {
                        // Some Android versions/devices may not return EXTRA_DEVICE reliably.
                        val target = device ?: findCandidateDevice()
                        if (target != null && usbManager.hasPermission(target)) {
                            openAndInitialize(target)
                        } else {
                            setStatus("USB permission granted, but no device available")
                        }
                    } else {
                        setStatus("USB permission denied")
                    }
                }

                UsbManager.ACTION_USB_DEVICE_ATTACHED -> {
                    setStatus("USB device attached")
                }

                UsbManager.ACTION_USB_DEVICE_DETACHED -> {
                    val detached = intent.usbDeviceExtra()
                    if (detached != null && detached.deviceId == connectedDevice?.deviceId) {
                        closeConnection()
                        setStatus("Keyboard disconnected")
                    }
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        usbManager = getSystemService(Context.USB_SERVICE) as UsbManager
        definition = readKeyboardDefinition()
        keycodes = Array(definition.layers.coerceAtLeast(1)) {
            Array(definition.rows.coerceAtLeast(1)) { IntArray(definition.cols.coerceAtLeast(1)) }
        }

        catalog = KeycodeCatalog.load(this)
        repo = ProfileRepository(this)
        activeProfile = repo.loadProfile()
        viaLayers = catalog.layers.filter { it.viaSlot >= 0 }.sortedBy { it.viaSlot }

        keyButtons = listOf(
            binding.key00,
            binding.key01,
            binding.key02,
            binding.key10,
            binding.key11,
            binding.key12,
            binding.key20,
            binding.key21,
            binding.key22
        )

        updateKeyboardInfoText()
        setupLayerSpinner()
        setupButtons()
        registerUsbReceiver()
        setStatus("Connect keyboard to start VIA session")
    }

    override fun onResume() {
        super.onResume()
        // Re-render with potentially updated profile (e.g. after returning from OLED/LED editor)
        activeProfile = repo.loadProfile()
        renderLayer(currentLayer)
    }

    override fun onDestroy() {
        super.onDestroy()
        closeConnection()
        if (receiverRegistered) {
            unregisterReceiver(usbReceiver)
            receiverRegistered = false
        }
    }

    private fun setupButtons() {
        binding.connectButton.setOnClickListener { connectToKeyboard() }
        binding.refreshButton.setOnClickListener { loadLayer(currentLayer) }
        binding.saveButton.setOnClickListener { saveToEeprom() }

        binding.ledSettingsButton.setOnClickListener {
            val firmwareId = viaLayers.getOrNull(currentLayer)?.id ?: currentLayer
            LedSettingsActivity.start(this, firmwareId)
        }
        binding.oledPreviewButton.setOnClickListener {
            val firmwareId = viaLayers.getOrNull(currentLayer)?.id ?: currentLayer
            OledPreviewActivity.start(this, firmwareId)
        }
        binding.importExportButton.setOnClickListener {
            ImportExportActivity.start(this)
        }

        for (row in 0 until 3) {
            for (col in 0 until 3) {
                val index = row * 3 + col
                keyButtons[index].setOnClickListener {
                    showKeyEditDialog(row, col)
                }
            }
        }
    }

    private fun setupLayerSpinner() {
        val layerNames = if (viaLayers.isNotEmpty()) {
            viaLayers.map { "${it.displayName} [${it.shortLabel}]" }
        } else {
            (0 until definition.layers.coerceAtLeast(1)).map { "Layer $it" }
        }
        val adapter = ArrayAdapter(this, android.R.layout.simple_spinner_item, layerNames)
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        binding.layerSpinner.adapter = adapter
        binding.layerSpinner.setSelection(0)
        binding.layerSpinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                currentLayer = position
                renderLayer(currentLayer)
                loadLayer(currentLayer)
            }

            override fun onNothingSelected(parent: AdapterView<*>?) {
                // Keep previous layer selection.
            }
        }
    }

    private fun registerUsbReceiver() {
        val filter = IntentFilter().apply {
            addAction(usbPermissionAction)
            addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED)
            addAction(UsbManager.ACTION_USB_DEVICE_DETACHED)
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(usbReceiver, filter, Context.RECEIVER_NOT_EXPORTED)
        } else {
            registerReceiver(usbReceiver, filter)
        }
        receiverRegistered = true
    }

    private fun connectToKeyboard() {
        val device = findCandidateDevice()
        if (device == null) {
            setStatus("No compatible USB HID keyboard device found")
            return
        }

        if (usbManager.hasPermission(device)) {
            openAndInitialize(device)
            return
        }

        val permissionIntent = PendingIntent.getBroadcast(
            this,
            0,
            Intent(usbPermissionAction).setPackage(packageName),
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                PendingIntent.FLAG_MUTABLE
            } else {
                0
            }
        )
        usbManager.requestPermission(device, permissionIntent)
        setStatus("Waiting for USB permission")
    }

    private fun findCandidateDevice(): UsbDevice? {
        val devices = usbManager.deviceList.values

        val idMatched = devices.firstOrNull { device ->
            definition.vendorId != null &&
                definition.productId != null &&
                device.vendorId == definition.vendorId &&
                device.productId == definition.productId
        }
        if (idMatched != null) {
            return idMatched
        }

        return devices.firstOrNull { device ->
            RawHidTransport.hasInterruptInOutEndpoints(device)
        }
    }

    private fun openAndInitialize(device: UsbDevice) {
        setStatus("Opening device ${device.deviceName}")
        Thread {
            val client = RawHidProtocolClient.connect(usbManager, device, definition.packetSize)
            if (client == null) {
                runOnUiThread { setStatus("Failed to open VIA Raw HID interface") }
                return@Thread
            }

            protocolClient = client
            connectedDevice = device

            val pingOk = client.ping()
            val info = client.getInfo()

            runOnUiThread {
                if (!pingOk) {
                    setStatus("Connected but VIA handshake failed")
                    return@runOnUiThread
                }

                setStatus("Connected: VIA OK")
                if (info != null) {
                    binding.keyboardInfo.text = buildString {
                        append("Keyboard: ")
                        append(definition.displayName)
                        append("\n")
                        append("Matrix: ")
                        append(definition.rows)
                        append("x")
                        append(definition.cols)
                        append("\n")
                        append("Layers: ")
                        append(definition.layers)
                        append("\n")
                        append("Transport: via/raw_hid")
                        append("\n")
                        append("ID: ")
                        append(info.keyboardId)
                    }
                }
            }

            loadLayer(currentLayer)
        }.start()
    }

    private fun loadLayer(layer: Int) {
        val client = protocolClient
        if (client == null) {
            renderLayer(layer)
            return
        }

        Thread {
            var ok = true
            for (row in 0 until definition.rows) {
                for (col in 0 until definition.cols) {
                    val code = client.getKey(layer, row, col)
                    if (code == null) {
                        ok = false
                    } else {
                        keycodes[layer][row][col] = code
                    }
                }
            }

            runOnUiThread {
                if (ok) {
                    // Merge device keycodes into local profile, preserving custom labels
                    val rawKeycodes = IntArray(9) { i -> keycodes[layer][i / 3][i % 3] }
                    val layerInfo = viaLayers.getOrNull(layer)
                    if (layerInfo != null) {
                        activeProfile = repo.mergeDeviceKeycodes(activeProfile, layerInfo.id, rawKeycodes, catalog)
                        repo.saveProfile(activeProfile)
                    }
                }
                renderLayer(layer)
                setStatus(if (ok) "Layer $layer loaded" else "Layer $layer loaded with errors")
            }
        }.start()
    }

    private fun renderLayer(layer: Int) {
        val layerInfo = viaLayers.getOrNull(layer)
        val firmwareId = layerInfo?.id ?: layer
        val layerProfile = activeProfile.layers.firstOrNull { it.id == firmwareId }

        for (row in 0 until 3) {
            for (col in 0 until 3) {
                val button = keyButtons[row * 3 + col]
                if (row < definition.rows && col < definition.cols && layer < keycodes.size) {
                    val keycode = keycodes[layer][row][col]
                    val keyIndex = row * 3 + col
                    val assignment = layerProfile?.keys?.getOrNull(keyIndex)
                    val displayText = when {
                        assignment != null && assignment.label.isNotBlank() && assignment.label != "---" ->
                            assignment.label
                        keycode != 0 -> catalog.displayLabel(keycode)
                        else -> {
                            val legend = layerInfo?.keyLegends?.getOrNull(keyIndex)
                            legend?.takeIf { it.isNotBlank() } ?: "---"
                        }
                    }
                    button.text = displayText
                    button.isEnabled = true
                } else {
                    button.text = getString(R.string.key_button_unused, row, col)
                    button.isEnabled = false
                }
            }
        }
    }

    private fun showKeyEditDialog(row: Int, col: Int) {
        val keyIndex = row * 3 + col
        val layerInfo = viaLayers.getOrNull(currentLayer)
        val firmwareId = layerInfo?.id ?: currentLayer
        val layerProfile = activeProfile.layers.firstOrNull { it.id == firmwareId }
        val currentAssignment = layerProfile?.keys?.getOrNull(keyIndex)
            ?: KeyAssignment(
                code = if (currentLayer < keycodes.size) keycodes[currentLayer][row][col] else 0,
                label = catalog.displayLabel(if (currentLayer < keycodes.size) keycodes[currentLayer][row][col] else 0)
            )

        val sheet = KeyEditorSheet.newInstance(
            layerId = firmwareId,
            keyIndex = keyIndex,
            current = currentAssignment,
            catalog = catalog
        ) { assignment ->
            // Update local profile
            activeProfile = repo.updateKey(activeProfile, firmwareId, keyIndex, assignment)
            repo.saveProfile(activeProfile)
            renderLayer(currentLayer)

            // Push to device if connected
            if (protocolClient != null) {
                applyKeycode(currentLayer, row, col, assignment.code)
            }
        }
        sheet.show(supportFragmentManager, "key_editor")
    }

    private fun applyKeycode(layer: Int, row: Int, col: Int, keycode: Int) {
        val client = protocolClient ?: return
        Thread {
            val success = client.setKey(layer, row, col, keycode)
            runOnUiThread {
                if (success) {
                    keycodes[layer][row][col] = keycode
                    renderLayer(layer)
                    setStatus("Key updated L$layer R$row C$col")
                } else {
                    setStatus("Failed to update key")
                }
            }
        }.start()
    }

    private fun saveToEeprom() {
        val client = protocolClient
        if (client == null) {
            setStatus("Connect keyboard first")
            return
        }

        Thread {
            val success = client.saveEeprom()
            runOnUiThread {
                setStatus(if (success) "EEPROM saved" else "EEPROM save failed")
            }
        }.start()
    }

    private fun updateKeyboardInfoText() {
        binding.keyboardInfo.text = buildString {
            append("Keyboard: ")
            append(definition.displayName)
            append("\n")
            append("Matrix: ")
            append(definition.rows)
            append("x")
            append(definition.cols)
            append("\n")
            append("Layers: ")
            append(definition.layers)
            append("\n")
            append("Transport: ")
            append(definition.transport)
        }
    }

    private fun closeConnection() {
        protocolClient?.close()
        protocolClient = null
        connectedDevice = null
    }

    private fun setStatus(message: String) {
        runOnUiThread {
            binding.statusText.text = getString(R.string.status_template, message)
        }
    }

    private fun Intent.usbDeviceExtra(): UsbDevice? {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice::class.java)
        } else {
            @Suppress("DEPRECATION")
            getParcelableExtra(UsbManager.EXTRA_DEVICE)
        }
    }

    private fun readKeyboardDefinition(): KeyboardDefinition {
        val raw = assets.open("keyboard-definition.json").bufferedReader().use { it.readText() }
        val json = JSONObject(raw)
        return KeyboardDefinition(
            keyboard = json.optString("keyboard", "9key_makro_master"),
            displayName = json.optString("displayName", "9-Key Macro Master"),
            rows = json.optInt("rows", 3),
            cols = json.optInt("cols", 3),
            layers = json.optInt("layers", 4),
            encoders = json.optInt("encoders", 1),
            transport = json.optString("transport", "raw_hid"),
            packetSize = json.optInt("packetSize", 32),
            firmware = json.optString("firmware", "qmk"),
            vendorId = parseHexOrNull(json.optString("vendorId", "")),
            productId = parseHexOrNull(json.optString("productId", ""))
        )
    }

    private fun parseHexOrNull(text: String): Int? {
        val value = text.trim()
        if (value.isEmpty()) return null
        return if (value.startsWith("0x", ignoreCase = true)) {
            value.substring(2).toIntOrNull(16)
        } else {
            value.toIntOrNull()
        }
    }
}

data class KeyboardDefinition(
    val keyboard: String,
    val displayName: String,
    val rows: Int,
    val cols: Int,
    val layers: Int,
    val encoders: Int,
    val transport: String,
    val packetSize: Int,
    val firmware: String,
    val vendorId: Int?,
    val productId: Int?
)
