package com.ninekey.configurator

import android.hardware.usb.UsbConstants
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbDeviceConnection
import android.hardware.usb.UsbEndpoint
import android.hardware.usb.UsbInterface
import android.hardware.usb.UsbManager
import kotlin.random.Random

private const val PROTOCOL_VERSION: Int = 1
private const val STATUS_OK: Int = 0x00

private const val CMD_PING: Int = 0x01
private const val CMD_GET_INFO: Int = 0x02
private const val CMD_GET_KEY: Int = 0x10
private const val CMD_SET_KEY: Int = 0x11
private const val CMD_SAVE_EEPROM: Int = 0x20

class RawHidProtocolClient private constructor(
    private val transport: RawHidTransport,
    private val packetSize: Int
) {
    private var nextRequestId: Int = 1

    fun ping(): Boolean {
        val payload = ByteArray(packetSize)
        val nonce = Random.nextInt()
        payload[4] = (nonce and 0xFF).toByte()
        payload[5] = ((nonce shr 8) and 0xFF).toByte()
        payload[6] = ((nonce shr 16) and 0xFF).toByte()
        payload[7] = ((nonce shr 24) and 0xFF).toByte()

        val response = request(CMD_PING, payload) ?: return false
        if (response[3].toUnsignedInt() != STATUS_OK) return false

        val rNonce = response.readInt32(4)
        val pong = String(response.copyOfRange(8, 12), Charsets.US_ASCII)
        return rNonce == nonce && pong == "PONG"
    }

    fun getInfo(): KeyboardInfo? {
        val response = request(CMD_GET_INFO, ByteArray(packetSize)) ?: return null
        if (response[3].toUnsignedInt() != STATUS_OK) return null

        return KeyboardInfo(
            rows = response[4].toUnsignedInt(),
            cols = response[5].toUnsignedInt(),
            layers = response[6].toUnsignedInt(),
            encoders = response[7].toUnsignedInt(),
            packetSize = response[8].toUnsignedInt(),
            keyboardId = response.copyOfRange(9, 25).toAsciiTrimmed()
        )
    }

    fun getKey(layer: Int, row: Int, col: Int): Int? {
        val payload = ByteArray(packetSize)
        payload[4] = layer.toByte()
        payload[5] = row.toByte()
        payload[6] = col.toByte()

        val response = request(CMD_GET_KEY, payload) ?: return null
        if (response[3].toUnsignedInt() != STATUS_OK) return null

        return response.readUInt16(4)
    }

    fun setKey(layer: Int, row: Int, col: Int, keycode: Int): Boolean {
        val payload = ByteArray(packetSize)
        payload[4] = layer.toByte()
        payload[5] = row.toByte()
        payload[6] = col.toByte()
        payload[7] = (keycode and 0xFF).toByte()
        payload[8] = ((keycode shr 8) and 0xFF).toByte()

        val response = request(CMD_SET_KEY, payload) ?: return false
        if (response[3].toUnsignedInt() != STATUS_OK) return false

        return response[4].toUnsignedInt() == 1
    }

    fun saveEeprom(): Boolean {
        val response = request(CMD_SAVE_EEPROM, ByteArray(packetSize)) ?: return false
        if (response[3].toUnsignedInt() != STATUS_OK) return false
        return response[4].toUnsignedInt() == 1
    }

    fun close() {
        transport.close()
    }

    private fun request(command: Int, payload: ByteArray): ByteArray? {
        if (payload.size != packetSize) {
            return null
        }

        val packet = payload.copyOf()
        val requestId = nextRequestId and 0xFF
        packet[0] = PROTOCOL_VERSION.toByte()
        packet[1] = command.toByte()
        packet[2] = requestId.toByte()
        packet[3] = 0

        nextRequestId = (nextRequestId + 1) and 0xFF

        if (!transport.send(packet)) {
            return null
        }

        val response = transport.receive() ?: return null
        if (response.size != packetSize) return null

        val responseCommand = response[1].toUnsignedInt()
        val responseId = response[2].toUnsignedInt()

        if (responseCommand != command || responseId != requestId) {
            return null
        }

        return response
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

private fun ByteArray.readUInt16(offset: Int): Int {
    val lo = this[offset].toUnsignedInt()
    val hi = this[offset + 1].toUnsignedInt()
    return lo or (hi shl 8)
}

private fun ByteArray.readInt32(offset: Int): Int {
    val b0 = this[offset].toUnsignedInt()
    val b1 = this[offset + 1].toUnsignedInt()
    val b2 = this[offset + 2].toUnsignedInt()
    val b3 = this[offset + 3].toUnsignedInt()
    return b0 or (b1 shl 8) or (b2 shl 16) or (b3 shl 24)
}

private fun ByteArray.toAsciiTrimmed(): String {
    val raw = String(this, Charsets.US_ASCII)
    return raw.trim { it <= ' ' || it == '\u0000' }
}
