# Desktop Configurator (Portable EXE)

This is the Windows desktop counterpart to the reworked Android configurator.

It speaks QMK VIA over Raw HID and now uses the same shared data model as Android:

- keycode catalog and category-based key editor
- layer definitions (display names, legends, layer-slot mapping)
- profile import/export/reset
- OLED label and description editor with 3x3 preview
- per-layer LED settings editor (key/gap/frame zones)
- default profile and LED presets from shared JSON assets

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

Notes:

- The portable build bundles shared metadata (`default-profile.json`, `keycode-catalog.json`, `layer-definitions.json`, `led-presets.json`) into `resources/`.

## GitHub Actions

A workflow builds a portable Windows artifact:

- `.github/workflows/desktop-app-windows.yml`
