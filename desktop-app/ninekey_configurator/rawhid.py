from __future__ import annotations

from dataclasses import dataclass


VIA_PROTOCOL_VERSION = 0x000C
VIA_ID_GET_PROTOCOL_VERSION = 0x01
VIA_ID_GET_KEYBOARD_VALUE = 0x02
VIA_ID_DYNAMIC_KEYMAP_GET_KEYCODE = 0x04
VIA_ID_DYNAMIC_KEYMAP_SET_KEYCODE = 0x05
VIA_ID_CUSTOM_SET_VALUE = 0x07
VIA_ID_CUSTOM_GET_VALUE = 0x08
VIA_ID_CUSTOM_SAVE = 0x09
VIA_ID_DYNAMIC_KEYMAP_GET_LAYER_COUNT = 0x11
VIA_ID_UNHANDLED = 0xFF

VIA_CUSTOM_CHANNEL_ID = 0x01

VIA_KEYBOARD_VALUE_FIRMWARE_VERSION = 0x04


class RawHidTransport:
    def send(self, packet: bytes) -> bool:  # pragma: no cover
        raise NotImplementedError

    def receive(self, timeout_ms: int = 1000) -> bytes | None:  # pragma: no cover
        raise NotImplementedError

    def close(self) -> None:  # pragma: no cover
        raise NotImplementedError


@dataclass(frozen=True)
class KeyboardInfo:
    rows: int
    cols: int
    layers: int
    encoders: int
    packet_size: int
    keyboard_id: str


def _u8(b: int) -> int:
    return b & 0xFF


def _read_u16_le(buf: bytes, offset: int) -> int:
    return _u8(buf[offset]) | (_u8(buf[offset + 1]) << 8)


def _read_i32_le(buf: bytes, offset: int) -> int:
    return (
        _u8(buf[offset])
        | (_u8(buf[offset + 1]) << 8)
        | (_u8(buf[offset + 2]) << 16)
        | (_u8(buf[offset + 3]) << 24)
    )


def _ascii_trimmed(data: bytes) -> str:
    try:
        raw = data.decode("ascii", errors="ignore")
    except Exception:
        return ""
    return raw.strip("\x00\r\n\t ")


class RawHidProtocolClient:
    def __init__(self, transport: RawHidTransport, packet_size: int):
        self._transport = transport
        self._packet_size = int(packet_size)

    def close(self) -> None:
        self._transport.close()

    def handshake(self) -> bool:
        response = self._request(VIA_ID_GET_PROTOCOL_VERSION)
        return response is not None

    def get_info(self) -> KeyboardInfo | None:
        response = self._request(VIA_ID_DYNAMIC_KEYMAP_GET_LAYER_COUNT)
        if response is None:
            return None

        firmware_version = self._get_firmware_version()
        keyboard_id = f"VIA 0x{firmware_version:08X}" if firmware_version is not None else "VIA"

        return KeyboardInfo(
            rows=0,
            cols=0,
            layers=_u8(response[1]),
            encoders=0,
            packet_size=self._packet_size,
            keyboard_id=keyboard_id,
        )

    def get_key(self, layer: int, row: int, col: int) -> int | None:
        response = self._request(VIA_ID_DYNAMIC_KEYMAP_GET_KEYCODE, bytes([layer & 0xFF, row & 0xFF, col & 0xFF]))
        if response is None or len(response) < 6:
            return None

        return (_u8(response[4]) << 8) | _u8(response[5])

    def set_key(self, layer: int, row: int, col: int, keycode: int) -> bool:
        response = self._request(
            VIA_ID_DYNAMIC_KEYMAP_SET_KEYCODE,
            bytes([
                layer & 0xFF,
                row & 0xFF,
                col & 0xFF,
                (keycode >> 8) & 0xFF,
                keycode & 0xFF,
            ]),
        )
        return response is not None

    def custom_set_value(self, channel_id: int, value_id: int, value_data: bytes = b"") -> bool:
        response = self._custom_request(VIA_ID_CUSTOM_SET_VALUE, channel_id, value_id, value_data)
        return response is not None

    def custom_get_value(self, channel_id: int, value_id: int, value_data: bytes = b"") -> bytes | None:
        response = self._custom_request(VIA_ID_CUSTOM_GET_VALUE, channel_id, value_id, value_data)
        if response is None:
            return None
        return response[3:]

    def custom_save(self, channel_id: int) -> bool:
        response = self._custom_request(VIA_ID_CUSTOM_SAVE, channel_id, 0x00, b"")
        return response is not None

    def save_eeprom(self) -> bool:
        # VIA dynamic keymap writes are persisted by firmware; no separate save command is required.
        return True

    def _get_firmware_version(self) -> int | None:
        response = self._request(VIA_ID_GET_KEYBOARD_VALUE, bytes([VIA_KEYBOARD_VALUE_FIRMWARE_VERSION]))
        if response is None or len(response) < 6:
            return None
        return (_u8(response[2]) << 24) | (_u8(response[3]) << 16) | (_u8(response[4]) << 8) | _u8(response[5])

    def _request(self, command: int, payload: bytes = b"") -> bytes | None:
        if len(payload) > self._packet_size - 1:
            return None

        packet = bytearray(self._packet_size)
        packet[0] = command & 0xFF
        packet[1 : 1 + len(payload)] = payload

        if not self._transport.send(bytes(packet)):
            return None

        response = self._transport.receive(timeout_ms=1000)
        if response is None or len(response) != self._packet_size:
            return None

        if _u8(response[0]) == VIA_ID_UNHANDLED:
            return None

        if _u8(response[0]) != (command & 0xFF):
            return None

        return response

    def _custom_request(self, command: int, channel_id: int, value_id: int, value_data: bytes = b"") -> bytes | None:
        if len(value_data) > self._packet_size - 3:
            return None

        payload = bytes([channel_id & 0xFF, value_id & 0xFF]) + value_data
        return self._request(command, payload)
