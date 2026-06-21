package com.ninekey.configurator

import android.hardware.usb.UsbConstants
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbDeviceConnection
import android.hardware.usb.UsbEndpoint
import android.hardware.usb.UsbInterface
import android.hardware.usb.UsbManager

private const val VIA_PROTOCOL_VERSION: Int = 0x000C
private const val VIA_ID_GET_PROTOCOL_VERSION: Int = 0x01
private const val VIA_ID_GET_KEYBOARD_VALUE: Int = 0x02
private const val VIA_ID_DYNAMIC_KEYMAP_GET_KEYCODE: Int = 0x04
private const val VIA_ID_DYNAMIC_KEYMAP_SET_KEYCODE: Int = 0x05
private const val VIA_ID_CUSTOM_SET_VALUE: Int = 0x07
private const val VIA_ID_CUSTOM_GET_VALUE: Int = 0x08
private const val VIA_ID_CUSTOM_SAVE: Int = 0x09
private const val VIA_ID_DYNAMIC_KEYMAP_GET_LAYER_COUNT: Int = 0x11
private const val VIA_ID_UNHANDLED: Int = 0xFF

const val VIA_CUSTOM_CHANNEL_ID: Int = 0x01

private const val VIA_KEYBOARD_VALUE_FIRMWARE_VERSION: Int = 0x04

class RawHidProtocolClient private constructor(
    private val transport: RawHidTransport,
    private val packetSize: Int
) {
    fun ping(): Boolean {
        val response = request(VIA_ID_GET_PROTOCOL_VERSION) ?: return false
        return ((response[1].toUnsignedInt() shl 8) or response[2].toUnsignedInt()) == VIA_PROTOCOL_VERSION
    }

    fun getInfo(): KeyboardInfo? {
        val response = request(VIA_ID_DYNAMIC_KEYMAP_GET_LAYER_COUNT) ?: return null
        val firmwareVersion = getFirmwareVersion()
        val keyboardId = if (firmwareVersion != null) {
            String.format("VIA 0x%08X", firmwareVersion)
        } else {
            "VIA"
        }

        return KeyboardInfo(
            rows = 0,
            cols = 0,
            layers = response[1].toUnsignedInt(),
            encoders = 0,
            packetSize = packetSize,
            keyboardId = keyboardId
        )
    }

    fun getKey(layer: Int, row: Int, col: Int): Int? {
        val response = request(
            VIA_ID_DYNAMIC_KEYMAP_GET_KEYCODE,
            byteArrayOf(layer.toByte(), row.toByte(), col.toByte())
        ) ?: return null

        return (response[4].toUnsignedInt() shl 8) or response[5].toUnsignedInt()
    }

    fun setKey(layer: Int, row: Int, col: Int, keycode: Int): Boolean {
        return request(
            VIA_ID_DYNAMIC_KEYMAP_SET_KEYCODE,
            byteArrayOf(
                layer.toByte(),
                row.toByte(),
                col.toByte(),
                ((keycode shr 8) and 0xFF).toByte(),
                (keycode and 0xFF).toByte()
            )
        ) != null
    }

    fun customSetValue(channelId: Int, valueId: Int, valueData: ByteArray = byteArrayOf()): Boolean {
        return customRequest(VIA_ID_CUSTOM_SET_VALUE, channelId, valueId, valueData) != null
    }

    fun customGetValue(channelId: Int, valueId: Int, valueData: ByteArray = byteArrayOf()): ByteArray? {
        val response = customRequest(VIA_ID_CUSTOM_GET_VALUE, channelId, valueId, valueData) ?: return null
        return response.copyOfRange(3, response.size)
    }

    fun customSave(channelId: Int): Boolean {
        return customRequest(VIA_ID_CUSTOM_SAVE, channelId, 0x00, byteArrayOf()) != null
    }

    fun saveEeprom(): Boolean {
        return true
    }

    fun close() {
        transport.close()
    }

    private fun getFirmwareVersion(): Int? {
        val response = request(
            VIA_ID_GET_KEYBOARD_VALUE,
            byteArrayOf(VIA_KEYBOARD_VALUE_FIRMWARE_VERSION.toByte())
        ) ?: return null

        return (response[2].toUnsignedInt() shl 24) or
            (response[3].toUnsignedInt() shl 16) or
            (response[4].toUnsignedInt() shl 8) or
            response[5].toUnsignedInt()
    }

    private fun request(command: Int, payload: ByteArray = byteArrayOf()): ByteArray? {
        if (payload.size > packetSize - 1) {
            return null
        }

        val packet = ByteArray(packetSize)
        packet[0] = command.toByte()
        System.arraycopy(payload, 0, packet, 1, payload.size)

        if (!transport.send(packet)) {
            return null
        }

        val response = transport.receive() ?: return null
        if (response.size != packetSize) return null

        if (response[0].toUnsignedInt() == VIA_ID_UNHANDLED) {
            return null
        }

        if (response[0].toUnsignedInt() != command) {
            return null
        }

        return response
    }

    private fun customRequest(command: Int, channelId: Int, valueId: Int, valueData: ByteArray = byteArrayOf()): ByteArray? {
        if (valueData.size > packetSize - 3) {
            return null
        }

        val payload = ByteArray(2 + valueData.size)
        payload[0] = channelId.toByte()
        payload[1] = valueId.toByte()
        System.arraycopy(valueData, 0, payload, 2, valueData.size)
        return request(command, payload)
    }

    companion object {
        fun connect(usbManager: UsbManager, device: UsbDevice, packetSize: Int): RawHidProtocolClient? {
            val transport = RawHidTransport.open(usbManager, device, packetSize) ?: return null
            return RawHidProtocolClient(transport, packetSize)
        }
    }
}

