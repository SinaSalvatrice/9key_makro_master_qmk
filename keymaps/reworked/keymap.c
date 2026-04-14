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
#define BOOT_TOTAL_MS        2800
#define BUTTON_DEBOUNCE_MS   150

// ── Layer enum ──────────────────────────────────────────────
enum layers {
    _BASE,
    _WINDOW,
    _TEXT,
    _MEDIA,
    _DEV,
    _VSC,
    _RGB,
    _SELECT,
    _LAYER_COUNT
};

// ── Custom keycodes for SELECT mode ─────────────────────────
enum custom_keycodes {
    SEL_BASE = SAFE_RANGE,
    SEL_WINDOW,
    SEL_TEXT,
    SEL_MEDIA,
    SEL_RGB,
    SEL_DEV,
    SEL_VSC,
    WIN_BRO,
    WIN_AUX,
    WIN_1,
    WIN_2,
    WIN_3,
    WIN_4,
    WIN_5,
    WIN_6,
    TXT_ACT,
    TXT_EDT,
    TXT_1,
    TXT_2,
    TXT_3,
    TXT_4,
    TXT_5,
    TXT_6,
    VSC_BAR,
    VSC_CHAT,
    VSC_1,
    VSC_2,
    VSC_3,
    VSC_4,
    VSC_5,
    VSC_6,
    RGB_PROFILE,
    LAYER_LEGEND,
    KEY_FUNCTION_LEGEND,
    LAST_KEY_LEGEND
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

typedef enum {
    TEXT_MODE_NAV,
    TEXT_MODE_ACTIONS,
    TEXT_MODE_EDIT,
} text_mode_t;

typedef enum {
    WINDOW_MODE_NAV,
    WINDOW_MODE_BROWSER,
} window_mode_t;

// ── OLED / state tracking ───────────────────────────────────
static uint16_t last_keycode         = KC_NO;
static uint8_t  last_key_layer       = _BASE;
static uint8_t  last_row             = 0;
static uint8_t  last_col             = 0;
static uint8_t  selector_target      = _BASE;
static uint8_t  select_cursor        = 0;
static uint32_t rgb_frame_timer      = 0;
static uint32_t boot_start           = 0;
static oled_view_t oled_view         = OLED_VIEW_LEGEND;
static vsc_mode_t vsc_mode           = VSC_MODE_NONE;
static vsc_mode_t last_vsc_mode      = VSC_MODE_BAR;
static bool matrix_select_held       = false;
static bool encoder_btn_pressed      = false;
static bool text_action_held         = false;
static bool text_edit_held           = false;
static bool window_browser_held      = false;
static text_mode_t last_key_text_mode    = TEXT_MODE_NAV;
static window_mode_t last_key_window_mode = WINDOW_MODE_NAV;
static vsc_mode_t last_key_vsc_mode      = VSC_MODE_BAR;

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
    { _MEDIA,   32, 255, 130, "MEDIA",  true  },  // key 4
    { _SELECT,   0,   0, 120, "SELECT", false },  // key 5 / style toggle
    { _DEV,     18, 255, 130, "DEV",    true  },  // key 6
    { _VSC,    200, 255, 130, "VSC",    true  },  // key 7
    { _RGB,    215, 240, 130, "RGB",    true  },  // key 8
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
    "Review the selected repo for bugs, regressions, edge cases, and missing tests.",
    "Suggest a minimal fix for the current problem and explain the root cause.",
    "Write focused tests for the selected code path and cover the main edge cases.",
    "Explain this code step by step, including the important state changes and control flow.",
    "Write a concise commit message and short body for the current staged changes.",
};

static const char *const text_nav_labels[6] = {
    "HOME", "UP", "END", "LEFT", "DOWN", "RGHT"
};

static const char *const text_action_labels[6] = {
    "ALL", "COPY", "PASTE", "CUT", "UNDO", "REDO"
};

static const char *const text_edit_labels[6] = {
    "ENT", "BSPC", "SPC", "TAB", "SHIFT", "BTN1"
};

static const char *const text_nav_functions[6] = {
    "Line start", "Cursor up", "Line end", "Cursor left", "Cursor down", "Cursor right"
};

