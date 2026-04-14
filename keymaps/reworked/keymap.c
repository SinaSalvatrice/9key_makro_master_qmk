
#include QMK_KEYBOARD_H
#include "gpio.h"
#include <stdio.h>

// ============================================================
// RGB / OLED selector build
//
// Notes:
// - This version stops relying on built-in RGBLIGHT animations for the
//   layer visuals and renders the LEDs directly per frame.
// - Adjust key_led_map[] if your WS2812 strip order is not left->right 1..9.
// - Current SELECT layer is now a proper layer launcher / style selector.
// ============================================================

#ifndef RGBLIGHT_LED_COUNT
#    define RGBLIGHT_LED_COUNT 9
#endif

#define PAD_KEY_COUNT        9
#define RGB_FRAME_MS         33
#define SELECT_STEP_MS       500
#define BOOT_TOTAL_MS        2800
#define BUTTON_DEBOUNCE_MS   150

// ── Layer enum ──────────────────────────────────────────────
enum layers {
    _BASE,
    _WINDOW,
    _TEXT,
    _RGB,
    _DEV,
    _VSC,
    _SELECT,
    _LAYER_COUNT
};

// ── Custom keycodes for SELECT mode ─────────────────────────
enum custom_keycodes {
    SEL_BASE = SAFE_RANGE,
    SEL_WINDOW,
    SEL_TEXT,
    SEL_RGB,
    SEL_DEV,
    SEL_VSC,
    VSC_BAR,
    VSC_CHAT,
    VSC_1,
    VSC_2,
    VSC_3,
    VSC_4,
    VSC_5,
    VSC_6,
    RGB_PROFILE
};

// ── Visual profile ──────────────────────────────────────────
// false = wild per-layer animations
// true  = reduced profile; only the currently selected layer key breathes
static bool rgb_minimal_mode = false;

typedef enum {
    VSC_MODE_NONE,
    VSC_MODE_BAR,
    VSC_MODE_CHAT,
} vsc_mode_t;

typedef enum {
    OLED_VIEW_LEGEND,
    OLED_VIEW_LAST_KEY,
} oled_view_t;

// ── OLED / state tracking ───────────────────────────────────
static uint16_t last_keycode    = KC_NO;
static uint8_t  last_key_layer  = _BASE;
static uint8_t  last_row        = 0;
static uint8_t  last_col        = 0;
static uint8_t  select_return_layer = _BASE;
static uint8_t  select_cursor       = 0;
static uint32_t select_cycle_timer  = 0;
static uint32_t rgb_frame_timer     = 0;
static uint32_t boot_start          = 0;
static oled_view_t oled_view        = OLED_VIEW_LEGEND;
static vsc_mode_t vsc_mode          = VSC_MODE_NONE;
static vsc_mode_t last_vsc_mode     = VSC_MODE_BAR;

// ── Key -> LED mapping ──────────────────────────────────────
// Physical key numbering for OLED is row-major / left-to-right:
//
//  1 2 3
//  4 5 6
//  7 8 9
//
// If your strip snakes around, only change this array.
static const uint8_t key_led_map[PAD_KEY_COUNT] = {
    0, 1, 2,
    3, 4, 5,
    6, 7, 8
};

typedef struct {
    uint8_t layer;
    uint8_t hue;
    uint8_t sat;
    uint8_t val;
    const char *name;
    bool selectable;
} select_slot_t;

// 9 physical keys, future-proofed for a 3x3 layer selector.
static const select_slot_t select_slots[PAD_KEY_COUNT] = {
    { _BASE,   160, 220, 120, "BASE",   true  },  // key 1
    { _WINDOW, 176, 240, 120, "WINDOW", true  },  // key 2
    { _TEXT,    96, 220, 110, "TXT",    true  },  // key 3
    { _RGB,    215, 240, 130, "RGB",    true  },  // key 4
    { _SELECT,   0,   0, 120, "SELECT", false },  // key 5 / style toggle
    { _DEV,     18,  255, 130, "DEV",   true  },  // key 6
    { _VSC,    200,  255, 130, "VSC",   true  },  // key 7
    { _SELECT,   0,   0,  24, "FREE",   false },  // key 8
    { _SELECT,   0,   0,  24, "FREE",   false },  // key 9
};

