from __future__ import annotations

import sys

from PySide6 import QtWidgets

from ninekey_configurator.ui_main import MainWindow


def main() -> int:
    app = QtWidgets.QApplication(sys.argv)
    win = MainWindow()
    win.resize(520, 520)
    win.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