static const char *const text_action_functions[6] = {
    "Select all", "Copy", "Paste", "Cut", "Undo", "Redo"
};

static const char *const text_edit_functions[6] = {
    "Enter", "Backspace", "Space", "Tab", "One-shot Shift", "Mouse button 1"
};

static const char *const window_nav_labels[6] = {
    "DESK<", "TASK", "DESK>", "WIN<", "SHOW", "WIN>"
};

static const char *const window_browser_labels[6] = {
    "BACK", "REFR", "FWD", "TAB<", "NEW", "TAB>"
};

static const char *const window_nav_functions[6] = {
    "Previous desktop", "Task view", "Next desktop", "Previous window", "Show desktop", "Next window"
};

static const char *const window_browser_functions[6] = {
    "Browser back", "Refresh page", "Browser forward", "Previous tab", "New tab", "Next tab"
};

static const char *const layer_legend[_LAYER_COUNT][PAD_KEY_COUNT] = {
    [_BASE] = {
        "SEL",  "UP",   "BSPC",
        "LEFT", "ENT",  "RGHT",
        "UNDO", "DOWN", "REDO",
    },
    [_WINDOW] = {
        "SEL",   "BRO",   "AUX",
        "DESK<", "TASK",  "DESK>",
        "WIN<",  "SHOW",  "WIN>",
    },
    [_TEXT] = {
        "SEL",  "ACT",  "EDT",
        "HOME", "UP",   "END",
        "LEFT", "DOWN", "RGHT",
    },
    [_MEDIA] = {
        "SEL",  "PREV", "NEXT",
        "RWND", "PLAY", "FFWD",
        "VOL-", "MUTE", "VOL+",
    },
    [_RGB] = {
        "SEL",  "SPD-", "TOG",
        "HUE+", "HUE-", "VAL+",
        "SAT+", "SAT-", "VAL-",
    },
    [_DEV] = {
        "SEL",  "CTRL", "SHIFT",
        "ALT",  "GUI",  "BTN1",
        "M<-",  "M^",   "M->",
    },
    [_VSC] = {
        "SEL",  "BAR",  "CHAT",
        "EXPL", "SRC",  "GH-A",
        "GHUB", "GPT",  "FREE",
    },
    [_SELECT] = {
        "BASE", "WIN",  "TXT",
        "MED",  "FX",   "DEV",
        "VSC",  "RGB",  "FREE",
    },
};

static const char *const layer_function[_LAYER_COUNT][PAD_KEY_COUNT] = {
    [_BASE] = {
        "Select layer",   "Arrow up",       "Backspace",
        "Arrow left",     "Enter",          "Arrow right",
        "Undo",           "Arrow down",     "Redo",
    },
    [_WINDOW] = {
        "Select layer",   "Browser combo",  "Reserved",
        "Prev desktop",   "Task view",      "Next desktop",
        "Prev window",    "Show desktop",   "Next window",
    },
    [_TEXT] = {
        "Select layer",   "Action combo",   "Edit combo",
        "Line start",     "Cursor up",      "Line end",
        "Cursor left",    "Cursor down",    "Cursor right",
    },
    [_MEDIA] = {
        "Select layer",   "Previous track", "Next track",
        "Rewind",         "Play/Pause",     "Fast forward",
        "Volume down",    "Mute",           "Volume up",
    },
    [_RGB] = {
        "Select layer",   "Speed down",      "Toggle RGB",
        "Hue up",         "Hue down",        "Brightness up",
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
        "Go to media",    "Toggle FX mode", "Go to DEV",
        "Go to VSC",      "Go to RGB",      "Unused",
    },
};

static const char *layer_name_short(uint8_t l) {
    switch (l) {
        case _BASE:   return "BASE";
        case _WINDOW: return "WIN";
        case _TEXT:   return "TXT";
        case _MEDIA:  return "MED";
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
        case _MEDIA:  return "MEDIA";
        case _RGB:    return "RGB";
        case _DEV:    return "DEV";
        case _VSC:    return "VSC";
        case _SELECT: return "SELECT";
        default:      return "BASE";
    }
}

static vsc_mode_t current_vsc_preview_mode(void) {
    if (vsc_mode != VSC_MODE_NONE) return vsc_mode;
    return last_vsc_mode;
}

