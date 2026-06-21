$ErrorActionPreference = 'Stop'

# From repo root:
#   .\desktop-app\build-portable.ps1

python -m pip install --upgrade pip
pip install -r desktop-app/requirements.txt

pyinstaller desktop-app/pyinstaller-windows.spec

Write-Host "Built: dist/ninekey-configurator/" -ForegroundColor Green