class RawHidTransport private constructor(
    private val connection: UsbDeviceConnection,
    private val usbInterface: UsbInterface,
    private val endpointIn: UsbEndpoint,
    private val endpointOut: UsbEndpoint,
    private val packetSize: Int
) {
    fun send(packet: ByteArray): Boolean {
        if (packet.size != packetSize) return false
        val transferred = connection.bulkTransfer(endpointOut, packet, packet.size, 1000)
        return transferred == packet.size
    }

    fun receive(): ByteArray? {
        val buffer = ByteArray(packetSize)
        val transferred = connection.bulkTransfer(endpointIn, buffer, buffer.size, 1000)
        if (transferred <= 0) return null
        if (transferred == packetSize) return buffer

        val trimmed = ByteArray(packetSize)
        System.arraycopy(buffer, 0, trimmed, 0, transferred)
        return trimmed
    }

    fun close() {
        connection.releaseInterface(usbInterface)
        connection.close()
    }

    companion object {
        fun open(usbManager: UsbManager, device: UsbDevice, packetSize: Int): RawHidTransport? {
            val selected = findRawHidInterface(device) ?: return null
            val connection = usbManager.openDevice(device) ?: return null

            if (!connection.claimInterface(selected.iface, true)) {
                connection.close()
                return null
            }

            return RawHidTransport(
                connection = connection,
                usbInterface = selected.iface,
                endpointIn = selected.endpointIn,
                endpointOut = selected.endpointOut,
                packetSize = packetSize
            )
        }

        fun hasInterruptInOutEndpoints(device: UsbDevice): Boolean {
            return findRawHidInterface(device) != null
        }

        private fun findRawHidInterface(device: UsbDevice): SelectedInterface? {
            for (i in 0 until device.interfaceCount) {
                val iface = device.getInterface(i)

                var endpointIn: UsbEndpoint? = null
                var endpointOut: UsbEndpoint? = null

                for (e in 0 until iface.endpointCount) {
                    val endpoint = iface.getEndpoint(e)
                    if (endpoint.type != UsbConstants.USB_ENDPOINT_XFER_INT) continue

                    if (endpoint.direction == UsbConstants.USB_DIR_IN && endpointIn == null) {
                        endpointIn = endpoint
                    } else if (endpoint.direction == UsbConstants.USB_DIR_OUT && endpointOut == null) {
                        endpointOut = endpoint
                    }
                }

                if (endpointIn != null && endpointOut != null) {
                    return SelectedInterface(iface, endpointIn, endpointOut)
                }
            }

            return null
        }
    }
}

data class KeyboardInfo(
    val rows: Int,
    val cols: Int,
    val layers: Int,
    val encoders: Int,
    val packetSize: Int,
    val keyboardId: String
)

private data class SelectedInterface(
    val iface: UsbInterface,
    val endpointIn: UsbEndpoint,
    val endpointOut: UsbEndpoint
)

private fun Byte.toUnsignedInt(): Int = toInt() and 0xFF