static text_mode_t current_text_preview_mode(void) {
    if (text_action_held) return TEXT_MODE_ACTIONS;
    if (text_edit_held) return TEXT_MODE_EDIT;
    return TEXT_MODE_NAV;
}

static window_mode_t current_window_preview_mode(void) {
    return window_browser_held ? WINDOW_MODE_BROWSER : WINDOW_MODE_NAV;
}

static const char *text_label_for_mode(text_mode_t mode, uint8_t index) {
    if (index == 0) return "SEL";
    if (index == 1) return "ACT";
    if (index == 2) return "EDT";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        switch (mode) {
            case TEXT_MODE_ACTIONS:
                return text_action_labels[slot];
            case TEXT_MODE_EDIT:
                return text_edit_labels[slot];
            case TEXT_MODE_NAV:
            default:
                return text_nav_labels[slot];
        }
    }
    return "----";
}

static const char *text_function_for_mode(text_mode_t mode, uint8_t index) {
    if (index == 0) return "Select layer";
    if (index == 1) return "Hold text actions";
    if (index == 2) return "Hold text edit tools";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        switch (mode) {
            case TEXT_MODE_ACTIONS:
                return text_action_functions[slot];
            case TEXT_MODE_EDIT:
                return text_edit_functions[slot];
            case TEXT_MODE_NAV:
            default:
                return text_nav_functions[slot];
        }
    }
    return "Unknown";
}

static const char *window_label_for_mode(window_mode_t mode, uint8_t index) {
    if (index == 0) return "SEL";
    if (index == 1) return "BRO";
    if (index == 2) return "AUX";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        if (mode == WINDOW_MODE_BROWSER) {
            return window_browser_labels[slot];
        }
        return window_nav_labels[slot];
    }
    return "----";
}

static const char *window_function_for_mode(window_mode_t mode, uint8_t index) {
    if (index == 0) return "Select layer";
    if (index == 1) return "Hold browser controls";
    if (index == 2) return "Reserved for later";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        if (mode == WINDOW_MODE_BROWSER) {
            return window_browser_functions[slot];
        }
        return window_nav_functions[slot];
    }
    return "Unknown";
}

static const char *text_label_for(uint8_t index) {
    return text_label_for_mode(current_text_preview_mode(), index);
}

static const char *text_function_for(uint8_t index) {
    return text_function_for_mode(current_text_preview_mode(), index);
}

static const char *window_label_for(uint8_t index) {
    return window_label_for_mode(current_window_preview_mode(), index);
}

static const char *window_function_for(uint8_t index) {
    return window_function_for_mode(current_window_preview_mode(), index);
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

static void update_select_layer_state(void) {
    if (matrix_select_held) {
        layer_on(_SELECT);
    } else {
        layer_off(_SELECT);
    }
}

static uint8_t slot_for_layer(uint8_t layer) {
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        if (select_slots[i].selectable && select_slots[i].layer == layer) {
            return i;
        }
    }
    return 0;
}

