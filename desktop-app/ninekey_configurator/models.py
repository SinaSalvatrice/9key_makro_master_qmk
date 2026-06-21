from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
import json
import sys


def _resource_root() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent / "resources"
    return Path(__file__).resolve().parents[1] / "resources"


def _shared_root() -> Path:
    return Path(__file__).resolve().parents[2] / "shared"


def _load_json_text(filename: str) -> str:
    candidates = [
        _resource_root() / filename,
    ]

    # Dev fallback: shared data in repository root.
    if not getattr(sys, "frozen", False):
        candidates.append(_shared_root() / filename)

    # PyInstaller one-file extraction path fallback.
    meipass = getattr(sys, "_MEIPASS", None)
    if meipass:
        candidates.append(Path(meipass) / "resources" / filename)

    for path in candidates:
        if path.exists():
            return path.read_text(encoding="utf-8")

    raise FileNotFoundError(f"Could not locate {filename}")


def profile_store_path() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent / "profile.json"
    return Path(__file__).resolve().parents[1] / "resources" / "profile.json"


@dataclass(frozen=True)
class KeycodeEntry:
    code: int
    label: str
    display: str
    description: str
    category: str


@dataclass(frozen=True)
class KeycodeCategory:
    id: str
    label: str
    keycodes: list[KeycodeEntry]


@dataclass(frozen=True)
class LayerInfo:
    id: int
    name: str
    short_label: str
    display_name: str
    hue: int
    sat: int
    value: int
    via_slot: int
    visible: bool
    key_legends: list[str]
    key_functions: list[str]
    encoder_function: str = ""


@dataclass(frozen=True)
class LedEffect:
    id: int
    label: str


@dataclass(frozen=True)
class KeyAssignment:
    code: int = 0
    label: str = "---"
    oled_label: str = ""
    description: str = ""


@dataclass(frozen=True)
class LedZoneSettings:
    effect: int = 5
    speed: int = 88
    hue: int = 128
    sat: int = 200
    value: int = 100


@dataclass(frozen=True)
class LedLayerSettings:
    key: LedZoneSettings = field(default_factory=LedZoneSettings)
    gap: LedZoneSettings = field(default_factory=lambda: LedZoneSettings(effect=10, speed=80, hue=128, sat=180, value=28))
    frame: LedZoneSettings = field(default_factory=lambda: LedZoneSettings(effect=1, speed=92, hue=128, sat=200, value=86))


@dataclass(frozen=True)
class LayerProfile:
    id: int
    name: str
    keys: list[KeyAssignment]
    led: LedLayerSettings = field(default_factory=LedLayerSettings)


@dataclass(frozen=True)
class AppProfile:
    name: str = "Default"
    layers: list[LayerProfile] = field(default_factory=list)


