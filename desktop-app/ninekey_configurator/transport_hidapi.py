from __future__ import annotations

from dataclasses import dataclass
from typing import Any

import hid  # type: ignore

from .rawhid import RawHidTransport


QMK_RAW_USAGE_PAGE = 0xFF60
QMK_RAW_USAGE = 0x61


@dataclass(frozen=True)
class HidDeviceRef:
    path: bytes | str
    product_string: str | None = None
    manufacturer_string: str | None = None
    interface_number: int | None = None
    usage_page: int | None = None
    usage: int | None = None


class HidApiTransport(RawHidTransport):
    def __init__(self, device: Any, packet_size: int):
        self._dev = device
        self._packet_size = int(packet_size)

    @staticmethod
    def enumerate(vid: int | None, pid: int | None) -> list[HidDeviceRef]:
        if vid is not None and pid is not None:
            devices = hid.enumerate(vid, pid)
        else:
            devices = hid.enumerate()

        out: list[HidDeviceRef] = []
        for d in devices:
            path = d.get("path")
            if not isinstance(path, (bytes, bytearray, str)):
                continue
            out.append(
                HidDeviceRef(
                    path=bytes(path) if isinstance(path, bytearray) else path,
                    product_string=d.get("product_string"),
                    manufacturer_string=d.get("manufacturer_string"),
                    interface_number=d.get("interface_number"),
                    usage_page=d.get("usage_page"),
                    usage=d.get("usage"),
                )
            )

        def rank(ref: HidDeviceRef) -> tuple[int, int, int]:
            is_qmk_raw = ref.usage_page == QMK_RAW_USAGE_PAGE and ref.usage == QMK_RAW_USAGE
            is_vendor_defined = ref.usage_page is not None and 0xFF00 <= ref.usage_page <= 0xFFFF
            interface_number = ref.interface_number if ref.interface_number is not None else 9999
            return (0 if is_qmk_raw else 1 if is_vendor_defined else 2, interface_number, 0)

        out.sort(key=rank)
        return out

    @classmethod
    def open_first(cls, vid: int | None, pid: int | None, packet_size: int) -> "HidApiTransport | None":
        refs = cls.enumerate(vid, pid)
        if not refs:
            return None

        dev = hid.Device(path=refs[0].path)
        dev.nonblocking = False
        return cls(dev, packet_size)

    def send(self, packet: bytes) -> bool:
        if len(packet) != self._packet_size:
            return False

        # hidapi on Windows commonly expects a leading Report ID byte.
        # QMK Raw HID typically uses report ID 0.
        with_report_id = bytes([0x00]) + packet
        try:
            written = self._dev.write(with_report_id)
            if written in (len(with_report_id), len(packet)):
                return True
        except Exception:
            pass

        # Fallback: try without report id.
        try:
            written = self._dev.write(packet)
            return written in (len(packet), len(with_report_id))
        except Exception:
            return False

    def receive(self, timeout_ms: int = 1000) -> bytes | None:
        size_a = self._packet_size + 1
        size_b = self._packet_size

        try:
            data = self._dev.read(size_a, timeout_ms)
        except Exception:
            data = []

        if not data:
            try:
                data = self._dev.read(size_b, timeout_ms)
            except Exception:
                return None

        if not data:
            return None

        buf = bytes(bytearray(data))

        if len(buf) == self._packet_size + 1 and buf[0] == 0x00:
            buf = buf[1:]

        if len(buf) != self._packet_size:
            return None

        return buf

    def close(self) -> None:
        try:
            self._dev.close()
        except Exception:
            pass
