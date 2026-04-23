9key_makro_master_qmk safe EEPROM + VIA RGB effects patch

Changes:
- VIA RGB Effect dropdown: Wild, Breathing, Running, Twinkle, Pulse, Solid, Off.
- Safe EEPROM clear combo: Encoder button + GP12 held for 3000 ms.
- Uses eeconfig_init() instead of eeconfig_disable().
- Rewrites VIA custom config defaults after EEPROM init.
- Robust VIA config validation: signature, oled_view, fx_mode, rgb_effect.
- Optional FORCE_EEPROM_RESET_ON_BOOT recovery switch in config.h.

Recovery workflow:
1. If device is weird/dead after VIA config changes, uncomment FORCE_EEPROM_RESET_ON_BOOT in config.h.
2. Flash once and let the board boot once.
3. Comment FORCE_EEPROM_RESET_ON_BOOT again.
4. Flash normally.

Note:
Warnings like layer_name_long defined but not used are not fatal unless warnings are treated as errors.
If build still stops at quantum/keymap_introspection.c, the real error is hidden above the shown warning.