static void sync_selector_target_from_cursor(void) {
    if (select_cursor < PAD_KEY_COUNT && select_slots[select_cursor].selectable) {
        selector_target = select_slots[select_cursor].layer;
    }
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
    if (layer == _TEXT) {
        return text_label_for(index);
    }
    if (layer == _WINDOW) {
        return window_label_for(index);
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
    if (layer == _TEXT) {
        return text_function_for(index);
    }
    if (layer == _WINDOW) {
        return window_function_for(index);
    }
    if (layer == _VSC) {
        return vsc_function_for(current_vsc_preview_mode(), index);
    }
    return layer_function[layer][index];
}

static const char *last_key_label_for(void) {
    uint8_t index = (last_row * MATRIX_COLS) + last_col;
    if (last_key_layer >= _LAYER_COUNT || index >= PAD_KEY_COUNT) {
        return "----";
    }

    if (last_key_layer == _TEXT) {
        return text_label_for_mode(last_key_text_mode, index);
    }
    if (last_key_layer == _WINDOW) {
        return window_label_for_mode(last_key_window_mode, index);
    }
    if (last_key_layer == _VSC) {
        return vsc_label_for(last_key_vsc_mode, index);
    }
    return layer_legend[last_key_layer][index];
}

static const char *last_key_function_for(void) {
    uint8_t index = (last_row * MATRIX_COLS) + last_col;
    if (last_key_layer >= _LAYER_COUNT || index >= PAD_KEY_COUNT) {
        return "Unknown";
    }

    if (last_key_layer == _TEXT) {
        return text_function_for_mode(last_key_text_mode, index);
    }
    if (last_key_layer == _WINDOW) {
        return window_function_for_mode(last_key_window_mode, index);
    }
    if (last_key_layer == _VSC) {
        return vsc_function_for(last_key_vsc_mode, index);
    }
    return layer_function[last_key_layer][index];
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

    if (vsc_mode == VSC_MODE_NONE) {
        return;
    }

    if (vsc_mode == VSC_MODE_BAR) {
        send_vsc_command(vsc_bar_commands[slot]);
    } else if (vsc_mode == VSC_MODE_CHAT) {
        send_string(vsc_chat_macros[slot]);
    }
}

static void tap_text_target(uint8_t slot) {
    if (slot >= 6) {
        return;
    }

    switch (current_text_preview_mode()) {
        case TEXT_MODE_ACTIONS:
            switch (slot) {
                case 0: tap_code16(C(KC_A)); break;
                case 1: tap_code16(C(KC_C)); break;
                case 2: tap_code16(C(KC_V)); break;
                case 3: tap_code16(C(KC_X)); break;
                case 4: tap_code16(C(KC_Z)); break;
                case 5: tap_code16(C(KC_Y)); break;
            }
            break;

        case TEXT_MODE_EDIT:
            switch (slot) {
                case 0: tap_code(KC_ENT); break;
                case 1: tap_code(KC_BSPC); break;
                case 2: tap_code(KC_SPC); break;
                case 3: tap_code(KC_TAB); break;
                case 4: set_oneshot_mods(MOD_LSFT); break;
                case 5: tap_code(MS_BTN1); break;
            }
            break;

        case TEXT_MODE_NAV:
        default:
            switch (slot) {
                case 0: tap_code(KC_HOME); break;
                case 1: tap_code(KC_UP); break;
                case 2: tap_code(KC_END); break;
                case 3: tap_code(KC_LEFT); break;
                case 4: tap_code(KC_DOWN); break;
                case 5: tap_code(KC_RGHT); break;
            }
            break;
    }
}

static void tap_window_target(uint8_t slot) {
    if (slot >= 6) {
        return;
    }

    if (current_window_preview_mode() == WINDOW_MODE_BROWSER) {
        switch (slot) {
            case 0: tap_code16(A(KC_LEFT)); break;
            case 1: tap_code16(C(KC_R)); break;
            case 2: tap_code16(A(KC_RGHT)); break;
            case 3: tap_code16(C(S(KC_TAB))); break;
            case 4: tap_code16(C(KC_T)); break;
            case 5: tap_code16(C(KC_TAB)); break;
        }
        return;
    }

    switch (slot) {
        case 0: tap_code16(G(C(KC_LEFT))); break;
        case 1: tap_code16(G(KC_TAB)); break;
        case 2: tap_code16(G(C(KC_RGHT))); break;
        case 3: tap_code16(S(A(KC_TAB))); break;
        case 4: tap_code16(G(KC_D)); break;
        case 5: tap_code16(A(KC_TAB)); break;
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
        uint8_t target_slot = slot_for_layer(selector_target);
        const select_slot_t *slot = &select_slots[target_slot];
        uint8_t val = pulse_val(timer_read32(), 2200, 0, 14, 96);
        set_key_hsv(target_slot, slot->hue, slot->sat, val);
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
    uint32_t now   = timer_read32();
    uint8_t  spike = (now / 110) % PAD_KEY_COUNT;

    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t dist = (i > spike) ? (i - spike) : (spike - i);
        uint8_t hue  = 176;
        uint8_t sat  = 245;
        uint8_t val  = pulse_val(now, 1800, i * 19, 10, 40);

        if (dist == 0) {
            hue = 168;
            sat = 245;
            val = 160;
        } else if (dist == 1) {
            hue = 176;
            sat = 240;
            val = 90;
        }

        set_key_hsv(i, hue, sat, val);
    }
}

static void render_text_wild(void) {
    uint32_t now = timer_read32();
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t drift = triwave8_period(now, 7600, i * 13);
        uint8_t hue   = lerp8(88, 112, drift);
        uint8_t val   = pulse_val(now, 3600, i * 9, 10, 58);
        set_key_hsv(i, hue, 210, val);
    }
}

