from __future__ import annotations

from pathlib import Path
import colorsys
import sys

from PySide6 import QtCore, QtGui, QtWidgets

from .definition import KeyboardDefinition
from .models import (
    AppProfile,
    KeyAssignment,
    KeycodeCatalog,
    KeycodeEntry,
    LayerInfo,
    LedLayerSettings,
    LedZoneSettings,
    ProfileRepository,
)
from .rawhid import RawHidProtocolClient, VIA_CUSTOM_CHANNEL_ID
from .transport_hidapi import HidApiTransport


def _resource_path(relative: str) -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent / relative
    return Path(__file__).resolve().parents[1] / relative


def _load_definition() -> KeyboardDefinition:
    candidates: list[Path] = []
    candidates.append(_resource_path("resources/keyboard-definition.json"))

    meipass = getattr(sys, "_MEIPASS", None)
    if meipass:
        candidates.append(Path(meipass) / "resources" / "keyboard-definition.json")

    if not getattr(sys, "frozen", False):
        candidates.append(Path(__file__).resolve().parents[2] / "shared" / "keyboard-definition.json")

    for p in candidates:
        if p.exists():
            try:
                return KeyboardDefinition.from_json_file(p)
            except (OSError, ValueError):
                continue

    return KeyboardDefinition.default()


def _parse_keycode(text: str) -> int | None:
    t = text.strip()
    if not t:
        return None
    try:
        v = int(t[2:], 16) if t.lower().startswith("0x") else int(t, 10)
    except ValueError:
        return None
    if 0 <= v <= 0xFFFF:
        return v
    return None


class _Worker(QtCore.QRunnable):
    class Signals(QtCore.QObject):
        ok = QtCore.Signal(object)
        err = QtCore.Signal(object)

    def __init__(self, fn):
        super().__init__()
        self.fn = fn
        self.signals = _Worker.Signals()

    @QtCore.Slot()
    def run(self):
        try:
            res = self.fn()
        except Exception as e:  # noqa: BLE001
            self.signals.err.emit(e)
            return
        self.signals.ok.emit(res)


