#include QMK_KEYBOARD_H
#include "gpio.h"
#include <stdio.h>

// ── Layers ──────────────────────────────────────────────────
enum layers {
    _BASE,
    _L1,
    _L2,
    _SELECT,
};

// ── State ────────────────────────────────────────────────────
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
        case _L1:     return "L1";
        case _L2:     return "L2";
        case _SELECT: return "SELECT";
        default:      return "UNK";
    }
}

// ── RGB ──────────────────────────────────────────────────────
static void apply_layer_rgb(uint8_t layer) {
#ifdef RGBLIGHT_ENABLE
    rgblight_enable_noeeprom();
    rgblight_mode_noeeprom(RGBLIGHT_MODE_BREATHING + 3);
    rgblight_set_speed_noeeprom(220);
    switch (layer) {
        case _BASE:
            rgblight_sethsv_noeeprom(128, 255, 100);
            break;
        case _L1:
            rgblight_sethsv_noeeprom(85, 255, 100);
            break;
        case _L2:
            rgblight_sethsv_noeeprom(192, 255, 100);
            break;
        case _SELECT:
        default:
            rgblight_sethsv_noeeprom(0, 0, 160);
            break;
    }
#else
    (void)layer;
#endif
}

static void refresh_feedback(void) {
    apply_layer_rgb(visual_layer());
}

// ── Selector (GP12, hold = SELECT layer) ─────────────────────
static void begin_selector(void) {
    if (selector_active) return;
    selector_active = true;
    layer_on(_SELECT);
    refresh_feedback();
}

static void finish_selector(void) {
    if (!selector_active) return;
    selector_active = false;
    layer_off(_SELECT);
    refresh_feedback();
}

// ── Init ─────────────────────────────────────────────────────
void keyboard_post_init_user(void) {
    gpio_set_pin_input_high(SELECTOR_BTN_PIN);
    last_activity_time = timer_read32();
    refresh_feedback();
}

// ── Layer → RGB on layer change ───────────────────────────────
layer_state_t layer_state_set_user(layer_state_t state) {
    if (!selector_active) {
        apply_layer_rgb(get_highest_layer(state | default_layer_state));
    }
    return state;
}

// ── GPIO scan: selector + encoder wake + idle sleep ──────────
void matrix_scan_user(void) {
    // selector button (GP12): hold = SELECT layer, any press = wake + activity reset
    static bool was_sel = false;
    bool        sel     = gpio_read_pin(SELECTOR_BTN_PIN) == 0;
    if (sel && !was_sel) {
        last_activity_time = timer_read32();
        sleep_sent = false;
        begin_selector();
    }
    if (!sel && was_sel) { finish_selector(); }
    was_sel = sel;

    // idle sleep
    if (!sleep_sent && timer_elapsed32(last_activity_time) > IDLE_SLEEP_TIMEOUT_MS) {
        sleep_sent = true;
        tap_code16(KC_SYSTEM_SLEEP);
    }
}

// ── Key tracking + activity reset ────────────────────────────
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

// ── Encoder ──────────────────────────────────────────────────
bool encoder_update_user(uint8_t index, bool clockwise) {
    (void)index;
    last_activity_time = timer_read32();
    sleep_sent         = false;

    if (selector_active) return false;

    switch (active_layer()) {
        case _BASE:
            clockwise ? tap_code(MS_WHLU) : tap_code(MS_WHLD);
            break;
        case _L1:
            clockwise ? tap_code(KC_VOLU) : tap_code(KC_VOLD);
            break;
        case _L2:
            clockwise ? tap_code(KC_RGHT) : tap_code(KC_LEFT);
            break;
        default:
            break;
    }
    return false;
}

// ── OLED ─────────────────────────────────────────────────────
#ifdef OLED_ENABLE

oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    return rotation;
}

static void render_selector(void) {
    oled_write_ln_P(PSTR("BASE  L1    L2  "), false);
    oled_write_ln_P(PSTR("^ pick top row  "), false);
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

// ── Keymaps ──────────────────────────────────────────────────
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(
        LALT(KC_TAB),  KC_UP,         LGUI(KC_TAB),
        KC_LEFT,       LALT(KC_SPC),  KC_RGHT,
        KC_PGUP,       KC_DOWN,       KC_PGDN
    ),
    [_L1] = LAYOUT(
        KC_SELECT,  KC_COPY,   KC_PASTE,
        KC_CUT,     MS_BTN1,   MS_BTN2,
        KC_BSPC,    KC_SPC,    KC_ENT
    ),
    [_L2] = LAYOUT(
        LCTL(KC_Z),      LCTL(KC_S),       LCTL(KC_R),
        KC_WWW_BACK,     KC_WWW_REFRESH,   KC_WWW_SEARCH,
        KC_F22,          KC_F23,           KC_F24
    ),
    [_SELECT] = LAYOUT(
        TO(_BASE),  TO(_L1),   TO(_L2),
        UG_PREV,    UG_TOGG,   UG_NEXT,
        KC_CALC,    KC_FIND,   KC_MYCM
    ),
};
