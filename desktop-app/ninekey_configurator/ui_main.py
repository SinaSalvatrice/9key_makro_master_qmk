from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import sys

from PySide6 import QtCore, QtWidgets

from .definition import KeyboardDefinition
from .rawhid import RawHidProtocolClient
from .transport_hidapi import HidApiTransport


def _resource_path(relative: str) -> Path:
    # Prefer a portable layout: resources next to the executable.
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent / relative

    # Dev mode: repo layout.
    return Path(__file__).resolve().parents[1] / relative


def _load_definition() -> KeyboardDefinition:
    candidates: list[Path] = []

    # Primary expected path in both dev and frozen one-folder layouts.
    candidates.append(_resource_path("resources/keyboard-definition.json"))

    # PyInstaller one-file extraction temp directory.
    meipass = getattr(sys, "_MEIPASS", None)
    if meipass:
        candidates.append(Path(meipass) / "resources" / "keyboard-definition.json")

    # When the executable gets moved away from its dist folder, try nearby locations.
    exe_dir = Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else None
    if exe_dir is not None:
        candidates.append(exe_dir / "keyboard-definition.json")

    for p in candidates:
        if p.exists():
            return KeyboardDefinition.from_json_file(p)

    return KeyboardDefinition.default()


def _parse_keycode(text: str) -> int | None:
    t = text.strip()
    if not t:
        return None

    try:
        if t.lower().startswith("0x"):
            v = int(t[2:], 16)
        else:
            v = int(t, 10)
    except ValueError:
        return None

    if 0 <= v <= 0xFFFF:
        return v
    return None