class KeyEditorDialog(QtWidgets.QDialog):
    def __init__(
        self,
        parent: QtWidgets.QWidget,
        catalog: KeycodeCatalog,
        layer_id: int,
        key_index: int,
        current: KeyAssignment,
    ):
        super().__init__(parent)
        self.setWindowTitle(f"Edit Key {key_index + 1}")
        self.resize(540, 520)
        self._catalog = catalog
        self._current = current
        self._selected: KeycodeEntry | None = None
        self._result: KeyAssignment | None = None

        layout = QtWidgets.QVBoxLayout(self)

        self.current_label = QtWidgets.QLabel(f"Current: 0x{current.code:04X} -> {current.label}")
        layout.addWidget(self.current_label)

        top = QtWidgets.QHBoxLayout()
        layout.addLayout(top)

        top.addWidget(QtWidgets.QLabel("Category:"))
        self.category = QtWidgets.QComboBox()
        self._categories = self._catalog.get_categories()
        for c in self._categories:
            self.category.addItem(c.label)
        self.category.currentIndexChanged.connect(self._load_category)
        top.addWidget(self.category, 1)

        top.addWidget(QtWidgets.QLabel("Hex:"))
        self.hex_input = QtWidgets.QLineEdit(f"0x{current.code:04X}")
        top.addWidget(self.hex_input)

        self.hex_apply = QtWidgets.QPushButton("Apply Hex")
        self.hex_apply.clicked.connect(self._apply_hex)
        top.addWidget(self.hex_apply)

        self.list = QtWidgets.QListWidget()
        self.list.currentRowChanged.connect(self._entry_selected)
        layout.addWidget(self.list, 1)

        self.selected_title = QtWidgets.QLabel("Selected: -")
        layout.addWidget(self.selected_title)

        self.selected_desc = QtWidgets.QLabel("Description")
        self.selected_desc.setWordWrap(True)
        layout.addWidget(self.selected_desc)

        oled_row = QtWidgets.QHBoxLayout()
        layout.addLayout(oled_row)
        oled_row.addWidget(QtWidgets.QLabel("OLED Label:"))
        self.oled_input = QtWidgets.QLineEdit(current.oled_label[:5])
        self.oled_input.setMaxLength(5)
        oled_row.addWidget(self.oled_input)

        buttons = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Cancel | QtWidgets.QDialogButtonBox.Ok)
        buttons.accepted.connect(self._accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self._load_category(0)

    def assignment(self) -> KeyAssignment | None:
        return self._result

    def _load_category(self, index: int) -> None:
        self.list.clear()
        if not (0 <= index < len(self._categories)):
            return
        for entry in self._categories[index].keycodes:
            item = QtWidgets.QListWidgetItem(f"{entry.display:<8}  {entry.label}")
            item.setData(QtCore.Qt.UserRole, entry)
            self.list.addItem(item)

    def _entry_selected(self, row: int) -> None:
        item = self.list.item(row)
        if item is None:
            return
        entry = item.data(QtCore.Qt.UserRole)
        if not isinstance(entry, KeycodeEntry):
            return
        self._apply_entry(entry)

    def _apply_entry(self, entry: KeycodeEntry) -> None:
        self._selected = entry
        self.selected_title.setText(f"Selected: {entry.display} ({entry.label})")
        self.selected_desc.setText(entry.description or self._catalog.description_for(entry.code))
        self.hex_input.setText(f"0x{entry.code:04X}")
        if not self.oled_input.text().strip():
            self.oled_input.setText(entry.display[:5])

    def _apply_hex(self) -> None:
        parsed = _parse_keycode(self.hex_input.text())
        if parsed is None:
            QtWidgets.QMessageBox.warning(self, "Invalid", "Enter decimal or 0x0000..0xFFFF")
            return
        label = self._catalog.display_label(parsed)
        self._apply_entry(
            KeycodeEntry(
                code=parsed,
                label=label,
                display=label,
                description=self._catalog.description_for(parsed),
                category="manual",
            )
        )

    def _accept(self) -> None:
        if self._selected is None:
            self._apply_hex()
            if self._selected is None:
                return
        oled = self.oled_input.text().strip()[:5]
        label = self._catalog.display_label(self._selected.code)
        self._result = KeyAssignment(
            code=self._selected.code,
            label=label,
            oled_label=oled if oled else label[:5],
            description=self._selected.description or self._catalog.description_for(self._selected.code),
        )
        self.accept()


class ImportExportDialog(QtWidgets.QDialog):
    def __init__(self, parent: QtWidgets.QWidget, repo: ProfileRepository, profile: AppProfile):
        super().__init__(parent)
        self.setWindowTitle("Import / Export Profile")
        self.resize(720, 560)
        self._repo = repo
        self._profile = profile

        layout = QtWidgets.QVBoxLayout(self)
        self.text = QtWidgets.QPlainTextEdit()
        self.text.setPlainText(self._repo.export_to_json(profile))
        layout.addWidget(self.text, 1)

        row = QtWidgets.QHBoxLayout()
        layout.addLayout(row)
        self.status = QtWidgets.QLabel("Ready")
        row.addWidget(self.status, 1)
        export_btn = QtWidgets.QPushButton("Refresh Export")
        export_btn.clicked.connect(self._do_export)
        row.addWidget(export_btn)
        import_btn = QtWidgets.QPushButton("Import")
        import_btn.clicked.connect(self._do_import)
        row.addWidget(import_btn)
        reset_btn = QtWidgets.QPushButton("Reset Defaults")
        reset_btn.clicked.connect(self._do_reset)
        row.addWidget(reset_btn)

        close = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Close)
        close.rejected.connect(self.reject)
        close.accepted.connect(self.accept)
        layout.addWidget(close)

    def profile(self) -> AppProfile:
        return self._profile

    def _do_export(self) -> None:
        self.text.setPlainText(self._repo.export_to_json(self._profile))
        self.status.setText("Profile exported")

    def _do_import(self) -> None:
        try:
            profile = self._repo.import_from_json(self.text.toPlainText())
        except Exception as exc:  # noqa: BLE001
            self.status.setText(f"Import failed: {exc}")
            return
        self._profile = profile
        self._repo.save_profile(profile)
        self.status.setText(f"Profile imported ({len(profile.layers)} layers)")

    def _do_reset(self) -> None:
        self._profile = self._repo.load_default()
        self._repo.save_profile(self._profile)
        self.text.setPlainText(self._repo.export_to_json(self._profile))
        self.status.setText("Profile reset to defaults")


