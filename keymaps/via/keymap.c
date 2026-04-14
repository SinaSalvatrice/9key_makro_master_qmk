#include QMK_KEYBOARD_H
#include <stdio.h>
#include "gpio.h"

enum layers {
    _BASE,
    _WIN,
    _EDIT,
    _MEDIA,
    _FN,
    _RGB,
    _SELECT,
};

static bool     selector_active    = false;
static uint32_t last_activity_time = 0;
static bool     sleep_sent         = false;
static uint16_t last_keycode       = KC_NO;
static uint8_t  last_row           = 0;
static uint8_t  last_col           = 0;

static uint8_t active_layer(void) {
    return get_highest_layer(layer_state | default_layer_state);
}

static uint8_t visual_layer(void) {
    return selector_active ? _SELECT : active_layer();
}

static const char *layer_name(uint8_t layer) {
    switch (layer) {
        case _BASE:   return "BASE";
        case _WIN:    return "WIN";
        case _EDIT:   return "EDIT";
        case _MEDIA:  return "MEDIA";
        case _FN:     return "FN";
        case _RGB:    return "RGB";
        case _SELECT: return "SELECT";
        default:      return "UNK";
    }
}

static void apply_layer_rgb(uint8_t layer) {
#ifdef RGBLIGHT_ENABLE
    rgblight_enable_noeeprom();

    switch (layer) {
        case _BASE:
            rgblight_mode_noeeprom(RGBLIGHT_MODE_RAINBOW_MOOD);
            rgblight_set_speed_noeeprom(128);
            break;
        case _WIN:
            rgblight_mode_noeeprom(RGBLIGHT_MODE_BREATHING + 3);
            rgblight_set_speed_noeeprom(128);
            rgblight_sethsv_noeeprom(158, 220, 100);
            break;
        case _EDIT:
            rgblight_mode_noeeprom(RGBLIGHT_MODE_BREATHING + 3);
            rgblight_set_speed_noeeprom(128);
            rgblight_sethsv_noeeprom(72, 255, 100);
            break;
        case _MEDIA:
            rgblight_mode_noeeprom(RGBLIGHT_MODE_BREATHING + 3);
            rgblight_set_speed_noeeprom(144);
            rgblight_sethsv_noeeprom(224, 200, 100);
            break;
        case _FN:
            rgblight_mode_noeeprom(RGBLIGHT_MODE_BREATHING + 3);
            rgblight_set_speed_noeeprom(128);
            rgblight_sethsv_noeeprom(28, 255, 100);
            break;
        case _RGB:
            rgblight_mode_noeeprom(RGBLIGHT_MODE_BREATHING + 3);
            rgblight_set_speed_noeeprom(128);
            rgblight_sethsv_noeeprom(192, 255, 100);
            break;
        case _SELECT:
        default:
            rgblight_mode_noeeprom(RGBLIGHT_MODE_BREATHING + 3);
            rgblight_set_speed_noeeprom(192);
            rgblight_sethsv_noeeprom(0, 255, 100);
            break;
    }
#else
    (void)layer;
#endif
}

static void refresh_feedback(void) {
    apply_layer_rgb(visual_layer());
}

static void begin_selector(void) {
    if (selector_active) {
        return;
    }
    selector_active = true;
    layer_on(_SELECT);
    refresh_feedback();
}

static void finish_selector(void) {
    if (!selector_active) {
        return;
    }
    selector_active = false;
    layer_off(_SELECT);
    refresh_feedback();
}

void keyboard_post_init_user(void) {
    gpio_set_pin_input_high(ENCODER_BTN_PIN);
    gpio_set_pin_input_high(SELECTOR_BTN_PIN);

    last_activity_time = timer_read32();
    refresh_feedback();
}

layer_state_t layer_state_set_user(layer_state_t state) {
    if (!selector_active) {
        apply_layer_rgb(get_highest_layer(state | default_layer_state));
    }

    return state;
}

void matrix_scan_user(void) {
    // ── selector button (GP12) ──────────────────────────────
    static bool was_sel_pressed = false;
    bool        sel_pressed     = gpio_read_pin(SELECTOR_BTN_PIN) == 0;

    if (sel_pressed && !was_sel_pressed) { begin_selector(); }
    if (!sel_pressed && was_sel_pressed) { finish_selector(); }
    was_sel_pressed = sel_pressed;

    // ── encoder button (GP9): wake ──────────────────────────
    static bool was_enc_pressed = false;
    bool        enc_pressed     = gpio_read_pin(ENCODER_BTN_PIN) == 0;

    if (enc_pressed && !was_enc_pressed) {
        last_activity_time = timer_read32();
        sleep_sent = false;
        tap_code16(KC_SYSTEM_WAKE);
    }
    was_enc_pressed = enc_pressed;

    // ── idle sleep ──────────────────────────────────────────
    if (!sleep_sent && timer_elapsed32(last_activity_time) > IDLE_SLEEP_TIMEOUT_MS) {
        sleep_sent = true;
        tap_code16(KC_SYSTEM_SLEEP);
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        last_activity_time = timer_read32();
        sleep_sent         = false;
        last_keycode       = keycode;
        last_row           = record->event.key.row;
        last_col           = record->event.key.col;
    }

    return true;
}