class _Worker(QtCore.QRunnable):
    def __init__(self, fn, on_ok, on_err):
        super().__init__()
        self.fn = fn
        self.on_ok = on_ok
        self.on_err = on_err

    @QtCore.Slot()
    def run(self):
        try:
            res = self.fn()
        except Exception as e:  # noqa: BLE001
            QtCore.QMetaObject.invokeMethod(self.on_err, "emit", QtCore.Qt.QueuedConnection, QtCore.Q_ARG(object, e))
            return

        QtCore.QMetaObject.invokeMethod(self.on_ok, "emit", QtCore.Qt.QueuedConnection, QtCore.Q_ARG(object, res))


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()

        self.setWindowTitle("9-Key Macro Master Configurator")

        self._definition = _load_definition()

        self._transport: HidApiTransport | None = None
        self._client: RawHidProtocolClient | None = None
        self._current_layer = 0
        self._keycodes = [
            [[0 for _ in range(self._definition.cols)] for _ in range(self._definition.rows)]
            for _ in range(max(1, self._definition.layers))
        ]

        self._pool = QtCore.QThreadPool.globalInstance()

        central = QtWidgets.QWidget()
        self.setCentralWidget(central)
        layout = QtWidgets.QVBoxLayout(central)

        self.title = QtWidgets.QLabel("9-Key Macro Master Configurator")
        font = self.title.font()
        font.setPointSize(font.pointSize() + 4)
        font.setBold(True)
        self.title.setFont(font)
        layout.addWidget(self.title)

        self.info = QtWidgets.QLabel("Keyboard metadata")
        self.info.setWordWrap(True)
        layout.addWidget(self.info)

        row = QtWidgets.QHBoxLayout()
        layout.addLayout(row)

        self.connect_btn = QtWidgets.QPushButton("Connect USB Keyboard")
        self.connect_btn.clicked.connect(self._connect_clicked)
        row.addWidget(self.connect_btn, 3)

        self.layer_combo = QtWidgets.QComboBox()
        for i in range(max(1, self._definition.layers)):
            self.layer_combo.addItem(f"Layer {i}")
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

        self.grid = QtWidgets.QGridLayout()
        layout.addLayout(self.grid)

        self.key_buttons: list[list[QtWidgets.QPushButton]] = []
        for r in range(self._definition.rows):
            row_buttons: list[QtWidgets.QPushButton] = []
            for c in range(self._definition.cols):
                btn = QtWidgets.QPushButton("0x0000")
                btn.setMinimumHeight(44)
                btn.clicked.connect(lambda _=False, rr=r, cc=c: self._edit_key(rr, cc))
                self.grid.addWidget(btn, r, c)
                row_buttons.append(btn)
            self.key_buttons.append(row_buttons)

        self.status = QtWidgets.QLabel("Status: ready")
        layout.addWidget(self.status)

        self._update_keyboard_info_text()
        self._render_layer(0)

    def closeEvent(self, event):  # noqa: N802
        self._close_connection()
        return super().closeEvent(event)

    def _set_status(self, msg: str) -> None:
        self.status.setText(f"Status: {msg}")

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
        self._set_status("Connecting...")

        class Ok(QtCore.QObject):
            sig = QtCore.Signal(object)

        class Err(QtCore.QObject):
            sig = QtCore.Signal(object)

        ok = Ok()
        err = Err()

        def work():
            transport = HidApiTransport.open_first(self._definition.vendor_id, self._definition.product_id, self._definition.packet_size)
            if transport is None:
                return (False, "No compatible HID device found")
            client = RawHidProtocolClient(transport, self._definition.packet_size)
            ping_ok = client.ping()
            info = client.get_info()
            if not ping_ok:
                client.close()
                return (False, "Connected but PING failed")
            return (True, transport, client, info)

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
                            f"Matrix: {info.rows}x{info.cols}",
                            f"Layers: {info.layers}",
                            "Transport: raw_hid",
                            f"ID: {info.keyboard_id}",
                        ]
                    )
                )

            self._set_status("Connected: PING OK")
            self._load_layer(self._current_layer)

        def on_err(exc):
            self._set_status(f"Error: {exc}")

        ok.sig.connect(on_ok)
        err.sig.connect(on_err)
        self._pool.start(_Worker(work, ok.sig, err.sig))

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

        class Ok(QtCore.QObject):
            sig = QtCore.Signal(object)

        class Err(QtCore.QObject):
            sig = QtCore.Signal(object)

        ok = Ok()
        err = Err()

        def work():
            return client.save_eeprom()

        def on_ok(res):
            self._set_status("EEPROM saved" if res else "EEPROM save failed")

        def on_err(exc):
            self._set_status(f"Error: {exc}")

        ok.sig.connect(on_ok)
        err.sig.connect(on_err)
        self._pool.start(_Worker(work, ok.sig, err.sig))

    def _load_layer(self, layer: int) -> None:
        client = self._client
        if client is None:
            self._render_layer(layer)
            self._set_status("Not connected")
            return

        self._set_status(f"Loading layer {layer}...")

        class Ok(QtCore.QObject):
            sig = QtCore.Signal(object)

        class Err(QtCore.QObject):
            sig = QtCore.Signal(object)

        ok = Ok()
        err = Err()

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
            self._render_layer(layer)
            self._set_status(f"Layer {layer} loaded")

        def on_err(exc):
            self._set_status(f"Error: {exc}")

        ok.sig.connect(on_ok)
        err.sig.connect(on_err)
        self._pool.start(_Worker(work, ok.sig, err.sig))

    def _render_layer(self, layer: int) -> None:
        rows = self._definition.rows
        cols = self._definition.cols
        for r in range(rows):
            for c in range(cols):
                v = self._keycodes[layer][r][c]
                self.key_buttons[r][c].setText(f"0x{v:04X}")

    def _edit_key(self, row: int, col: int) -> None:
        client = self._client
        if client is None:
            self._set_status("Not connected")
            return

        current = self._keycodes[self._current_layer][row][col]
        text, ok = QtWidgets.QInputDialog.getText(
            self,
            "Edit keycode",
            f"Layer {self._current_layer}, row {row}, col {col}\nEnter keycode (decimal or 0xFFFF):",
            text=f"0x{current:04X}",
        )
        if not ok:
            return

        parsed = _parse_keycode(text)
        if parsed is None:
            QtWidgets.QMessageBox.warning(self, "Invalid", "Keycode must be 0..65535 (decimal) or 0x0000..0xFFFF")
            return

        self._set_status("Setting key...")

        class Ok(QtCore.QObject):
            sig = QtCore.Signal(object)

        class Err(QtCore.QObject):
            sig = QtCore.Signal(object)

        ok_sig = Ok()
        err_sig = Err()

        def work():
            return client.set_key(self._current_layer, row, col, parsed)

        def on_ok(res):
            if res:
                self._keycodes[self._current_layer][row][col] = parsed
                self._render_layer(self._current_layer)
                self._set_status("Key updated")
            else:
                self._set_status("SET_KEY failed")

        def on_err(exc):
            self._set_status(f"Error: {exc}")

        ok_sig.sig.connect(on_ok)
        err_sig.sig.connect(on_err)
        self._pool.start(_Worker(work, ok_sig.sig, err_sig.sig))