class OledDialog(QtWidgets.QDialog):
    def __init__(
        self,
        parent: QtWidgets.QWidget,
        layer_info: LayerInfo | None,
        assignments: list[KeyAssignment],
    ):
        super().__init__(parent)
        title = layer_info.display_name if layer_info is not None else "Layer"
        self.setWindowTitle(f"OLED Labels - {title}")
        self.resize(620, 620)

        self._layer_info = layer_info
        self._labels: list[QtWidgets.QLineEdit] = []
        self._descs: list[QtWidgets.QLineEdit] = []

        layout = QtWidgets.QVBoxLayout(self)

        grid = QtWidgets.QGridLayout()
        layout.addLayout(grid, 1)

        for i in range(9):
            row = i // 3
            col = i % 3
            box = QtWidgets.QGroupBox(f"Key {i + 1}: {assignments[i].label}")
            vb = QtWidgets.QVBoxLayout(box)

            l1 = QtWidgets.QLineEdit(assignments[i].oled_label[:5])
            l1.setPlaceholderText("OLED label")
            l1.setMaxLength(5)
            l1.textChanged.connect(self._update_preview)
            vb.addWidget(l1)
            self._labels.append(l1)

            l2 = QtWidgets.QLineEdit(assignments[i].description)
            l2.setPlaceholderText("Description")
            vb.addWidget(l2)
            self._descs.append(l2)

            grid.addWidget(box, row, col)

        self.preview = QtWidgets.QLabel()
        self.preview.setTextInteractionFlags(QtCore.Qt.TextSelectableByMouse)
        layout.addWidget(self.preview)

        row2 = QtWidgets.QHBoxLayout()
        layout.addLayout(row2)
        reset_btn = QtWidgets.QPushButton("Reset To Firmware")
        reset_btn.clicked.connect(self._reset_firmware)
        row2.addWidget(reset_btn)
        row2.addStretch(1)

        buttons = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Cancel | QtWidgets.QDialogButtonBox.Ok)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self._update_preview()

    def result_values(self) -> tuple[list[str], list[str]]:
        return ([w.text().strip()[:5] for w in self._labels], [w.text().strip() for w in self._descs])

    def _reset_firmware(self) -> None:
        if self._layer_info is None:
            return
        for i, label in enumerate(self._labels):
            legend = self._layer_info.key_legends[i] if i < len(self._layer_info.key_legends) else ""
            label.setText(legend[:5])
        for i, desc in enumerate(self._descs):
            text = self._layer_info.key_functions[i] if i < len(self._layer_info.key_functions) else ""
            desc.setText(text)
        self._update_preview()

    def _update_preview(self) -> None:
        labels = [w.text().strip()[:5].ljust(5) for w in self._labels]
        self.preview.setText(
            "\n".join(
                [
                    "OLED Preview",
                    "┌─────┬─────┬─────┐",
                    f"│{labels[0]}│{labels[1]}│{labels[2]}│",
                    "├─────┼─────┼─────┤",
                    f"│{labels[3]}│{labels[4]}│{labels[5]}│",
                    "├─────┼─────┼─────┤",
                    f"│{labels[6]}│{labels[7]}│{labels[8]}│",
                    "└─────┴─────┴─────┘",
                ]
            )
        )


