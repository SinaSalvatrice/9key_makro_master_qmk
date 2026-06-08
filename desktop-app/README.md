# Desktop Configurator (Portable EXE)

This is a minimal desktop counterpart to the Android configurator.

It speaks QMK VIA over Raw HID for dynamic keymap access and uses the same keyboard definition JSON.

## Run (dev)

From the repo root:

- `python -m venv .venv`
- Activate it
- `pip install -r desktop-app/requirements.txt`
- `python desktop-app/app.py`

## Build portable Windows EXE

- `pip install -r desktop-app/requirements.txt`
- `pyinstaller desktop-app/pyinstaller-windows.spec`

Output:
- `dist/ninekey-configurator/` (portable folder)

## GitHub Actions

A workflow builds a portable Windows artifact:
- `.github/workflows/desktop-app-windows.yml`
