from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import json


@dataclass(frozen=True)
class KeyboardDefinition:
    keyboard: str
    display_name: str
    rows: int
    cols: int
    layers: int
    encoders: int
    transport: str
    packet_size: int
    firmware: str
    vendor_id: int | None
    product_id: int | None

    @classmethod
    def default(cls) -> "KeyboardDefinition":
        return cls(
            keyboard="9key_makro_master",
            display_name="9-Key Macro Master",
            rows=3,
            cols=3,
            layers=4,
            encoders=1,
            transport="raw_hid",
            packet_size=32,
            firmware="qmk",
            vendor_id=0xFEED,
            product_id=0x9B01,
        )

    @staticmethod
    def _parse_hex_or_int(value: object) -> int | None:
        if value is None:
            return None
        if isinstance(value, int):
            return value
        if not isinstance(value, str):
            return None
        text = value.strip()
        if not text:
            return None
        try:
            if text.lower().startswith("0x"):
                return int(text[2:], 16)
            return int(text, 10)
        except ValueError:
            return None

    @classmethod
    def from_json_file(cls, path: Path) -> "KeyboardDefinition":
        raw = path.read_text(encoding="utf-8")
        data = json.loads(raw)
        return cls(
            keyboard=str(data.get("keyboard", "9key_makro_master")),
            display_name=str(data.get("displayName", "9-Key Macro Master")),
            rows=int(data.get("rows", 3)),
            cols=int(data.get("cols", 3)),
            layers=int(data.get("layers", 4)),
            encoders=int(data.get("encoders", 1)),
            transport=str(data.get("transport", "raw_hid")),
            packet_size=int(data.get("packetSize", 32)),
            firmware=str(data.get("firmware", "qmk")),
            vendor_id=cls._parse_hex_or_int(data.get("vendorId")),
            product_id=cls._parse_hex_or_int(data.get("productId")),
        )