class _LedZoneEditor(QtWidgets.QGroupBox):
    changed = QtCore.Signal()

    def __init__(self, title: str, effects: list[str], initial: LedZoneSettings):
        super().__init__(title)
        self._effects = effects
        v = QtWidgets.QVBoxLayout(self)

        self.effect = QtWidgets.QComboBox()
        self.effect.addItems(effects)
        self.effect.setCurrentIndex(max(0, min(len(effects) - 1, initial.effect)))
        self.effect.currentIndexChanged.connect(self.changed)
        v.addWidget(self.effect)

        self.preview = QtWidgets.QLabel(" ")
        self.preview.setMinimumHeight(20)
        self.preview.setAutoFillBackground(True)
        v.addWidget(self.preview)

        self.hue = self._slider(v, "Hue", initial.hue)
        self.sat = self._slider(v, "Sat", initial.sat)
        self.val = self._slider(v, "Val", initial.value)
        self.speed = self._slider(v, "Speed", initial.speed)

        self._refresh_preview()

    def _slider(self, parent: QtWidgets.QVBoxLayout, title: str, value: int) -> QtWidgets.QSlider:
        row = QtWidgets.QHBoxLayout()
        parent.addLayout(row)
        row.addWidget(QtWidgets.QLabel(title), 1)
        slider = QtWidgets.QSlider(QtCore.Qt.Horizontal)
        slider.setRange(0, 255)
        slider.setValue(value)
        label = QtWidgets.QLabel(str(value))
        slider.valueChanged.connect(lambda n: label.setText(str(n)))
        slider.valueChanged.connect(lambda _: self._refresh_preview())
        slider.valueChanged.connect(self.changed)
        row.addWidget(slider, 5)
        row.addWidget(label, 1)
        return slider

    def settings(self) -> LedZoneSettings:
        return LedZoneSettings(
            effect=self.effect.currentIndex(),
            speed=self.speed.value(),
            hue=self.hue.value(),
            sat=self.sat.value(),
            value=self.val.value(),
        )

    def _refresh_preview(self) -> None:
        h = self.hue.value() / 255.0
        s = self.sat.value() / 255.0
        v = self.val.value() / 255.0
        r, g, b = colorsys.hsv_to_rgb(h, s, v)
        pal = self.preview.palette()
        pal.setColor(self.preview.backgroundRole(), QtCore.Qt.GlobalColor.black)
        pal.setColor(self.preview.foregroundRole(), QtCore.Qt.GlobalColor.white)
        pal.setColor(QtGui.QPalette.Window, QtGui.QColor(int(r * 255), int(g * 255), int(b * 255)))
        self.preview.setPalette(pal)


