#!/usr/bin/env python3
from pathlib import Path
import re
import sys

path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("keymap.c")
s = path.read_text(encoding="utf-8")

# 1) Replace comment
s = s.replace(
    "// - GP12 toggles OLED Legend <-> Last Key view",
    "// - OLED_TOGGLE_BTN_PIN toggles OLED Legend <-> Last Key view"
)

# 2) Add OLED_TOGGLE_BTN_PIN default after ENCODER_HELP_SHOW_MS
needle = "#define ENCODER_HELP_SHOW_MS 2500\n"
insert = needle + "\n#ifndef OLED_TOGGLE_BTN_PIN\n#    define OLED_TOGGLE_BTN_PIN GP12\n#endif\n"
if "OLED_TOGGLE_BTN_PIN" not in s:
    if needle not in s:
        raise SystemExit("Could not find ENCODER_HELP_SHOW_MS define.")
    s = s.replace(needle, insert, 1)

# 3) Replace vsc_chat_macros array
new_macros = """static const char *const vsc_chat_macros[6] = {
    "Explain this QMK error or build output. Identify the most likely root cause, the affected file or setting, why it happens, and the safest fix. Include the exact command or file to check next. Do not modify files automatically.",

    "Perform a thorough error analysis of the project. Search for root causes, erroneous dependencies, configuration issues, broken tests, security risks, and architecture breaks. Prioritize the issues, fix them gradually, and validate any change.",

    "Review the selected QMK code or configuration for improvement opportunities, but do not change files. Suggest safe improvements for readability, maintainability, structure, naming, comments, QMK best practices, and reliability. Prioritize the suggestions by impact and risk.",

    "Create an Arduino sketch with pin output. It should show coordinate in the matrix, function, and if available and recognized, the GPIO pin. Make two versions: one as serial sketch and one as real keyboard. Include RGBs, OLED and encoder if given.",

    "Check my QMK environment in VS Code, but don't change files. Execute diagnostic commands such as qmk doctor, qmk config, and qmk userspace-doctor. Analyze OS, shell, dependencies, compilers, paths, VS Code terminal, tasks, and QMK project structure. Create a prioritized error list with specific fix steps.",

    "Repair my QMK development environment in VS Code. Use the output of qmk doctor, build errors, and VS Code configurations. Only fix secure issues such as missing paths, incorrect terminal profiles, erroneous tasks, incorrect QMK configuration, or obvious dependency issues. Then validate with qmk doctor and a test build."
};"""
s, n = re.subn(
    r'static const char \*const vsc_chat_macros\[6\] = \{.*?\n\};',
    new_macros,
    s,
    count=1,
    flags=re.S
)
if n != 1:
    raise SystemExit("Could not replace vsc_chat_macros array.")

