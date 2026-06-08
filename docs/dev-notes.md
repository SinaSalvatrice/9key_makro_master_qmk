# Developer Notes

- The Android app must not load or embed the VIA website.
- The Android configurator must use a local/native UI.
- Communication must happen over USB HID using the keyboard's Raw HID channel.
- Both app and firmware-facing code should rely on the shared keyboard definition file.
- Keep architecture keyboard-specific for the 9-Key Macro Master instead of aiming for generic full VIA support.