static const char *const vsc_bar_labels[6] = {
    "EXPL", "SRC", "GH-A", "GHUB", "GPT", "FREE"
};

static const char *const vsc_bar_functions[6] = {
    "Explorer", "Source control", "GitHub Actions", "GitHub", "ChatGPT", "Unused"
};

static const char *const vsc_bar_commands[6] = {
    "View: Show Explorer",
    "View: Show Source Control",
    "GitHub Actions: Focus on Workflows View",
    "GitHub Pull Requests: Focus on GitHub Pull Requests View",
    "GitHub Copilot Chat: Focus on Chat View",
    ""
};

static const char *const vsc_chat_labels[6] = {
    "SUM", "REVW", "FIX", "TEST", "EXPL", "COMMIT"
};

static const char *const vsc_chat_functions[6] = {
    "Summarize", "Review", "Suggest fix", "Write tests", "Explain code", "Commit message"
};

static const char *const vsc_chat_macros[6] = {
    "Summarize the selected file and list the most important implementation details.",
    "Review the selected code for bugs, regressions, edge cases, and missing tests.",
    "Suggest a minimal fix for the current problem and explain the root cause.",
    "Write focused tests for the selected code path and cover the main edge cases.",
    "Explain this code step by step, including the important state changes and control flow.",
    "Write a concise commit message and short body for the current staged changes.",
};

static const char *const layer_legend[_LAYER_COUNT][PAD_KEY_COUNT] = {
    [_BASE] = {
        "SEL",  "UP",   "SPC",
        "LEFT", "ENT",  "RGHT",
        "UNDO",  "DOWN", "REDO",
    },
    [_WINDOW] = {
        "SEL",   "HOME",  "END",
        "DESK<", "NO",    "DESK>",
        "WIN<",  "NO",    "WIN>",
    },
    [_TEXT] = {
        "SEL",   "POS1",  "END",
        "ALL",   "COPY",  "PASTE",
        "CUT",   "SPC",   "PENT",
    },
    [_RGB] = {
        "SEL",  "SPD-", "TOG",
        "HUE+", "HUE-", "VAL+",
        "SAT+", "SAT-", "VAL-",
    },
    [_DEV] = {
        "SEL",   "CTRL",  "SHIFT",
        "ALT",   "GUI",   "BTN1",
        "M<-",   "M^",    "M->",
    },
    [_VSC] = {
        "SEL",   "BAR",   "CHAT",
        "EXPL",  "SRC",   "GH-A",
        "GHUB",  "GPT",   "FREE",
    },
    [_SELECT] = {
        "BASE", "WIN",  "TXT",
        "RGB",  "FX",   "DEV",
        "VSC",  "FREE", "FREE",
    },
};

static const char *const layer_function[_LAYER_COUNT][PAD_KEY_COUNT] = {
    [_BASE] = {
        "Select layer",   "Arrow up",       "Space",
        "Arrow left",     "Enter",          "Arrow right",
        "Undo",           "Arrow down",     "Redo",
    },
    [_WINDOW] = {
        "Select layer",   "Jump home",      "Jump end",
        "Prev desktop",   "Unused",         "Next desktop",
        "Prev window",    "Unused",         "Next window",
    },
    [_TEXT] = {
        "Select layer",   "Line start",     "Line end",
        "Select all",     "Copy",           "Paste",
        "Cut",            "Space",          "Keypad enter",
    },
    [_RGB] = {
        "Select layer",   "Speed down",     "Toggle RGB",
        "Hue up",         "Hue down",       "Brightness up",
        "Saturation up",  "Saturation down", "Brightness down",
    },
    [_DEV] = {
        "Select layer",   "Hold Ctrl",      "Hold Shift",
        "Hold Alt",       "Hold GUI",       "Mouse button 1",
        "Mouse left",     "Mouse up",       "Mouse right",
    },
    [_VSC] = {
        "Select layer",   "BAR mode",       "CHAT mode",
        "Combo target 1", "Combo target 2", "Combo target 3",
        "Combo target 4", "Combo target 5", "Combo target 6",
    },
    [_SELECT] = {
        "Go to base",     "Go to window",   "Go to text",
        "Go to RGB",      "Toggle FX mode", "Go to DEV",
        "Go to VSC",      "Unused",         "Unused",
    },
};

