from __future__ import annotations

from dataclasses import dataclass


PROTOCOL_VERSION = 1
STATUS_OK = 0x00

CMD_PING = 0x01
CMD_GET_INFO = 0x02
CMD_GET_KEY = 0x10
CMD_SET_KEY = 0x11
CMD_SAVE_EEPROM = 0x20


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
        self._next_request_id = 1

    def close(self) -> None:
        self._transport.close()

    def ping(self) -> bool:
        import random

        payload = bytearray(self._packet_size)
        nonce = random.randint(-2**31, 2**31 - 1)
        payload[4:8] = int(nonce & 0xFFFFFFFF).to_bytes(4, "little", signed=False)

        response = self._request(CMD_PING, payload)
        if response is None or _u8(response[3]) != STATUS_OK:
            return False

        r_nonce = _read_i32_le(response, 4)
        pong = _ascii_trimmed(response[8:12])

        # Kotlin uses signed Int for nonce; compare on 32-bit wrap.
        return (r_nonce & 0xFFFFFFFF) == (nonce & 0xFFFFFFFF) and pong == "PONG"

    def get_info(self) -> KeyboardInfo | None:
        response = self._request(CMD_GET_INFO, bytearray(self._packet_size))
        if response is None or _u8(response[3]) != STATUS_OK:
            return None

        return KeyboardInfo(
            rows=_u8(response[4]),
            cols=_u8(response[5]),
            layers=_u8(response[6]),
            encoders=_u8(response[7]),
            packet_size=_u8(response[8]),
            keyboard_id=_ascii_trimmed(response[9:25]),
        )

    def get_key(self, layer: int, row: int, col: int) -> int | None:
        payload = bytearray(self._packet_size)
        payload[4] = layer & 0xFF
        payload[5] = row & 0xFF
        payload[6] = col & 0xFF

        response = self._request(CMD_GET_KEY, payload)
        if response is None or _u8(response[3]) != STATUS_OK:
            return None

        return _read_u16_le(response, 4)

    def set_key(self, layer: int, row: int, col: int, keycode: int) -> bool:
        payload = bytearray(self._packet_size)
        payload[4] = layer & 0xFF
        payload[5] = row & 0xFF
        payload[6] = col & 0xFF
        payload[7] = keycode & 0xFF
        payload[8] = (keycode >> 8) & 0xFF

        response = self._request(CMD_SET_KEY, payload)
        if response is None or _u8(response[3]) != STATUS_OK:
            return False

        return _u8(response[4]) == 1

    def save_eeprom(self) -> bool:
        response = self._request(CMD_SAVE_EEPROM, bytearray(self._packet_size))
        if response is None or _u8(response[3]) != STATUS_OK:
            return False
        return _u8(response[4]) == 1

    def _request(self, command: int, payload: bytearray) -> bytes | None:
        if len(payload) != self._packet_size:
            return None

        packet = bytearray(payload)
        request_id = self._next_request_id & 0xFF
        packet[0] = PROTOCOL_VERSION
        packet[1] = command & 0xFF
        packet[2] = request_id
        packet[3] = 0

        self._next_request_id = (self._next_request_id + 1) & 0xFF

        if not self._transport.send(bytes(packet)):
            return None

        response = self._transport.receive(timeout_ms=1000)
        if response is None or len(response) != self._packet_size:
            return None

        if _u8(response[1]) != (command & 0xFF) or _u8(response[2]) != request_id:
            return None

        return response
