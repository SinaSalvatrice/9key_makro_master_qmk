#include QMK_KEYBOARD_H

// ── Layers ──────────────────────────────────────────────────
enum layers {
    _BASE,
    _FN,
    _SELECT,
};

// ── State ────────────────────────────────────────────────────
static bool     selector_active    = false;
static uint32_t last_activity_time = 0;
static bool     sleep_sent         = false;

static uint8_t active_layer(void) {
    return get_highest_layer(layer_state | default_layer_state);
}

static void begin_selector(void) {
    if (selector_active) return;
    selector_active = true;
    layer_on(_SELECT);
}

static void finish_selector(void) {
    if (!selector_active) return;
    selector_active = false;
    layer_off(_SELECT);
}

// ── Init ─────────────────────────────────────────────────────
void keyboard_post_init_user(void) {
    gpio_set_pin_input_high(SELECTOR_BTN_PIN);
    last_activity_time = timer_read32();
}

// ── GPIO scan: selector + idle sleep ─────────────────────────
void matrix_scan_user(void) {
    static bool was_sel = false;
    bool        sel     = gpio_read_pin(SELECTOR_BTN_PIN) == 0;
    if (sel && !was_sel) {
        last_activity_time = timer_read32();
        sleep_sent = false;
        begin_selector();
    }
    if (!sel && was_sel) { finish_selector(); }
    was_sel = sel;

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
        case _FN:
            clockwise ? tap_code(KC_VOLU) : tap_code(KC_VOLD);
            break;
        case _BASE:
        default:
            clockwise ? tap_code(MS_WHLU) : tap_code(MS_WHLD);
            break;
    }
    return false;
}

// ── Keymaps ──────────────────────────────────────────────────
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(
        LALT(KC_TAB),  KC_UP,         LGUI(KC_TAB),
        KC_LEFT,       KC_ENT,        KC_RGHT,
        LCTL(KC_Z),    KC_DOWN,       LCTL(KC_R)
    ),
    [_FN] = LAYOUT(
        KC_F13,  KC_F14,  KC_F15,
        KC_F16,  KC_F17,  KC_F18,
        KC_F19,  KC_F20,  KC_F21
    ),
    [_SELECT] = LAYOUT(
        TO(_BASE),  UG_HUEU,  UG_HUED,
        UG_SATU,    UG_TOGG,  UG_SATD,
        UG_VALU,    UG_VALD,  TO(_FN)
    ),
};