class KeycodeCatalog:
    def __init__(self, categories: list[KeycodeCategory], layers: list[LayerInfo], effects: list[LedEffect], code_to_entry: dict[int, KeycodeEntry]):
        self._categories = categories
        self.layers = layers
        self.effects = effects
        self._code_to_entry = code_to_entry

    @classmethod
    def load(cls) -> "KeycodeCatalog":
        catalog_root = json.loads(_load_json_text("keycode-catalog.json"))
        layer_root = json.loads(_load_json_text("layer-definitions.json"))
        led_root = json.loads(_load_json_text("led-presets.json"))

        categories: list[KeycodeCategory] = []
        code_to_entry: dict[int, KeycodeEntry] = {}
        for c in catalog_root.get("categories", []):
            items: list[KeycodeEntry] = []
            for k in c.get("keycodes", []):
                e = KeycodeEntry(
                    code=int(k.get("code", 0)),
                    label=str(k.get("label", "")),
                    display=str(k.get("display", k.get("label", ""))),
                    description=str(k.get("description", "")),
                    category=str(c.get("id", "")),
                )
                items.append(e)
                code_to_entry[e.code] = e
            categories.append(KeycodeCategory(id=str(c.get("id", "")), label=str(c.get("label", "")), keycodes=items))

        layers: list[LayerInfo] = []
        for l in layer_root.get("layers", []):
            layers.append(
                LayerInfo(
                    id=int(l.get("id", 0)),
                    name=str(l.get("name", "")),
                    short_label=str(l.get("shortLabel", "")),
                    display_name=str(l.get("displayName", "")),
                    hue=int(l.get("hue", 0)),
                    sat=int(l.get("sat", 0)),
                    value=int(l.get("val", 0)),
                    via_slot=int(l.get("viaSlot", -1)),
                    visible=bool(l.get("visible", False)),
                    key_legends=[str(x) for x in l.get("keyLegends", [])],
                    key_functions=[str(x) for x in l.get("keyFunctions", [])],
                    encoder_function=str(l.get("encoderFunction", "")),
                )
            )

        effects = [LedEffect(id=int(e.get("id", 0)), label=str(e.get("label", ""))) for e in led_root.get("effects", [])]

        return cls(categories, layers, effects, code_to_entry)

    def get_categories(self) -> list[KeycodeCategory]:
        return self._categories

    def layer_by_id(self, layer_id: int) -> LayerInfo | None:
        for layer in self.layers:
            if layer.id == layer_id:
                return layer
        return None

    def via_layers(self) -> list[LayerInfo]:
        return sorted([l for l in self.layers if l.via_slot >= 0], key=lambda l: l.via_slot)

    def display_label(self, code: int) -> str:
        if code == 0x0000:
            return "---"
        if code == 0x0001:
            return "▽"
        if 0x5700 <= code <= 0x57FF:
            entry = self._code_to_entry.get(code)
            return entry.display if entry is not None else f"TD({code & 0xFF})"
        if 0x4000 <= code <= 0x4FFF:
            layer = (code >> 8) & 0x0F
            base = code & 0xFF
            layer_name = self.layer_by_id(layer).short_label if self.layer_by_id(layer) is not None else str(layer)
            base_name = self._basic_key_display(base) or self.display_label(base)
            return f"LT({layer_name},{base_name})"
        if 0x5100 <= code <= 0x51FF:
            layer = code & 0xFF
            layer_name = self.layer_by_id(layer).short_label if self.layer_by_id(layer) is not None else str(layer)
            return f"MO({layer_name})"
        if 0x5300 <= code <= 0x53FF:
            layer = code & 0xFF
            layer_name = self.layer_by_id(layer).short_label if self.layer_by_id(layer) is not None else str(layer)
            return f"TG({layer_name})"
        if 0x5000 <= code <= 0x50FF:
            layer = code & 0xFF
            layer_name = self.layer_by_id(layer).short_label if self.layer_by_id(layer) is not None else str(layer)
            return f"TO({layer_name})"
        if 0x6000 <= code <= 0x7DFF:
            mod = (code >> 8) & 0x1F
            base = code & 0xFF
            mod_str = self._build_mod_string(mod)
            base_name = self._basic_key_display(base) or f"0x{base:02X}"
            return f"MT({mod_str},{base_name})"
        if 0x0100 <= code <= 0x1FFF:
            mod = (code >> 8) & 0x1F
            base = code & 0xFF
            mod_str = self._build_mod_string(mod)
            base_name = self._basic_key_display(base) or f"0x{base:02X}"
            return f"{mod_str}+{base_name}" if mod_str else f"0x{code:04X}"

        entry = self._code_to_entry.get(code)
        if entry is not None:
            return entry.display
        basic = self._basic_key_display(code)
        if basic is not None:
            return basic
        return f"0x{code:04X}"

    def description_for(self, code: int) -> str:
        entry = self._code_to_entry.get(code)
        if entry is not None and entry.description:
            return entry.description
        if code == 0x0000:
            return "No operation"
        if code == 0x0001:
            return "Transparent - passes through to lower layer"
        return f"Keycode 0x{code:04X}"

    def _build_mod_string(self, mod: int) -> str:
        parts: list[str] = []
        if mod & 0x01:
            parts.append("LCtrl")
        if mod & 0x02:
            parts.append("LShift")
        if mod & 0x04:
            parts.append("LAlt")
        if mod & 0x08:
            parts.append("LWin")
        if mod & 0x10:
            parts.append("RCtrl")
        if mod & 0x20:
            parts.append("RShift")
        return "+".join(parts)

    def _basic_key_display(self, code: int) -> str | None:
        mapping = {
            0x00: "---",
            0x01: "▽",
            0x28: "Enter",
            0x29: "Esc",
            0x2A: "⌫",
            0x2B: "Tab",
            0x2C: "Space",
            0x4F: "→",
            0x50: "←",
            0x51: "↓",
            0x52: "↑",
            0xA0: "Mute",
            0xA1: "Vol+",
            0xA2: "Vol-",
            0xA3: "Next",
            0xA4: "Prev",
            0xA7: "Play",
        }
        if code in mapping:
            return mapping[code]
        if 0x04 <= code <= 0x1D:
            return chr(ord("A") + (code - 0x04))
        if 0x1E <= code <= 0x27:
            return str((code - 0x1E + 1) % 10)
        return None


