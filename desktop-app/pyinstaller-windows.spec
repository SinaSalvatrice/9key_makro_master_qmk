# PyInstaller spec for a portable (one-folder) Windows build.
# Build:
#   pyinstaller desktop-app/pyinstaller-windows.spec

from pathlib import Path
from PyInstaller.utils.hooks import collect_dynamic_libs

block_cipher = None

# NOTE: In PyInstaller's spec execution environment, __file__ may be undefined.
# PyInstaller injects SPECPATH (directory containing this spec file).
spec_dir = Path(globals().get("SPECPATH", ".")).resolve()

entry = spec_dir / "app.py"

resources = spec_dir / "resources" / "keyboard-definition.json"

# Destination inside dist folder: resources/keyboard-definition.json
added_files = [(str(resources), str(Path("resources") / "keyboard-definition.json"))]


a = Analysis(
    [str(entry)],
    pathex=[str(spec_dir)],
    binaries=collect_dynamic_libs('hid'),
    datas=added_files,
    hiddenimports=[],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name="ninekey-configurator",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)

coll = COLLECT(
    exe,
    a.binaries,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name="ninekey-configurator",
)
