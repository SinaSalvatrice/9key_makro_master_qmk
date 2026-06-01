# Android App (Native Configurator)

This folder now contains the initial native Android scaffold for the 9-Key Macro Master configurator.

## Scope

- Keyboard-specific app for 9key_makro_master
- Local/native UI only (no VIA website, no web embed)
- USB HID / Raw HID transport
- Uses the shared keyboard metadata model

## Current Status

- Android Gradle project structure created
- App module created with Kotlin + XML UI
- Static 3x3 key grid placeholder screen added
- Keyboard definition asset loading wired in `MainActivity`
- HID protocol communication not implemented yet

## Structure

- `app/src/main/java/.../MainActivity.kt`: startup screen + metadata load
- `app/src/main/res/layout/activity_main.xml`: fixed 3x3 layout placeholder
- `app/src/main/assets/keyboard-definition.json`: keyboard metadata for app runtime

## Next Work Items

1. Add USB device detection and permission flow.
2. Implement Raw HID packet transport service.
3. Implement protocol client for `PING` and `GET_INFO`.
4. Replace placeholder buttons with dynamic per-layer key state.
5. Add key edit flow backed by `GET_KEY` and `SET_KEY`.
6. Add explicit save action for `SAVE_EEPROM`.

## Notes

- Keep protocol behavior aligned with `../protocol/raw-hid-protocol.md`.
- Keep keyboard metadata aligned with `../shared/keyboard-definition.json`.