class ProfileRepository:
    def __init__(self) -> None:
        self._profile_path = profile_store_path()

    def load_default(self) -> AppProfile:
        return self._parse_profile(_load_json_text("default-profile.json"))

    def load_led_presets(self) -> list[LedLayerSettings]:
        root = json.loads(_load_json_text("led-presets.json"))
        out: list[LedLayerSettings] = []
        for slot in root.get("slots", []):
            out.append(
                LedLayerSettings(
                    key=self._parse_zone(slot.get("key", {})),
                    gap=self._parse_zone(slot.get("gap", {})),
                    frame=self._parse_zone(slot.get("frame", {})),
                )
            )
        return out

    def load_profile(self) -> AppProfile:
        if self._profile_path.exists():
            try:
                return self._parse_profile(self._profile_path.read_text(encoding="utf-8"))
            except Exception:
                pass
        return self.load_default()

    def save_profile(self, profile: AppProfile) -> None:
        self._profile_path.parent.mkdir(parents=True, exist_ok=True)
        self._profile_path.write_text(self.export_to_json(profile), encoding="utf-8")

    def export_to_json(self, profile: AppProfile) -> str:
        root: dict[str, object] = {
            "version": "1.0",
            "name": profile.name,
            "layers": [],
        }
        layers = root["layers"]
        assert isinstance(layers, list)
        for layer in profile.layers:
            layer_obj = {
                "id": layer.id,
                "name": layer.name,
                "keys": [
                    {
                        "code": k.code,
                        "label": k.label,
                        "oledLabel": k.oled_label,
                        "description": k.description,
                    }
                    for k in layer.keys
                ],
                "led": {
                    "key": self._serialize_zone(layer.led.key),
                    "gap": self._serialize_zone(layer.led.gap),
                    "frame": self._serialize_zone(layer.led.frame),
                },
            }
            layers.append(layer_obj)
        return json.dumps(root, indent=2)

    def import_from_json(self, text: str) -> AppProfile:
        return self._parse_profile(text)

    def update_key(self, profile: AppProfile, layer_id: int, key_index: int, assignment: KeyAssignment) -> AppProfile:
        updated_layers: list[LayerProfile] = []
        for layer in profile.layers:
            if layer.id == layer_id:
                keys = list(layer.keys)
                if 0 <= key_index < len(keys):
                    keys[key_index] = assignment
                updated_layers.append(LayerProfile(id=layer.id, name=layer.name, keys=keys, led=layer.led))
            else:
                updated_layers.append(layer)
        return AppProfile(name=profile.name, layers=updated_layers)

    def update_led(self, profile: AppProfile, layer_id: int, led: LedLayerSettings) -> AppProfile:
        updated_layers = [
            LayerProfile(id=l.id, name=l.name, keys=l.keys, led=led if l.id == layer_id else l.led)
            for l in profile.layers
        ]
        return AppProfile(name=profile.name, layers=updated_layers)

    def merge_device_keycodes(self, profile: AppProfile, layer_id: int, raw_keycodes: list[int], catalog: KeycodeCatalog) -> AppProfile:
        updated_layers: list[LayerProfile] = []
        for layer in profile.layers:
            if layer.id == layer_id:
                keys = list(layer.keys)
                for i, code in enumerate(raw_keycodes):
                    if i >= len(keys):
                        continue
                    existing = keys[i]
                    label = catalog.display_label(code)
                    if existing.code == code:
                        continue
                    keys[i] = KeyAssignment(
                        code=code,
                        label=label,
                        oled_label=existing.oled_label if existing.oled_label else label[:5],
                        description=catalog.description_for(code),
                    )
                updated_layers.append(LayerProfile(id=layer.id, name=layer.name, keys=keys, led=layer.led))
            else:
                updated_layers.append(layer)
        return AppProfile(name=profile.name, layers=updated_layers)

    def _serialize_zone(self, zone: LedZoneSettings) -> dict[str, int]:
        return {
            "effect": zone.effect,
            "speed": zone.speed,
            "hue": zone.hue,
            "sat": zone.sat,
            "val": zone.value,
        }

    def _parse_zone(self, data: dict[str, object]) -> LedZoneSettings:
        return LedZoneSettings(
            effect=int(data.get("effect", 5)),
            speed=int(data.get("speed", 88)),
            hue=int(data.get("hue", 128)),
            sat=int(data.get("sat", 200)),
            value=int(data.get("val", 100)),
        )

    def _parse_profile(self, text: str) -> AppProfile:
        root = json.loads(text)
        layers: list[LayerProfile] = []
        for l in root.get("layers", []):
            keys = [
                KeyAssignment(
                    code=int(k.get("code", 0)),
                    label=str(k.get("label", "---")),
                    oled_label=str(k.get("oledLabel", "")),
                    description=str(k.get("description", "")),
                )
                for k in l.get("keys", [])
            ]
            while len(keys) < 9:
                keys.append(KeyAssignment())

            led_obj = l.get("led", {})
            led = LedLayerSettings(
                key=self._parse_zone(led_obj.get("key", {})),
                gap=self._parse_zone(led_obj.get("gap", {})),
                frame=self._parse_zone(led_obj.get("frame", {})),
            )
            layers.append(
                LayerProfile(
                    id=int(l.get("id", 0)),
                    name=str(l.get("name", "Layer")),
                    keys=keys,
                    led=led,
                )
            )

        return AppProfile(name=str(root.get("name", "Default")), layers=layers)