static void render_media_wild(void) {
    uint32_t now = timer_read32();
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t swing = triwave8_period(now, 3000, i * 18);
        uint8_t hue   = lerp8(24, 42, swing);
        uint8_t val   = pulse_val(now, 2000, i * 14, 14, 100);
        set_key_hsv(i, hue, 255, val);
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
    uint8_t target_slot = slot_for_layer(selector_target);

    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        const select_slot_t *slot = &select_slots[i];
        uint8_t val = slot->selectable ? 34 : 10;
        uint8_t sat = slot->sat;
        uint8_t hue = slot->hue;

        if (i == 4) {
            sat = 0;
            val = 24;
        }

        if (i == target_slot && i != select_cursor && slot->selectable) {
            if (val < 72) {
                val = 72;
            }
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
        case _MEDIA:
            render_media_wild();
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
        MO(_SELECT),         KC_UP,              KC_BSPC,
        KC_LEFT,             KC_ENT,             KC_RGHT,
        LCTL(KC_Z),          KC_DOWN,            LCTL(KC_Y)
    ),

    [_WINDOW] = LAYOUT(
        MO(_SELECT),         WIN_BRO,            WIN_AUX,
        WIN_1,               WIN_2,              WIN_3,
        WIN_4,               WIN_5,              WIN_6
    ),

    [_TEXT] = LAYOUT(
        MO(_SELECT),         TXT_ACT,            TXT_EDT,
        TXT_1,               TXT_2,              TXT_3,
        TXT_4,               TXT_5,              TXT_6
    ),

    [_MEDIA] = LAYOUT(
        MO(_SELECT),         KC_MPRV,            KC_MNXT,
        KC_MRWD,             KC_MPLY,            KC_MFFD,
        KC_VOLD,             KC_MUTE,            KC_VOLU
    ),

    [_RGB] = LAYOUT(
        MO(_SELECT),         UG_SPDD,            UG_TOGG,
        UG_HUEU,             UG_HUED,            UG_VALU,
        UG_SATU,             UG_SATD,            UG_VALD
    ),

    [_DEV] = LAYOUT(
        MO(_SELECT),         MS_UP,              KC_LSFT,
        MS_LEFT,             MS_BTN1,            MS_RGHT,
        KC_LALT,             MS_DOWN,            MS_BTN2
    ),

    [_VSC] = LAYOUT(
        MO(_SELECT),         VSC_BAR,            VSC_CHAT,
        VSC_1,               VSC_2,              VSC_3,
        VSC_4,               VSC_5,              VSC_6
    ),

    [_SELECT] = LAYOUT(
        SEL_BASE,            SEL_WINDOW,         SEL_TEXT,
        SEL_MEDIA,           RGB_PROFILE,        SEL_DEV,
        SEL_VSC,             SEL_RGB,            KC_NO
    ),
};

// ── Fallback combo: R0C0 + R2C2 → _BASE ────────────────────
static bool r0c0_held = false;

// ── Track SELECT entry so selector holds remember the base ─
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
        selector_target = base;
        select_cursor   = slot_for_layer(selector_target);
    }

    last_state = state;
    return state;
}

