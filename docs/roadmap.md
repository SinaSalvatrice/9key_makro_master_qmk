# Roadmap

## Phase 1: Structure + Docs

- Create firmware/android-app/shared/protocol/docs folders.
- Add shared keyboard definition.
- Document custom Raw HID protocol and implementation plan.

## Phase 2: QMK Raw HID PING/GET_INFO

- Add keyboard-side Raw HID handlers for PING and GET_INFO.
- Return keyboard identity and matrix metadata.

## Phase 3: GET_KEY/SET_KEY + EEPROM

- Implement key read/write commands.
- Add explicit save-to-EEPROM command path.
- Validate boundaries for layer, row, and column indices.

## Phase 4: Android Fixed 3x3 UI

- Build a local/native Android UI for fixed 3x3 grid editing.
- Integrate USB HID transport and protocol client.

## Phase 5: Encoder Config + Import/Export

- Add encoder behavior configuration.
- Add local import/export for keyboard config snapshots.