static const char *layer_name_short(uint8_t l) {
    switch (l) {
        case _BASE:   return "BASE";
        case _WINDOW: return "WIN";
        case _TEXT:   return "TXT";
        case _RGB:    return "RGB";
        case _DEV:    return "DEV";
        case _VSC:    return "VSC";
        case _SELECT: return "SEL";
        default:      return "BASE";
    }
}

static const char *layer_name_long(uint8_t l) {
    switch (l) {
        case _BASE:   return "BASE";
        case _WINDOW: return "WINDOW";
        case _TEXT:   return "TXT";
        case _RGB:    return "RGB";
        case _DEV:    return "DEV";
        case _VSC:    return "VSC";
        case _SELECT: return "SELECT";
        default:      return "BASE";
    }
}

static vsc_mode_t current_vsc_preview_mode(void) {
    return (vsc_mode == VSC_MODE_CHAT) ? VSC_MODE_CHAT : VSC_MODE_BAR;
}

static const char *vsc_label_for(vsc_mode_t mode, uint8_t index) {
    if (index == 0) return "SEL";
    if (index == 1) return "BAR";
    if (index == 2) return "CHAT";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        if (mode == VSC_MODE_CHAT) {
            return vsc_chat_labels[slot];
        }
        return vsc_bar_labels[slot];
    }
    return "----";
}

static const char *vsc_function_for(vsc_mode_t mode, uint8_t index) {
    if (index == 0) return "Select layer";
    if (index == 1) return "Hold BAR mode";
    if (index == 2) return "Hold CHAT mode";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        if (mode == VSC_MODE_CHAT) {
            return vsc_chat_functions[slot];
        }
        return vsc_bar_functions[slot];
    }
    return "Unknown";
}

static uint8_t active_layer_raw(void) {
    return get_highest_layer(layer_state | default_layer_state);
}

static uint8_t slot_for_layer(uint8_t layer) {
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        if (select_slots[i].selectable && select_slots[i].layer == layer) {
            return i;
        }
    }
    return 0;
}

static uint8_t next_select_slot(uint8_t idx, bool clockwise) {
    for (uint8_t step = 0; step < PAD_KEY_COUNT; step++) {
        idx = clockwise ? ((idx + 1) % PAD_KEY_COUNT) : ((idx + PAD_KEY_COUNT - 1) % PAD_KEY_COUNT);
        if (select_slots[idx].selectable || idx == 4) {
            return idx;
        }
    }
    return idx;
}

static const char *legend_label_for(uint8_t layer, uint8_t row, uint8_t col) {
    uint8_t index = (row * MATRIX_COLS) + col;
    if (layer >= _LAYER_COUNT || index >= PAD_KEY_COUNT) {
        return "----";
    }
    if (layer == _VSC) {
        return vsc_label_for(current_vsc_preview_mode(), index);
    }
    return layer_legend[layer][index];
}

static const char *function_label_for(uint8_t layer, uint8_t row, uint8_t col) {
    uint8_t index = (row * MATRIX_COLS) + col;
    if (layer >= _LAYER_COUNT || index >= PAD_KEY_COUNT) {
        return "Unknown";
    }
    if (layer == _VSC) {
        return vsc_function_for(last_vsc_mode, index);
    }
    return layer_function[layer][index];
}

static void send_vsc_command(const char *command) {
    if (command == NULL || command[0] == '\0') {
        return;
    }

    tap_code16(C(S(KC_P)));
    wait_ms(30);
    send_string(command);
    tap_code(KC_ENT);
}

static void trigger_vsc_target(uint8_t slot) {
    if (slot >= 6) {
        return;
    }

    last_vsc_mode = vsc_mode;

    if (vsc_mode == VSC_MODE_BAR) {
        send_vsc_command(vsc_bar_commands[slot]);
    } else if (vsc_mode == VSC_MODE_CHAT) {
        send_string(vsc_chat_macros[slot]);
    }
}