bool encoder_update_user(uint8_t index, bool clockwise) {
    (void)index;

    last_activity_time = timer_read32();
    sleep_sent         = false;

    if (selector_active) {
        return false;
    }

    switch (active_layer()) {
        case _BASE:
            clockwise ? tap_code(MS_WHLU) : tap_code(MS_WHLD);
            break;
        case _WIN:
            clockwise ? tap_code16(LGUI(KC_TAB)) : tap_code16(LALT(KC_TAB));
            break;
        case _EDIT:
            clockwise ? tap_code(KC_RGHT) : tap_code(KC_LEFT);
            break;
        case _MEDIA:
            clockwise ? tap_code(KC_VOLU) : tap_code(KC_VOLD);
            break;
        case _FN:
            clockwise ? tap_code(KC_RGHT) : tap_code(KC_LEFT);
            break;
        case _RGB:
#ifdef RGBLIGHT_ENABLE
            clockwise ? rgblight_increase_val_noeeprom() : rgblight_decrease_val_noeeprom();
#endif
            break;
        default:
            break;
    }

    return false;
}

#ifdef OLED_ENABLE
oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    return rotation;
}

static void render_selector(void) {
    oled_write_ln_P(PSTR("BAS WIN EDT"), false);
    oled_write_ln_P(PSTR("MED FN  RGB"), false);
    oled_write_ln_P(PSTR("pick a key "), false);
    oled_write_P(PSTR("POS "), false);
    oled_write(get_u8_str(last_row, ' '), false);
    oled_write_P(PSTR(","), false);
    oled_write_ln(get_u8_str(last_col, ' '), false);
}

bool oled_task_user(void) {
    char keycode_hex[8];

    oled_set_cursor(0, 0);
    oled_write_P(PSTR("Layer "), false);
    oled_write_ln(layer_name(visual_layer()), false);

    if (selector_active) {
        render_selector();
        return false;
    }

    snprintf(keycode_hex, sizeof(keycode_hex), "0x%04X", last_keycode);

    oled_write_P(PSTR("Key   "), false);
    oled_write_ln(keycode_hex, false);
    oled_write_P(PSTR("Pos   "), false);
    oled_write(get_u8_str(last_row, ' '), false);
    oled_write_P(PSTR(","), false);
    oled_write_ln(get_u8_str(last_col, ' '), false);

    return false;
}
#endif

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(
        LGUI(KC_TAB), KC_UP,           LALT(KC_TAB),
        KC_LEFT,      KC_ENT,          KC_RGHT,
        LCTL(KC_Z),   KC_DOWN,         LCTL(KC_R)
    ),
    [_WIN] = LAYOUT(
        LGUI(KC_TAB), KC_UP,           LALT(KC_TAB),
        KC_LEFT,      KC_ENT,          KC_RGHT,
        LCTL(KC_Z),   KC_DOWN,         LCTL(KC_R)
    ),
    [_EDIT] = LAYOUT(
        LCTL(KC_A),        LCTL(KC_C),   LCTL(KC_V),
        LCTL(KC_X),        LCTL(KC_ENT), KC_NO,
        LCTL(LSFT(KC_Z)),  KC_SPC,       KC_BSPC
    ),
    [_MEDIA] = LAYOUT(
        KC_MPRV, KC_MSEL, KC_MNXT,
        KC_MRWD, KC_MPLY, KC_MFFD,
        KC_DOWN, KC_MSTP, KC_UP
    ),
    [_FN] = LAYOUT(
        KC_F13, KC_F14, KC_F15,
        KC_F16, KC_F17, KC_F18,
        KC_F19, KC_F20, KC_F21
    ),
    [_RGB] = LAYOUT(
        UG_SPDU, UG_SPDD, UG_TOGG,
        UG_HUEU, UG_HUED, UG_VALU,
        UG_SATU, UG_SATD, UG_VALD
    ),
    [_SELECT] = LAYOUT(
        TO(_WIN),  TO(_EDIT), TO(_MEDIA),
        TO(_FN),   TO(_BASE), TO(_RGB),
        KC_NO,     KC_NO,     KC_NO
    ),
};
