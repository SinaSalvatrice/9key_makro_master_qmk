#include QMK_KEYBOARD_H
#include <stdio.h>
#include "gpio.h"

enum layers {
    _BASE,
    _NAV,
    _EDIT,
    _MEDIA,
    _FN,
    _RGB,
    _SELECT,
};

static bool     selector_active = false;
static uint8_t  selector_origin = _BASE;
static uint8_t  selector_target = _BASE;
static uint16_t last_keycode    = KC_NO;
static uint8_t  last_row        = 0;
static uint8_t  last_col        = 0;

static uint8_t active_layer(void) {
    return get_highest_layer(layer_state | default_layer_state);
}

static uint8_t visual_layer(void) {
    return selector_active ? _SELECT : active_layer();
}

static const char *layer_name(uint8_t layer) {
    switch (layer) {
        case _BASE:   return "BASE";
        case _NAV:    return "NAV";
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
    rgblight_mode_noeeprom(RGBLIGHT_MODE_BREATHING + 3);

    switch (layer) {
        case _BASE:
            rgblight_set_speed_noeeprom(128);
            rgblight_sethsv_noeeprom(170, 255, 100);
            break;
        case _NAV:
            rgblight_set_speed_noeeprom(128);
            rgblight_sethsv_noeeprom(145, 220, 100);
            break;
        case _EDIT:
            rgblight_set_speed_noeeprom(128);
            rgblight_sethsv_noeeprom(50, 255, 100);
            break;
        case _MEDIA:
            rgblight_set_speed_noeeprom(144);
            rgblight_sethsv_noeeprom(210, 180, 100);
            break;
        case _FN:
            rgblight_set_speed_noeeprom(128);
            rgblight_sethsv_noeeprom(20, 255, 100);
            break;
        case _RGB:
            rgblight_set_speed_noeeprom(128);
            rgblight_sethsv_noeeprom(192, 255, 100);
            break;
        case _SELECT:
        default:
            rgblight_set_speed_noeeprom(192);
            rgblight_sethsv_noeeprom(0, 255, 100);
            break;
    }
#else
    (void)layer;
#endif
}

static void refresh_feedback(void) {
    apply_layer_rgb(selector_active ? selector_target : visual_layer());
}

layer_state_t layer_state_set_user(layer_state_t state) {
    if (!selector_active) {
        apply_layer_rgb(get_highest_layer(state | default_layer_state));
    }
    return state;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        last_keycode = keycode;
        last_row     = record->event.key.row;
        last_col     = record->event.key.col;
    }
    return true;
}

#ifdef OLED_ENABLE
oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    return rotation;
}

static void render_selector(void) {
    oled_write_ln_P(PSTR("BAS NAV EDT"), false);
    oled_write_ln_P(PSTR("MED FN  RGB"), false);
    oled_write_P(PSTR("SEL -> "), false);
    oled_write_ln(layer_name(selector_target), false);
    oled_write_P(PSTR("POS "), false);
    oled_write(get_u8_str(last_row, ' '), false);
    oled_write_P(PSTR(","), false);
    oled_write_ln(get_u8_str(last_col, ' '), false);
}

bool oled_task_user(void) {
    char keycode_hex[8];

    oled_clear();
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

void keyboard_post_init_user(void) {
    gpio_set_pin_input_high(ENCODER_BTN_PIN);

    // OLED power pin
    gpio_set_pin_output(GP25);
    gpio_write_pin_high(GP25);

    refresh_feedback();
}

static void begin_selector(void) {
    if (selector_active) {
        return;
    }

    selector_origin = active_layer();
    if (selector_origin == _SELECT) {
        selector_origin = _BASE;
    }

    selector_target = selector_origin;
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
    layer_move(selector_target);
    refresh_feedback();
}

static void rotate_selector(bool clockwise) {
    if (clockwise) {
        selector_target = (selector_target + 1) % _SELECT;
    } else {
        selector_target = (selector_target == _BASE) ? (_SELECT - 1) : (selector_target - 1);
    }
    refresh_feedback();
}

void matrix_scan_user(void) {
    static bool was_pressed = false;
    bool pressed = gpio_read_pin(ENCODER_BTN_PIN) == 0;

    if (pressed && !was_pressed) {
        begin_selector();
    }

    if (!pressed && was_pressed) {
        finish_selector();
    }

    was_pressed = pressed;
}

bool encoder_update_user(uint8_t index, bool clockwise) {
    (void)index;

    if (selector_active) {
        rotate_selector(clockwise);
        return false;
    }

    switch (active_layer()) {
        case _BASE:
            clockwise ? tap_code(MS_WHLU) : tap_code(MS_WHLD);
            break;
        case _NAV:
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

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(
        KC_1,         KC_2,        KC_3,
        KC_4,         MO(_SELECT), KC_6,
        KC_7,         KC_8,        KC_9
    ),

    [_NAV] = LAYOUT(
        KC_LEFT, KC_UP,   KC_RGHT,
        KC_HOME, KC_ENT,  KC_END,
        KC_PGDN, KC_DOWN, KC_PGUP
    ),

    [_EDIT] = LAYOUT(
        LCTL(KC_Z), LCTL(KC_C), LCTL(KC_V),
        LCTL(KC_X), KC_ENT,     KC_BSPC,
        LCTL(KC_A), KC_SPC,     LCTL(LSFT(KC_Z))
    ),

    [_MEDIA] = LAYOUT(
        KC_MPRV, KC_MPLY, KC_MNXT,
        KC_MUTE, KC_MSTP, KC_VOLU,
        KC_NO,   KC_NO,   KC_VOLD
    ),

    [_FN] = LAYOUT(
        KC_F13, KC_F14, KC_F15,
        KC_F16, KC_F17, KC_F18,
        KC_F19, KC_F20, KC_F21
    ),

    [_RGB] = LAYOUT(
        UG_TOGG, UG_HUEU, UG_HUED,
        UG_SATU, UG_VALU, UG_VALD,
        UG_SATD, UG_NEXT, UG_PREV
    ),

    [_SELECT] = LAYOUT(
        TO(_NAV),  TO(_EDIT), TO(_MEDIA),
        TO(_FN),   KC_TRNS,   TO(_RGB),
        KC_NO,     KC_NO,     KC_NO
    )
};