static uint8_t lerp8(uint8_t a, uint8_t b, uint8_t t) {
    return (uint8_t)(a + (((int16_t)b - (int16_t)a) * t) / 255);
}

static uint8_t triwave8_period(uint32_t now, uint16_t period_ms, uint8_t phase) {
    if (period_ms == 0) {
        return 0;
    }
    uint32_t t = (now + ((uint32_t)phase * period_ms) / 255) % period_ms;
    uint32_t x = (t * 510UL) / period_ms;
    return (x > 255) ? (uint8_t)(510 - x) : (uint8_t)x;
}

static uint8_t pulse_val(uint32_t now, uint16_t period, uint8_t phase, uint8_t min_v, uint8_t max_v) {
    uint8_t tri = triwave8_period(now, period, phase);
    return (uint8_t)(min_v + ((uint16_t)(max_v - min_v) * tri) / 255);
}

#ifdef RGBLIGHT_ENABLE
static void set_key_hsv(uint8_t key_index, uint8_t h, uint8_t s, uint8_t v) {
    if (key_index >= PAD_KEY_COUNT) return;
    uint8_t led = key_led_map[key_index];
    if (led >= RGBLIGHT_LED_COUNT) return;
    rgblight_sethsv_at(h, s, v, led);
}

static void clear_all_keys(void) {
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        set_key_hsv(i, 0, 0, 0);
    }
}

static void render_minimal_profile(uint8_t layer) {
    clear_all_keys();

    if (layer == _SELECT) {
        const select_slot_t *slot = &select_slots[select_cursor];
        uint8_t val = pulse_val(timer_read32(), 2200, 0, 14, 96);
        set_key_hsv(select_cursor, slot->hue, slot->sat, val);
        return;
    }

    uint8_t slot = slot_for_layer(layer);
    const select_slot_t *info = &select_slots[slot];
    uint8_t val = pulse_val(timer_read32(), 2400, 0, 12, 90);
    set_key_hsv(slot, info->hue, info->sat, val);
}

static void render_base_wild(void) {
    uint32_t now = timer_read32();
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t mix = triwave8_period(now, 5200, i * 17);
        uint8_t hue = lerp8(150, 205, mix);  // cyan -> violet
        uint8_t val = pulse_val(now, 2600, i * 11, 18, 88);
        set_key_hsv(i, hue, 220, val);
    }
}

static void render_window_wild(void) {
    uint32_t now        = timer_read32();
    uint8_t  spike      = (now / 110) % PAD_KEY_COUNT;
    uint16_t flash_tick = now % 900;
    bool     flash      = flash_tick < 95;

    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t dist = (i > spike) ? (i - spike) : (spike - i);
        uint8_t hue  = 176;
        uint8_t sat  = 245;
        uint8_t val  = pulse_val(now, 1800, i * 19, 10, 40);

        if (dist == 0) {
            hue = 168;
            val = 160;
        } else if (dist == 1) {
            hue = 176;
            val = 90;
        }

        if (flash && (i % 2 == 0)) {
            sat = 30;
            val = 180;
        }

        set_key_hsv(i, hue, sat, val);
    }
}

static void render_text_wild(void) {
    uint32_t now = timer_read32();
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t drift = triwave8_period(now, 7600, i * 13);
        uint8_t hue   = lerp8(88, 112, drift);  // green -> teal-ish green
        uint8_t val   = pulse_val(now, 3600, i * 9, 10, 58);
        set_key_hsv(i, hue, 210, val);
    }
}

static void render_rgb_wild(void) {
    uint32_t now       = timer_read32();
    bool     beatflash = ((now % 1100) < 120);

    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t hue = (uint8_t)((now / 18) + i * 28);
        uint8_t val = pulse_val(now, 1400, i * 15, 34, beatflash ? 180 : 120);
        set_key_hsv(i, hue, 255, val);
    }
}