static void select_target_layer(uint8_t layer) {
    selector_target = layer;
    select_cursor   = slot_for_layer(selector_target);
    layer_move(selector_target);
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.key.row == SELECTOR_MATRIX_ROW && record->event.key.col == SELECTOR_MATRIX_COL) {
        r0c0_held = record->event.pressed;
    }

    if (record->event.key.row == 2 && record->event.key.col == 2) {
        if (record->event.pressed && r0c0_held) {
            selector_target = _BASE;
            select_cursor   = slot_for_layer(selector_target);
            layer_move(_BASE);
            return false;
        }
    }

    if (keycode == MO(_SELECT)) {
        if (record->event.pressed) {
            matrix_select_held = true;
            update_select_layer_state();
        } else {
            matrix_select_held = false;
            update_select_layer_state();
            layer_move(selector_target);
        }
        return false;
    }

    if (record->event.pressed) {
        last_keycode          = keycode;
        last_key_layer        = active_layer_raw();
        last_row              = record->event.key.row;
        last_col              = record->event.key.col;
        last_key_text_mode    = current_text_preview_mode();
        last_key_window_mode  = current_window_preview_mode();
        last_key_vsc_mode     = current_vsc_preview_mode();
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

        case SEL_MEDIA:
            if (record->event.pressed) {
                select_target_layer(_MEDIA);
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

        case WIN_BRO:
            window_browser_held = record->event.pressed;
            return false;

        case WIN_AUX:
            return false;

        case WIN_1:
        case WIN_2:
        case WIN_3:
        case WIN_4:
        case WIN_5:
        case WIN_6:
            if (record->event.pressed) {
                tap_window_target((uint8_t)(keycode - WIN_1));
            }
            return false;

        case TXT_ACT:
            text_action_held = record->event.pressed;
            return false;

        case TXT_EDT:
            text_edit_held = record->event.pressed;
            return false;

        case TXT_1:
        case TXT_2:
        case TXT_3:
        case TXT_4:
        case TXT_5:
        case TXT_6:
            if (record->event.pressed) {
                tap_text_target((uint8_t)(keycode - TXT_1));
            }
            return false;

        case VSC_BAR:
            if (record->event.pressed) {
                vsc_mode = VSC_MODE_BAR;
                last_vsc_mode = VSC_MODE_BAR;
            } else {
                vsc_mode = VSC_MODE_NONE;
            }
            return false;

        case VSC_CHAT:
            if (record->event.pressed) {
                vsc_mode = VSC_MODE_CHAT;
                last_vsc_mode = VSC_MODE_CHAT;
            } else {
                vsc_mode = VSC_MODE_NONE;
            }
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
            tap_code(clockwise ? MS_WHLU : MS_WHLD);
            break;

        case _WINDOW:
            if (current_window_preview_mode() == WINDOW_MODE_BROWSER) {
                tap_code16(clockwise ? C(KC_PGDN) : C(KC_PGUP));
            } else if (clockwise) {
                tap_code16(A(KC_TAB));
            } else {
                tap_code16(S(A(KC_TAB)));
            }
            break;

        case _TEXT:
            if (encoder_btn_pressed) {
                tap_code16(clockwise ? S(KC_RGHT) : S(KC_LEFT));
            } else {
                tap_code(clockwise ? KC_RGHT : KC_LEFT);
            }
            break;

        case _MEDIA:
            tap_code(clockwise ? KC_VOLU : KC_VOLD);
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
            sync_selector_target_from_cursor();
            break;

        default:
            tap_code(clockwise ? KC_VOLU : KC_VOLD);
            break;
    }

    return false;
}