# 4) Replace matrix_scan_user()
new_matrix_scan = r"""void matrix_scan_user(void) {
#ifdef ENCODER_BTN_PIN
    encoder_btn_pressed = (gpio_read_pin(ENCODER_BTN_PIN) == 0);

    if (encoder_btn_pressed && !encoder_btn_was_pressed) {
        encoder_btn_rotated = false;

        if (active_layer_raw() == _TEXT && text_selection_pending_copy) {
            tap_code16(C(KC_C));
            text_selection_pending_copy = false;

            // This press was used for copy, not encoder help.
            encoder_help_started = 0;
            encoder_help_fired = true;
        } else {
            encoder_help_started = timer_read32() | 1;
            encoder_help_fired = false;
        }
    }

    if (encoder_btn_pressed && encoder_help_started != 0 && !encoder_help_fired) {
        if (timer_elapsed32(encoder_help_started) >= ENCODER_HELP_HOLD_MS) {
            encoder_help_fired = true;
            encoder_help_until = timer_read32() + ENCODER_HELP_SHOW_MS;
            oled_view = OLED_VIEW_ENCODER;
        }
    }

    if (!encoder_btn_pressed && encoder_btn_was_pressed) {
        encoder_help_started = 0;
        encoder_btn_rotated = false;
    }

    encoder_btn_was_pressed = encoder_btn_pressed;
#endif

#ifdef OLED_TOGGLE_BTN_PIN
    static bool oled_toggle_was_pressed = false;
    static uint32_t oled_toggle_last_action = 0;

    bool oled_toggle_pressed = (gpio_read_pin(OLED_TOGGLE_BTN_PIN) == 0);

    if (oled_toggle_pressed && !oled_toggle_was_pressed) {
        if (timer_elapsed32(oled_toggle_last_action) > BUTTON_DEBOUNCE_MS) {
            oled_view = (oled_view == OLED_VIEW_LAST_KEY) ? OLED_VIEW_LEGEND : OLED_VIEW_LAST_KEY;

#ifdef VIA_ENABLE
            via_user_config.oled_view = oled_view;
            save_via_config();
#endif

            oled_toggle_last_action = timer_read32();
        }
    }

    oled_toggle_was_pressed = oled_toggle_pressed;
#endif

#ifdef SELECTOR_BTN_PIN
    static bool selector_btn_was_pressed = false;
    static bool selector_btn_combo_used = false;
    static uint32_t selector_btn_last_action = 0;

    bool selector_btn_pressed = (gpio_read_pin(SELECTOR_BTN_PIN) == 0);

#    ifdef ENCODER_BTN_PIN
    bool clear_buttons_pressed = encoder_btn_pressed && selector_btn_pressed;

    if (clear_buttons_pressed) {
        selector_btn_combo_used = true;

        if (button_clear_started == 0) {
            button_clear_started = timer_read32() | 1;
        } else if (!button_clear_armed && timer_elapsed32(button_clear_started) >= CLEAR_EEPROM_HOLD_MS) {
            button_clear_armed = true;

            eeconfig_init();

#        ifdef VIA_ENABLE
            set_via_config_defaults();
            save_via_config();
#        endif

            soft_reset_keyboard();
        }
    } else {
        button_clear_started = 0;
        button_clear_armed = false;
    }
#    endif

    if (!selector_btn_pressed && selector_btn_was_pressed) {
        if (!selector_btn_combo_used && timer_elapsed32(selector_btn_last_action) > BUTTON_DEBOUNCE_MS) {
            selector_btn_last_action = timer_read32();
        }

        selector_btn_combo_used = false;
    }

    selector_btn_was_pressed = selector_btn_pressed;
#endif

#ifdef RGBLIGHT_ENABLE
    if (timer_elapsed32(rgb_frame_timer) >= RGB_FRAME_MS) {
        rgb_frame_timer = timer_read32();
        render_rgb_layer_visuals();
    }
#endif
}"""
s, n = re.subn(
    r'void matrix_scan_user\(void\) \{.*?\n\}\n\nvoid keyboard_post_init_user',
    new_matrix_scan + "\n\nvoid keyboard_post_init_user",
    s,
    count=1,
    flags=re.S
)
if n != 1:
    raise SystemExit("Could not replace matrix_scan_user().")

# 5) Replace keyboard_post_init_user()
new_post_init = r"""void keyboard_post_init_user(void) {
#ifdef RGBLIGHT_ENABLE
    rgblight_enable_noeeprom();
    rgblight_mode_noeeprom(RGBLIGHT_MODE_STATIC_LIGHT);
#endif

#ifdef ENCODER_BTN_PIN
    gpio_set_pin_input_high(ENCODER_BTN_PIN);
#endif

#ifdef OLED_TOGGLE_BTN_PIN
    gpio_set_pin_input_high(OLED_TOGGLE_BTN_PIN);
#endif

    gpio_set_pin_output(GP25);
    gpio_write_pin_high(GP25);

#ifdef SELECTOR_BTN_PIN
    gpio_set_pin_input_high(SELECTOR_BTN_PIN);
#endif

#ifdef FORCE_EEPROM_RESET_ON_BOOT
    // Emergency recovery switch: define FORCE_EEPROM_RESET_ON_BOOT in config.h,
    // flash once, let the board boot, then remove the define and flash again.
    eeconfig_init();
#endif

#ifdef VIA_ENABLE
    load_via_config();
#endif

    boot_start = timer_read32() | 1;
    selector_target = _BASE;
    select_cursor = slot_for_layer(selector_target);
    rgb_frame_timer = timer_read32();
}"""
s, n = re.subn(
    r'void keyboard_post_init_user\(void\) \{.*?\n\}\n\n#ifdef OLED_ENABLE',
    new_post_init + "\n\n#ifdef OLED_ENABLE",
    s,
    count=1,
    flags=re.S
)
if n != 1:
    raise SystemExit("Could not replace keyboard_post_init_user().")

# 6) Fix OLED display labels
s = s.replace(
    'snprintf(line, sizeof(line), "GP12: Lay <-> Last");',
    'snprintf(line, sizeof(line), "GP12: Legend/Last");'
)
s = s.replace(
    'write_line(7, "GP12: back to Lay");',
    'write_line(7, "GP12: back Legend");'
)

# 7) Remove accidental SELECTOR_BTN_PIN GP12 defines in keymap.c only
s = re.sub(r'^\s*#\s*define\s+SELECTOR_BTN_PIN\s+GP12\s*\n', '', s, flags=re.M)

backup = path.with_suffix(path.suffix + ".bak")
backup.write_text(path.read_text(encoding="utf-8"), encoding="utf-8")
path.write_text(s, encoding="utf-8")

print(f"Patched {path}")
print(f"Backup: {backup}")