static void render_dev_wild(void) {
    uint32_t now = timer_read32();
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t swing = triwave8_period(now, 2200, i * 23);
        uint8_t hue   = lerp8(10, 40, swing);
        uint8_t sat   = (i >= 6) ? 255 : 220;
        uint8_t val   = pulse_val(now, 900 + (i * 40), i * 17, 18, 150);
        set_key_hsv(i, hue, sat, val);
    }
}

static void render_vsc_wild(void) {
    uint32_t now = timer_read32();
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t swing = triwave8_period(now, 1800, i * 21);
        uint8_t hue   = lerp8(160, 215, swing);
        uint8_t sat   = (i >= 3) ? 255 : 210;
        uint8_t val   = pulse_val(now, 1100 + (i * 30), i * 13, 18, 145);
        set_key_hsv(i, hue, sat, val);
    }
}

static void render_select_wild(void) {
    uint32_t now = timer_read32();
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        const select_slot_t *slot = &select_slots[i];
        uint8_t val = slot->selectable ? 34 : 10;
        uint8_t sat = slot->sat;
        uint8_t hue = slot->hue;

        if (i == 4) {  // center key = neutral SELECT/style slot
            sat = 0;
            val = 24;
        }

        if (i == select_cursor) {
            if (slot->selectable) {
                val = pulse_val(now, 900, 0, 80, 170);
            } else {
                sat = 0;
                val = pulse_val(now, 900, 0, 46, 120);
            }
        }

        set_key_hsv(i, hue, sat, val);
    }
}

static void render_rgb_layer_visuals(void) {
    uint8_t layer = active_layer_raw();

    if (rgb_minimal_mode) {
        render_minimal_profile(layer);
        return;
    }

    switch (layer) {
        case _BASE:
            render_base_wild();
            break;
        case _WINDOW:
            render_window_wild();
            break;
        case _TEXT:
            render_text_wild();
            break;
        case _RGB:
            render_rgb_wild();
            break;
        case _DEV:
            render_dev_wild();
            break;
        case _VSC:
            render_vsc_wild();
            break;
        case _SELECT:
            render_select_wild();
            break;
        default:
            render_base_wild();
            break;
    }
}
#endif

// ── Keymaps ─────────────────────────────────────────────────
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(
        MO(_SELECT), KC_UP,        KC_SPC,
        KC_LEFT,     KC_ENT,       KC_RGHT,
        LCTL(KC_Z),  KC_DOWN,      LCTL(KC_R)
    ),

    [_WINDOW] = LAYOUT(
        MO(_SELECT),         KC_HOME,            KC_END,
        LGUI(LCTL(KC_LEFT)), KC_NO,              LGUI(LCTL(KC_RGHT)),
        LSA(LALT(KC_TAB)),   KC_NO,             LALT(KC_TAB)
    ),

    [_TEXT] = LAYOUT(
        MO(_SELECT),         KC_HOME,            KC_END,
        LCTL(KC_A),          LCTL(KC_C),         LCTL(KC_V),
        LCTL(KC_X),          KC_SPC,             KC_PENT
    ),

    [_RGB] = LAYOUT(
        MO(_SELECT), UG_SPDD, UG_TOGG,
        UG_HUEU,     UG_HUED, UG_VALU,
        UG_SATU,     UG_SATD, UG_VALD
    ),

    [_DEV] = LAYOUT(
        MO(_SELECT), KC_LCTL,  KC_LSFT,
        KC_LALT,     KC_LGUI,  MS_BTN1,
        MS_LEFT,     MS_UP,    MS_RGHT
    ),

    [_VSC] = LAYOUT(
        MO(_SELECT), VSC_BAR,  VSC_CHAT,
        VSC_1,       VSC_2,    VSC_3,
        VSC_4,       VSC_5,    VSC_6
    ),

    // 1..9 = physical key numbering left to right
    [_SELECT] = LAYOUT(
        SEL_BASE,   SEL_WINDOW, SEL_TEXT,
        SEL_RGB,    RGB_PROFILE, SEL_DEV,
        SEL_VSC,    KC_NO,      KC_NO
    ),
};

// ── Fallback combo: R0C0 + R2C2 → _BASE ────────────────────
static bool r0c0_held = false;

