from __future__ import annotations

import os
import sys
from pathlib import Path


def _configure_frozen_import_path() -> None:
    """Prefer bundled modules in frozen builds to avoid local path shadowing."""
    if not getattr(sys, "frozen", False):
        return

    meipass = getattr(sys, "_MEIPASS", "")
    if meipass:
        base = Path(meipass)
        for candidate in (base, base / "PySide6", base / "shiboken6"):
            if candidate.exists():
                candidate_str = str(candidate)
                if candidate_str not in sys.path:
                    sys.path.insert(0, candidate_str)

    # Prevent an arbitrary working directory from shadowing bundled modules.
    try:
        cwd = Path.cwd().resolve()
    except Exception:
        cwd = None

    if cwd is not None:
        filtered: list[str] = []
        for path_item in sys.path:
            try:
                resolved = Path(path_item or ".").resolve()
            except Exception:
                filtered.append(path_item)
                continue
            if resolved == cwd:
                continue
            filtered.append(path_item)
        sys.path[:] = filtered

    # A stale PYTHONPATH can force imports away from bundled dependencies.
    os.environ.pop("PYTHONPATH", None)


_configure_frozen_import_path()

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