void matrix_scan_user(void) {
#ifdef ENCODER_BTN_PIN
    encoder_btn_pressed = (gpio_read_pin(ENCODER_BTN_PIN) == 0);
#endif

#ifdef SELECTOR_BTN_PIN
    static bool gp12_was_pressed = false;
    static uint32_t gp12_last_action = 0;

    bool gp12_pressed = (gpio_read_pin(SELECTOR_BTN_PIN) == 0);

    if (!gp12_pressed && gp12_was_pressed) {
        if (timer_elapsed32(gp12_last_action) > BUTTON_DEBOUNCE_MS) {
            oled_view = (oled_view == OLED_VIEW_LEGEND)
                      ? OLED_VIEW_LAST_KEY
                      : OLED_VIEW_LEGEND;
            gp12_last_action = timer_read32();
        }
    }

    gp12_was_pressed = gp12_pressed;
#endif

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

#ifdef SELECTOR_BTN_PIN
    gpio_set_pin_input_high(SELECTOR_BTN_PIN);
#endif

    boot_start      = timer_read32() | 1;
    selector_target = _BASE;
    select_cursor   = slot_for_layer(selector_target);
    rgb_frame_timer = timer_read32();
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

static bool __attribute__((unused)) get_basic_key_label(uint16_t kc, char *buf, uint8_t buflen) {
    switch (kc) {
        case KC_NO:   snprintf(buf, buflen, "NO");   return true;
        case KC_UP:   snprintf(buf, buflen, "UP");   return true;
        case KC_DOWN: snprintf(buf, buflen, "DOWN"); return true;
        case KC_LEFT: snprintf(buf, buflen, "LEFT"); return true;
        case KC_RGHT: snprintf(buf, buflen, "RGHT"); return true;
        case KC_ENT:  snprintf(buf, buflen, "ENT");  return true;
        case KC_SPC:  snprintf(buf, buflen, "SPC");  return true;
        case KC_BSPC: snprintf(buf, buflen, "BSPC"); return true;
        case KC_HOME: snprintf(buf, buflen, "HOME"); return true;
        case KC_END:  snprintf(buf, buflen, "END");  return true;
        case KC_PENT: snprintf(buf, buflen, "PENT"); return true;
        case KC_PGUP: snprintf(buf, buflen, "PGUP"); return true;
        case KC_PGDN: snprintf(buf, buflen, "PGDN"); return true;
        case KC_VOLU: snprintf(buf, buflen, "VOL+"); return true;
        case KC_VOLD: snprintf(buf, buflen, "VOL-"); return true;
        case KC_LCTL: snprintf(buf, buflen, "CTRL"); return true;
        case KC_LSFT: snprintf(buf, buflen, "SHIFT"); return true;
        case KC_LALT: snprintf(buf, buflen, "ALT"); return true;
        case KC_LGUI: snprintf(buf, buflen, "GUI"); return true;
        case MS_BTN1: snprintf(buf, buflen, "BTN1"); return true;
        case MS_LEFT: snprintf(buf, buflen, "M<-"); return true;
        case MS_UP:   snprintf(buf, buflen, "MUP"); return true;
        case MS_RGHT: snprintf(buf, buflen, "M->"); return true;
        case MS_WHLU: snprintf(buf, buflen, "WHL+"); return true;
        case MS_WHLD: snprintf(buf, buflen, "WHL-"); return true;
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
        snprintf(line, sizeof(line), "Cur%u Tgt->%.10s",
                 select_cursor + 1, layer_name_long(selector_target));
    } else if (layer == _VSC) {
        snprintf(line, sizeof(line), "%s mode%s",
                 (current_vsc_preview_mode() == VSC_MODE_CHAT) ? "CHAT" : "BAR",
                 (vsc_mode != VSC_MODE_NONE) ? " [HELD]" : "");
    } else if (layer == _TEXT) {
        const char *mode = "NAV";
        if (text_action_held) {
            mode = "ACT";
        } else if (text_edit_held) {
            mode = "EDT";
        }
        snprintf(line, sizeof(line), "TXT %s%s",
                 mode,
                 (text_action_held || text_edit_held) ? " [HELD]" : "");
    } else if (layer == _WINDOW) {
        snprintf(line, sizeof(line), "WIN %s%s",
                 window_browser_held ? "BRO" : "NAV",
                 window_browser_held ? " [HELD]" : "");
    } else {
#ifdef SELECTOR_BTN_PIN
        snprintf(line, sizeof(line), "GP12: Lay <-> Last");
#else
        snprintf(line, sizeof(line), "Hold SEL for grid");
#endif
    }
    write_line(7, line);
}

static void render_last_key_view(void) {
    char buf[22];
    const char *label = last_key_label_for();
    const char *func  = last_key_function_for();

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

#ifdef SELECTOR_BTN_PIN
    write_line(7, "GP12: back to Lay");
#else
    write_line(7, "Last-key details");
#endif
}

bool oled_task_user(void) {
    if (boot_start == 0 || timer_elapsed32(boot_start) < BOOT_TOTAL_MS) {
        render_boot();
        return false;
    }

    uint8_t layer = active_layer_raw();

    if (layer == _SELECT) {
        render_legend_view(_SELECT);
    } else if (oled_view == OLED_VIEW_LAST_KEY) {
        render_last_key_view();
    } else {
        render_legend_view(layer);
    }

    return false;
}
#endif