// ── Track SELECT entry so MO(_SELECT) remembers the base ───
layer_state_t layer_state_set_user(layer_state_t state) {
    static layer_state_t last_state = 0;

    bool select_now    = layer_state_cmp(state, _SELECT);
    bool select_before = layer_state_cmp(last_state, _SELECT);

    if (select_now && !select_before) {
        layer_state_t without_select = (state | default_layer_state) & ~((layer_state_t)1 << _SELECT);
        uint8_t base = get_highest_layer(without_select);
        if (base >= _SELECT) {
            base = _BASE;
        }
        select_return_layer = base;
        select_cursor       = slot_for_layer(base);
        select_cycle_timer  = timer_read32();
    }

    last_state = state;
    return state;
}

static void select_target_layer(uint8_t layer) {
    select_return_layer = layer;
    layer_move(layer);
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.key.row == 0 && record->event.key.col == 0) {
        r0c0_held = record->event.pressed;
    }
    if (record->event.key.row == 2 && record->event.key.col == 2) {
        if (record->event.pressed && r0c0_held) {
            layer_move(_BASE);
            return false;
        }
    }

    if (record->event.pressed) {
        last_keycode = keycode;
        last_key_layer = active_layer_raw();
        last_row     = record->event.key.row;
        last_col     = record->event.key.col;
    }

    switch (keycode) {
        case SEL_BASE:
            if (record->event.pressed) {
                select_target_layer(_BASE);
            }
            return false;
        case SEL_WINDOW:
            if (record->event.pressed) {
                select_target_layer(_WINDOW);
            }
            return false;
        case SEL_TEXT:
            if (record->event.pressed) {
                select_target_layer(_TEXT);
            }
            return false;
        case SEL_RGB:
            if (record->event.pressed) {
                select_target_layer(_RGB);
            }
            return false;
        case SEL_DEV:
            if (record->event.pressed) {
                select_target_layer(_DEV);
            }
            return false;
        case SEL_VSC:
            if (record->event.pressed) {
                select_target_layer(_VSC);
            }
            return false;
        case VSC_BAR:
            vsc_mode = record->event.pressed ? VSC_MODE_BAR : VSC_MODE_NONE;
            return false;
        case VSC_CHAT:
            vsc_mode = record->event.pressed ? VSC_MODE_CHAT : VSC_MODE_NONE;
            return false;
        case VSC_1:
        case VSC_2:
        case VSC_3:
        case VSC_4:
        case VSC_5:
        case VSC_6:
            if (record->event.pressed) {
                trigger_vsc_target((uint8_t)(keycode - VSC_1));
            }
            return false;
        case RGB_PROFILE:
            if (record->event.pressed) {
                rgb_minimal_mode = !rgb_minimal_mode;
                select_cycle_timer = timer_read32();
            }
            return false;
    }

    return true;
}

// ── Encoder: layer-dependent ────────────────────────────────
bool encoder_update_user(uint8_t index, bool clockwise) {
    (void)index;

    switch (active_layer_raw()) {
        case _BASE:
            tap_code(clockwise ? KC_VOLU : KC_VOLD);
            break;
        case _WINDOW:
            if (clockwise) {
                tap_code16(LALT(KC_TAB));
            } else {
                tap_code16(LSA(LALT(KC_TAB)));
            }
            break;
        case _TEXT:
            register_code(KC_LSFT);
            tap_code(clockwise ? KC_RGHT : KC_LEFT);
            unregister_code(KC_LSFT);
            break;
        case _RGB:
            tap_code16(clockwise ? UG_VALU : UG_VALD);
            break;
        case _DEV:
            tap_code(clockwise ? MS_WHLU : MS_WHLD);
            break;
        case _VSC:
            tap_code16(clockwise ? C(KC_PGDN) : C(KC_PGUP));
            break;
        case _SELECT:
            select_cursor = next_select_slot(select_cursor, clockwise);
            select_cycle_timer = timer_read32();
            break;
        default:
            tap_code(clockwise ? KC_VOLU : KC_VOLD);
            break;
    }
    return false;
}