class LedDialog(QtWidgets.QDialog):
    def __init__(self, parent: QtWidgets.QWidget, effects: list[str], initial: LedLayerSettings):
        super().__init__(parent)
        self.setWindowTitle("LED Settings")
        self.resize(700, 560)

        layout = QtWidgets.QVBoxLayout(self)
        row = QtWidgets.QHBoxLayout()
        layout.addLayout(row, 1)

        self.key = _LedZoneEditor("Key LEDs", effects, initial.key)
        row.addWidget(self.key)
        self.gap = _LedZoneEditor("Gap LEDs", effects, initial.gap)
        row.addWidget(self.gap)
        self.frame = _LedZoneEditor("Frame LEDs", effects, initial.frame)
        row.addWidget(self.frame)

        buttons = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Cancel | QtWidgets.QDialogButtonBox.Ok)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def settings(self) -> LedLayerSettings:
        return LedLayerSettings(key=self.key.settings(), gap=self.gap.settings(), frame=self.frame.settings())


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("9-Key Macro Master Configurator")

        self._definition = _load_definition()
        self._catalog = KeycodeCatalog.load()
        self._repo = ProfileRepository()
        self._profile = self._repo.load_profile()
        self._via_layers = self._catalog.via_layers()

        self._transport: HidApiTransport | None = None
        self._client: RawHidProtocolClient | None = None
        self._current_layer = 0
        self._keycodes = [
            [[0 for _ in range(self._definition.cols)] for _ in range(self._definition.rows)]
            for _ in range(max(1, self._definition.layers))
        ]

        self._pool = QtCore.QThreadPool.globalInstance()
        self._active_workers: set[_Worker] = set()

        central = QtWidgets.QWidget()
        self.setCentralWidget(central)
        layout = QtWidgets.QVBoxLayout(central)

        self.info = QtWidgets.QLabel("Keyboard metadata")
        self.info.setWordWrap(True)
        layout.addWidget(self.info)

        row = QtWidgets.QHBoxLayout()
        layout.addLayout(row)

        self.connect_btn = QtWidgets.QPushButton("Connect USB Keyboard")
        self.connect_btn.clicked.connect(self._connect_clicked)
        row.addWidget(self.connect_btn, 3)

        self.layer_combo = QtWidgets.QComboBox()
        self.layer_combo.currentIndexChanged.connect(self._layer_changed)
        row.addWidget(self.layer_combo, 2)

        row2 = QtWidgets.QHBoxLayout()
        layout.addLayout(row2)
        self.refresh_btn = QtWidgets.QPushButton("Refresh Layer")
        self.refresh_btn.clicked.connect(self._refresh_clicked)
        row2.addWidget(self.refresh_btn)
        self.save_btn = QtWidgets.QPushButton("Save EEPROM")
        self.save_btn.clicked.connect(self._save_clicked)
        row2.addWidget(self.save_btn)
        self.apply_profile_btn = QtWidgets.QPushButton("Apply Profile To Keyboard")
        self.apply_profile_btn.clicked.connect(self._apply_profile_clicked)
        row2.addWidget(self.apply_profile_btn)

        row3 = QtWidgets.QHBoxLayout()
        layout.addLayout(row3)
        self.led_btn = QtWidgets.QPushButton("LED Settings")
        self.led_btn.clicked.connect(self._led_clicked)
        row3.addWidget(self.led_btn)
        self.oled_btn = QtWidgets.QPushButton("OLED Labels")
        self.oled_btn.clicked.connect(self._oled_clicked)
        row3.addWidget(self.oled_btn)
        self.import_export_btn = QtWidgets.QPushButton("Import/Export")
        self.import_export_btn.clicked.connect(self._import_export_clicked)
        row3.addWidget(self.import_export_btn)

        self.grid = QtWidgets.QGridLayout()
        layout.addLayout(self.grid)

        self.key_buttons: list[list[QtWidgets.QPushButton]] = []
        for r in range(self._definition.rows):
            row_buttons: list[QtWidgets.QPushButton] = []
            for c in range(self._definition.cols):
                btn = QtWidgets.QPushButton("---")
                btn.setMinimumHeight(64)
                btn.clicked.connect(lambda _=False, rr=r, cc=c: self._edit_key(rr, cc))
                self.grid.addWidget(btn, r, c)
                row_buttons.append(btn)
            self.key_buttons.append(row_buttons)

        self.status = QtWidgets.QLabel("Status: ready")
        layout.addWidget(self.status)

        self._update_layer_combo()
        self._update_keyboard_info_text()
        self._render_layer(0)

    def _start_worker(self, work_fn, on_ok, on_err) -> None:
        worker = _Worker(work_fn)
        self._active_workers.add(worker)

        def done_ok(res):
            self._active_workers.discard(worker)
            on_ok(res)

        def done_err(exc):
            self._active_workers.discard(worker)
            on_err(exc)

        worker.signals.ok.connect(done_ok)
        worker.signals.err.connect(done_err)
        self._pool.start(worker)

    def closeEvent(self, event):  # noqa: N802
        self._close_connection()
        return super().closeEvent(event)

    def _set_status(self, msg: str) -> None:
        self.status.setText(f"Status: {msg}")

    def _update_layer_combo(self) -> None:
        self.layer_combo.blockSignals(True)
        self.layer_combo.clear()
        if self._via_layers:
            for layer in self._via_layers:
                self.layer_combo.addItem(f"{layer.display_name} [{layer.short_label}]")
        else:
            for i in range(max(1, self._definition.layers)):
                self.layer_combo.addItem(f"Layer {i}")
        self.layer_combo.setCurrentIndex(0)
        self.layer_combo.blockSignals(False)

    def _current_layer_info(self) -> LayerInfo | None:
        if 0 <= self._current_layer < len(self._via_layers):
            return self._via_layers[self._current_layer]
        return None

    def _current_firmware_layer_id(self) -> int:
        info = self._current_layer_info()
        return info.id if info is not None else self._current_layer

    def _layer_assignments(self, firmware_layer_id: int) -> list[KeyAssignment]:
        for layer in self._profile.layers:
            if layer.id == firmware_layer_id:
                return list(layer.keys[:9]) + [KeyAssignment()] * max(0, 9 - len(layer.keys))
        return [KeyAssignment() for _ in range(9)]

    def _layer_led(self, firmware_layer_id: int) -> LedLayerSettings:
        for layer in self._profile.layers:
            if layer.id == firmware_layer_id:
                return layer.led
        return LedLayerSettings()

    def _layer_profile(self, firmware_layer_id: int):
        for layer in self._profile.layers:
            if layer.id == firmware_layer_id:
                return layer
        return None

    def _update_keyboard_info_text(self) -> None:
        d = self._definition
        self.info.setText(
            "\n".join(
                [
                    f"Keyboard: {d.display_name}",
                    f"Matrix: {d.rows}x{d.cols}",
                    f"Layers: {d.layers}",
                    f"Transport: {d.transport}",
                ]
            )
        )

    def _close_connection(self) -> None:
        try:
            if self._client is not None:
                self._client.close()
        finally:
            self._client = None
            self._transport = None

    def _connect_clicked(self) -> None:
        # Drop any stale handle before attempting a new HID session.
        self._close_connection()
        self._set_status("Connecting...")

        def work():
            refs = HidApiTransport.enumerate(self._definition.vendor_id, self._definition.product_id)
            if not refs:
                return (False, "No compatible HID device found")

            last_error = "Connected but VIA handshake failed"
            for idx, ref in enumerate(refs):
                transport = HidApiTransport.open_from_ref(ref, self._definition.packet_size)
                if transport is None:
                    continue

                client = RawHidProtocolClient(transport, self._definition.packet_size)
                handshake_ok = client.handshake()
                info = client.get_info() if handshake_ok else None
                if handshake_ok:
                    return (True, transport, client, info)

                last_error = f"VIA handshake failed on HID interface {idx}"
                client.close()

            return (False, last_error)

        def on_ok(res):
            if not isinstance(res, tuple) or not res:
                self._set_status("Unexpected result")
                return
            if res[0] is False:
                self._set_status(str(res[1]))
                return

            _, transport, client, info = res
            self._transport = transport
            self._client = client

            if info is not None:
                self.info.setText(
                    "\n".join(
                        [
                            f"Keyboard: {self._definition.display_name}",
                            f"Matrix: {self._definition.rows}x{self._definition.cols}",
                            f"Layers: {self._definition.layers}",
                            "Transport: via/raw_hid",
                            f"ID: {info.keyboard_id}",
                        ]
                    )
                )

            self._set_status("Connected: VIA OK")
            self._load_layer(self._current_layer)

        def on_err(exc):
            self._set_status(f"Error: {exc}")

        self._start_worker(work, on_ok, on_err)

    def _layer_changed(self, index: int) -> None:
        if index < 0:
            return
        self._current_layer = index
        self._render_layer(index)
        self._load_layer(index)

    def _refresh_clicked(self) -> None:
        self._load_layer(self._current_layer)

    def _save_clicked(self) -> None:
        client = self._client
        if client is None:
            self._set_status("Not connected")
            return

        self._set_status("Saving EEPROM...")

        def work():
            return client.save_eeprom()

        def on_ok(res):
            self._set_status("EEPROM saved" if res else "EEPROM save failed")

        def on_err(exc):
            self._set_status(f"Error: {exc}")

        self._start_worker(work, on_ok, on_err)

    def _load_layer(self, layer: int) -> None:
        client = self._client
        if client is None:
            self._render_layer(layer)
            self._set_status("Not connected")
            return

        self._set_status(f"Loading layer {layer}...")

        def work():
            rows = self._definition.rows
            cols = self._definition.cols
            keys = [[0 for _ in range(cols)] for _ in range(rows)]
            for r in range(rows):
                for c in range(cols):
                    v = client.get_key(layer, r, c)
                    if v is None:
                        raise RuntimeError(f"GET_KEY failed at r={r} c={c}")
                    keys[r][c] = v
            return keys

        def on_ok(keys):
            self._keycodes[layer] = keys
            raw = [keys[i // 3][i % 3] for i in range(9)]
            firmware_layer = self._current_firmware_layer_id()
            self._profile = self._repo.merge_device_keycodes(self._profile, firmware_layer, raw, self._catalog)
            self._repo.save_profile(self._profile)
            self._render_layer(layer)
            self._set_status(f"Layer {layer} loaded")

        def on_err(exc):
            self._set_status(f"Error: {exc}")

        self._start_worker(work, on_ok, on_err)

    def _render_layer(self, layer: int) -> None:
        info = self._current_layer_info()
        firmware_layer = self._current_firmware_layer_id()
        assignments = self._layer_assignments(firmware_layer)

        rows = self._definition.rows
        cols = self._definition.cols
        for r in range(rows):
            for c in range(cols):
                idx = r * 3 + c
                keycode = self._keycodes[layer][r][c]
                assignment = assignments[idx]
                if assignment.label and assignment.label != "---" and assignment.code == keycode:
                    text = assignment.label
                elif keycode != 0:
                    text = self._catalog.display_label(keycode)
                else:
                    legend = info.key_legends[idx] if info is not None and idx < len(info.key_legends) else "---"
                    text = legend if legend else "---"
                self.key_buttons[r][c].setText(text)

    def _edit_key(self, row: int, col: int) -> None:
        if self._client is None:
            self._set_status("Not connected")
            return

        idx = row * 3 + col
        firmware_layer = self._current_firmware_layer_id()
        assignments = self._layer_assignments(firmware_layer)
        current = assignments[idx]
        if current.code == 0:
            current = KeyAssignment(
                code=self._keycodes[self._current_layer][row][col],
                label=self._catalog.display_label(self._keycodes[self._current_layer][row][col]),
                oled_label="",
                description=self._catalog.description_for(self._keycodes[self._current_layer][row][col]),
            )

        dlg = KeyEditorDialog(self, self._catalog, firmware_layer, idx, current)
        if dlg.exec() != QtWidgets.QDialog.Accepted:
            return
        assignment = dlg.assignment()
        if assignment is None:
            return

        self._profile = self._repo.update_key(self._profile, firmware_layer, idx, assignment)
        self._repo.save_profile(self._profile)
        self._apply_keycode(self._current_layer, row, col, assignment.code)

    def _apply_keycode(self, layer: int, row: int, col: int, keycode: int) -> None:
        client = self._client
        if client is None:
            return

        self._set_status("Setting key...")

        def work():
            return client.set_key(layer, row, col, keycode)

        def on_ok(res):
            if res:
                self._keycodes[layer][row][col] = keycode
                self._render_layer(self._current_layer)
                self._set_status("Key updated")
            else:
                self._set_status("SET_KEY failed")

        def on_err(exc):
            self._set_status(f"Error: {exc}")

        self._start_worker(work, on_ok, on_err)

    def _import_export_clicked(self) -> None:
        dlg = ImportExportDialog(self, self._repo, self._profile)
        dlg.exec()
        previous = self._profile
        self._profile = dlg.profile()
        self._render_layer(self._current_layer)
        if self._client is not None and self._profile is not previous:
            self._apply_profile_clicked()

    def _oled_clicked(self) -> None:
        firmware_layer = self._current_firmware_layer_id()
        info = self._current_layer_info()
        assignments = self._layer_assignments(firmware_layer)

        dlg = OledDialog(self, info, assignments)
        if dlg.exec() != QtWidgets.QDialog.Accepted:
            return

        labels, descs = dlg.result_values()
        for i in range(9):
            existing = assignments[i]
            updated = KeyAssignment(
                code=existing.code,
                label=existing.label,
                oled_label=labels[i],
                description=descs[i],
            )
            self._profile = self._repo.update_key(self._profile, firmware_layer, i, updated)
        self._repo.save_profile(self._profile)
        self._render_layer(self._current_layer)

    def _led_clicked(self) -> None:
        try:
            firmware_layer = self._current_firmware_layer_id()
            led = self._layer_led(firmware_layer)
            effects = [e.label for e in self._catalog.effects]
            dlg = LedDialog(self, effects, led)
            if dlg.exec() != QtWidgets.QDialog.Accepted:
                return
            settings = dlg.settings()
            self._profile = self._repo.update_led(self._profile, firmware_layer, settings)
            self._repo.save_profile(self._profile)
            if self._client is not None:
                self._apply_led_settings_to_device(firmware_layer, settings)
            self._set_status("LED settings saved to profile and applied to firmware")
        except Exception as exc:  # noqa: BLE001
            self._set_status(f"LED settings error: {exc}")
            QtWidgets.QMessageBox.critical(self, "LED Settings", f"Failed to open LED settings: {exc}")

    def _apply_led_settings_to_device(self, firmware_layer_id: int, settings: LedLayerSettings) -> None:
        client = self._client
        if client is None:
            return

        slot = None
        for idx, layer in enumerate(self._via_layers):
            if layer.id == firmware_layer_id:
                slot = idx
                break
        if slot is None:
            raise RuntimeError(f"No VIA slot mapped for layer {firmware_layer_id}")

        zone_map = [
            (settings.key, 3, 4, 5, 6),
            (settings.frame, 7, 8, 9, 10),
            (settings.gap, 11, 12, 13, 14),
        ]

        def pack_color(zone: LedZoneSettings) -> bytes:
            return bytes([slot, zone.hue & 0xFF, zone.sat & 0xFF])

        def pack_single(zone: LedZoneSettings, mode: str) -> bytes:
            value = zone.effect if mode == "effect" else zone.speed if mode == "speed" else zone.value
            return bytes([slot, value & 0xFF])

        for zone, color_id, brightness_id, effect_id, speed_id in zone_map:
            writes = [
                (color_id, pack_color(zone)),
                (brightness_id, pack_single(zone, "value")),
                (effect_id, pack_single(zone, "effect")),
                (speed_id, pack_single(zone, "speed")),
            ]
            for value_id, payload in writes:
                if not client.custom_set_value(VIA_CUSTOM_CHANNEL_ID, value_id, payload):
                    raise RuntimeError(f"LED set failed for value {value_id}")

                verify = client.custom_get_value(VIA_CUSTOM_CHANNEL_ID, value_id, bytes([slot]))
                if verify is None or len(verify) < len(payload):
                    raise RuntimeError(f"LED verify failed for value {value_id}")

                if verify[0] != slot:
                    raise RuntimeError(f"LED verify slot mismatch for value {value_id}")

                if verify[: len(payload)] != payload:
                    raise RuntimeError(
                        f"LED verify mismatch for value {value_id}: expected {payload.hex()}, got {verify[:len(payload)].hex()}"
                    )

        if not client.custom_save(VIA_CUSTOM_CHANNEL_ID):
            raise RuntimeError("LED custom save failed")
        return

    def _apply_profile_clicked(self) -> None:
        client = self._client
        if client is None:
            self._set_status("Not connected")
            return

        self._set_status("Applying profile to keyboard...")

        def work():
            writes = 0
            for via_slot, layer in enumerate(self._via_layers):
                assignments = self._layer_assignments(layer.id)
                for r in range(self._definition.rows):
                    for c in range(self._definition.cols):
                        idx = r * self._definition.cols + c
                        code = assignments[idx].code if idx < len(assignments) else 0
                        ok = client.set_key(via_slot, r, c, code)
                        if not ok:
                            raise RuntimeError(f"SET_KEY failed at via_slot={via_slot} r={r} c={c}")
                        reads = client.get_key(via_slot, r, c)
                        if reads is None:
                            raise RuntimeError(f"GET_KEY verify failed at via_slot={via_slot} r={r} c={c}")
                        if reads != code:
                            raise RuntimeError(
                                f"Verify mismatch at via_slot={via_slot} r={r} c={c}: expected 0x{code:04X}, got 0x{reads:04X}"
                            )
                        writes += 1

                layer_profile = self._layer_profile(layer.id)
                if layer_profile is not None:
                    self._apply_led_settings_to_device(layer.id, layer_profile.led)
            return writes

        def on_ok(count):
            self._load_layer(self._current_layer)
            self._set_status(f"Applied {count} key assignments and LED settings to keyboard")

        def on_err(exc):
            self._set_status(f"Apply failed: {exc}")

        self._start_worker(work, on_ok, on_err)


