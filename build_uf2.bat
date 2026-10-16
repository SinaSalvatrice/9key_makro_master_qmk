@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "PROJECT_ROOT=%~dp0"
set "PROJECT_ROOT=%PROJECT_ROOT:~0,-1%"
set "KEYBOARD=9key_makro_master_qmk"
set "KEYMAP=reworked"
set "OUTPUT_DIR=%PROJECT_ROOT%\build"
set "OUTPUT_FILE=%OUTPUT_DIR%\%KEYBOARD%_%KEYMAP%.uf2"

if not defined QMK_HOME set "QMK_HOME=%USERPROFILE%\qmk_firmware"
set "QMK_BASH=C:\QMK_MSYS\usr\bin\bash.exe"

echo.
echo === 9-Key Macro Master - QMK UF2 Build ===
echo Project : %PROJECT_ROOT%
echo QMK     : %QMK_HOME%
echo Target  : %KEYBOARD%:%KEYMAP%
echo.

if not exist "%QMK_HOME%\Makefile" (
    echo ERROR: QMK Firmware wurde nicht gefunden:
    echo   %QMK_HOME%
    echo.
    echo Falls QMK woanders liegt:
    echo   set QMK_HOME=C:\Pfad\zu\qmk_firmware
    exit /b 2
)

if not exist "%QMK_BASH%" (
    echo ERROR: QMK MSYS wurde nicht gefunden:
    echo   %QMK_BASH%
    exit /b 3
)

set "TARGET=%QMK_HOME%\keyboards\%KEYBOARD%"
echo Synchronisiere Firmware nach:
echo   %TARGET%

if not exist "%TARGET%" mkdir "%TARGET%"

robocopy "%PROJECT_ROOT%" "%TARGET%" /MIR /NFL /NDL /NJH /NJS /NC /NS /NP ^
    /XD ".git" ".github" ".venv" ".vscode" "android-app" "build" "artifacts" ^
    >nul
set "ROBOCOPY_RESULT=%ERRORLEVEL%"
if %ROBOCOPY_RESULT% GEQ 8 (
    echo ERROR: Firmware konnte nicht nach QMK kopiert werden. Robocopy-Code: %ROBOCOPY_RESULT%
    exit /b 5
)

rem Windows path C:\foo\bar -> MSYS path /C/foo/bar.
set "QMK_POSIX=%QMK_HOME:\=/%"
set "QMK_DRIVE=%QMK_POSIX:~0,1%"
set "QMK_POSIX=/%QMK_DRIVE%%QMK_POSIX:~2%"

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
del /q "%OUTPUT_FILE%" >nul 2>&1

echo.
echo Baue Firmware...
set "MSYSTEM=MINGW64"
set "CHERE_INVOKING=1"
"%QMK_BASH%" --noprofile --norc -c "export PATH=/mingw64/bin:/usr/bin:/opt/qmk/bin:$PATH; cd '%QMK_POSIX%'; qmk compile -kb %KEYBOARD% -km %KEYMAP%"
set "BUILD_RESULT=%ERRORLEVEL%"

if not "%BUILD_RESULT%"=="0" (
    echo.
    echo ERROR: QMK-Build fehlgeschlagen. Exit-Code: %BUILD_RESULT%
    exit /b %BUILD_RESULT%
)

set "BUILT_UF2=%QMK_HOME%\%KEYBOARD%_%KEYMAP%.uf2"
if not exist "%BUILT_UF2%" (
    for /f "delims=" %%F in ('dir /b /s /a-d "%QMK_HOME%\%KEYBOARD%_%KEYMAP%.uf2" 2^>nul') do if not defined FOUND_UF2 set "FOUND_UF2=%%F"
    if defined FOUND_UF2 set "BUILT_UF2=!FOUND_UF2!"
)

if not exist "%BUILT_UF2%" (
    echo ERROR: Build war erfolgreich, aber die UF2 wurde nicht gefunden.
    exit /b 7
)

copy /y "%BUILT_UF2%" "%OUTPUT_FILE%" >nul
if errorlevel 1 (
    echo ERROR: UF2 konnte nicht in den build-Ordner kopiert werden.
    exit /b 8
)

echo.
echo SUCCESS
echo   %OUTPUT_FILE%
echo.
exit /b 0