// ── Buttons + animation pump ────────────────────────────────
void matrix_scan_user(void) {
    // Encoder button: toggle OLED mode
    static bool enc_was_pressed = false;
    static uint32_t enc_last_toggle = 0;
    bool        enc_pressed     = (gpio_read_pin(ENCODER_BTN_PIN) == 0);

    if (enc_pressed && !enc_was_pressed && timer_elapsed32(enc_last_toggle) > BUTTON_DEBOUNCE_MS) {
        oled_view = (oled_view == OLED_VIEW_LEGEND) ? OLED_VIEW_LAST_KEY : OLED_VIEW_LEGEND;
        enc_last_toggle = timer_read32();
    }
    enc_was_pressed = enc_pressed;

    // GP12: momentary TXT layer
    static bool txt_was_pressed = false;
    bool        txt_pressed     = (gpio_read_pin(SELECTOR_BTN_PIN) == 0);

    if (txt_pressed && !txt_was_pressed) {
        layer_on(_TEXT);
    } else if (!txt_pressed && txt_was_pressed) {
        layer_off(_TEXT);
    }
    txt_was_pressed = txt_pressed;

    if (active_layer_raw() == _SELECT && timer_elapsed32(select_cycle_timer) >= SELECT_STEP_MS) {
        select_cursor = next_select_slot(select_cursor, true);
        select_cycle_timer = timer_read32();
    }

#ifdef RGBLIGHT_ENABLE
    if (timer_elapsed32(rgb_frame_timer) >= RGB_FRAME_MS) {
        rgb_frame_timer = timer_read32();
        render_rgb_layer_visuals();
    }
#endif
}

// ── Init ────────────────────────────────────────────────────
void keyboard_post_init_user(void) {
#ifdef RGBLIGHT_ENABLE
    rgblight_enable_noeeprom();
    rgblight_mode_noeeprom(RGBLIGHT_MODE_STATIC_LIGHT);
#endif

    gpio_set_pin_output(GP25);
    gpio_write_pin_high(GP25);

    gpio_set_pin_input_high(ENCODER_BTN_PIN);
    gpio_set_pin_input_high(SELECTOR_BTN_PIN);

    boot_start          = timer_read32() | 1;
    select_cursor      = slot_for_layer(_BASE);
    select_cycle_timer = timer_read32();
    rgb_frame_timer    = timer_read32();
}

#ifdef OLED_ENABLE
oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    return rotation;
}

static void write_line(uint8_t row, const char *str) {
    char buf[22];
    snprintf(buf, sizeof(buf), "%-21.21s", str);
    oled_set_cursor(0, row);
    oled_write(buf, false);
}

static bool get_basic_key_label(uint16_t kc, char *buf, uint8_t buflen) {
    switch (kc) {
        case KC_NO:
            snprintf(buf, buflen, "NO");
            return true;
        case KC_UP:
            snprintf(buf, buflen, "UP");
            return true;
        case KC_DOWN:
            snprintf(buf, buflen, "DOWN");
            return true;
        case KC_LEFT:
            snprintf(buf, buflen, "LEFT");
            return true;
        case KC_RGHT:
            snprintf(buf, buflen, "RGHT");
            return true;
        case KC_ENT:
            snprintf(buf, buflen, "ENT");
            return true;
        case KC_SPC:
            snprintf(buf, buflen, "SPC");
            return true;
        case KC_BSPC:
            snprintf(buf, buflen, "BSPC");
            return true;
        case KC_HOME:
            snprintf(buf, buflen, "HOME");
            return true;
        case KC_END:
            snprintf(buf, buflen, "END");
            return true;
        case KC_PENT:
            snprintf(buf, buflen, "PENT");
            return true;
        case KC_PGUP:
            snprintf(buf, buflen, "PGUP");
            return true;
        case KC_PGDN:
            snprintf(buf, buflen, "PGDN");
            return true;
        case KC_VOLU:
            snprintf(buf, buflen, "VOL+");
            return true;
        case KC_VOLD:
            snprintf(buf, buflen, "VOL-");
            return true;
        case KC_LCTL:
            snprintf(buf, buflen, "CTRL");
            return true;
        case KC_LSFT:
            snprintf(buf, buflen, "SHIFT");
            return true;
        case KC_LALT:
            snprintf(buf, buflen, "ALT");
            return true;
        case KC_LGUI:
            snprintf(buf, buflen, "GUI");
            return true;
        case MS_BTN1:
            snprintf(buf, buflen, "BTN1");
            return true;
        case MS_LEFT:
            snprintf(buf, buflen, "M<-");
            return true;
        case MS_UP:
            snprintf(buf, buflen, "MUP");
            return true;
        case MS_RGHT:
            snprintf(buf, buflen, "M->");
            return true;
        case MS_WHLU:
            snprintf(buf, buflen, "WHL+");
            return true;
        case MS_WHLD:
            snprintf(buf, buflen, "WHL-");
            return true;
    }

    return false;
}

static void render_boot(void) {
    if (boot_start == 0) {
        boot_start = timer_read32() | 1;
    }

    uint32_t t = timer_elapsed32(boot_start);

    write_line(0, "");
    write_line(1, "");
    write_line(2, t >=  800 ? "          I AM" : "             I");
    write_line(3, t >= 1200 ? "          ROOT" : "");
    write_line(4, "");
    write_line(5, "");
    write_line(6, "");
    write_line(7, "");
}

static void render_header(uint8_t layer) {
    char line[22];
    snprintf(line, sizeof(line), "%-6s FX:%s",
             layer_name_short(layer), rgb_minimal_mode ? "Q" : "W");
    write_line(0, line);
}

static void render_legend_view(uint8_t layer) {
    char line[22];

    render_header(layer);
    write_line(1, "1      2      3");
    snprintf(line, sizeof(line), "%-6.6s %-6.6s %-6.6s",
             legend_label_for(layer, 0, 0),
             legend_label_for(layer, 0, 1),
             legend_label_for(layer, 0, 2));
    write_line(2, line);
    write_line(3, "4      5      6");
    snprintf(line, sizeof(line), "%-6.6s %-6.6s %-6.6s",
             legend_label_for(layer, 1, 0),
             legend_label_for(layer, 1, 1),
             legend_label_for(layer, 1, 2));
    write_line(4, line);
    write_line(5, "7      8      9");
    snprintf(line, sizeof(line), "%-6.6s %-6.6s %-6.6s",
             legend_label_for(layer, 2, 0),
             legend_label_for(layer, 2, 1),
             legend_label_for(layer, 2, 2));
    write_line(6, line);

    if (layer == _SELECT) {
        snprintf(line, sizeof(line), "Cursor %u -> %.10s", select_cursor + 1, select_slots[select_cursor].name);
    } else if (layer == _VSC) {
        snprintf(line, sizeof(line), "%s mode", (vsc_mode == VSC_MODE_CHAT) ? "CHAT" : "BAR");
    } else {
        snprintf(line, sizeof(line), "BTN view  GP12 TXT");
    }
    write_line(7, line);
}

static void render_last_key_view(void) {
    char buf[22];
    const char *label = legend_label_for(last_key_layer, last_row, last_col);
    const char *func  = function_label_for(last_key_layer, last_row, last_col);

    render_header(last_key_layer);
    write_line(1, "LAST KEY");
    snprintf(buf, sizeof(buf), "Layer: %s", layer_name_long(last_key_layer));
    write_line(2, buf);
    snprintf(buf, sizeof(buf), "Key:   %.14s", label);
    write_line(3, buf);
    snprintf(buf, sizeof(buf), "Code:  0x%04X", last_keycode);
    write_line(4, buf);
    snprintf(buf, sizeof(buf), "Func:  %.14s", func);
    write_line(5, buf);
    snprintf(buf, sizeof(buf), "Pos:   %u (%u,%u)",
             (last_row * 3) + last_col + 1, last_row, last_col);
    write_line(6, buf);
    write_line(7, "BTN legend");
}

bool oled_task_user(void) {
    if (boot_start == 0 || timer_elapsed32(boot_start) < BOOT_TOTAL_MS) {
        render_boot();
        return false;
    }

    if (oled_view == OLED_VIEW_LAST_KEY) {
        render_last_key_view();
    } else {
        render_legend_view(active_layer_raw());
    }

    return false;
}
#endif
