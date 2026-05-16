#include QMK_KEYBOARD_H
#include "gpio.h"
#ifdef VIA_ENABLE
#include "via.h"
#endif
#include <stdio.h>

// ============================================================
// RGB / OLED selector build
// - GP11 toggles OLED Legend <-> Last Key view
// - Long encoder-button hold shows temporary Encoder Help view
// - GAME reuses the old DEV layer slot; MEDIA keeps its swapped palette slot
// ============================================================

#ifndef RGBLIGHT_LED_COUNT
#define RGBLIGHT_LED_COUNT 65
#endif

#define RGB_KEYFIELD_LED_COUNT 15
#define RGB_FRAME_LED_FIRST    RGB_KEYFIELD_LED_COUNT
#define RGB_FRAME_LED_COUNT    (RGBLIGHT_LED_COUNT - RGB_FRAME_LED_FIRST)
#define RGB_CORE_LED_COUNT     RGB_KEYFIELD_LED_COUNT

#ifndef SELECTOR_BTN_PIN
#    define SELECTOR_BTN_PIN GP12
#endif

#define PAD_KEY_COUNT          9
#define RGB_FRAME_MS           33
#define BOOT_TOTAL_MS          2800
#define BUTTON_DEBOUNCE_MS     150
#define SELECTOR_DOUBLE_TAP_MS 300
#define CLEAR_EEPROM_HOLD_MS   3000
#define VIA_LAYER_SLOT_COUNT   8
#define REWORKED_LAYOUT_VERSION 4
#define ENCODER_HELP_HOLD_MS   700
#define ENCODER_HELP_SHOW_MS   2500

// ── Layer enum ──────────────────────────────────────────────
enum layers {
    _BASE,
    _WINDOW,
    _TEXT,
    _MEDIA,
    _DEV,
    _VSC,
    _RGB,
    _RGBMOD,
    _RGBADJ,
    _PROMPT,
    _SELECT,
    _LAYER_COUNT
};

// ── Custom keycodes ─────────────────────────────────────────
enum custom_keycodes {
    SEL_BASE = QK_KB_0,
    SEL_WINDOW,
    SEL_TEXT,
    SEL_MEDIA,
    SEL_RGB,
    SEL_DEV,
    SEL_VSC,
    SEL_PROMPT,
    PRM_PICS,
    PRM_ETSY,
    GM_NAV,
    GM_WASD,
    GM_1,
    GM_2,
    GM_3,
    GM_4,
    GM_5,
    GM_6,
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
    RGB_MODE,
    RGB_TOG,
    RGB_HUEU,
    RGB_HUED,
    RGB_VALU,
    RGB_VALD,
    RGB_SATU,
    RGB_SATD
};

enum rgb_zone_bits {
    RGB_ZONE_FRAME = 1 << 0,
    RGB_ZONE_KEY   = 1 << 1,
    RGB_ZONE_GAP   = 1 << 2,
    RGB_ZONE_ALL   = RGB_ZONE_FRAME | RGB_ZONE_KEY | RGB_ZONE_GAP,
};

static bool rgb_output_enabled = true;
static bool rgb_mode_held = false;
static uint8_t rgb_zone_mask = RGB_ZONE_ALL;

typedef enum {
    VSC_MODE_NONE,
    VSC_MODE_BAR,
    VSC_MODE_CHAT,
} vsc_mode_t;

typedef enum {
    OLED_VIEW_LEGEND,
    OLED_VIEW_LAST_KEY,
    OLED_VIEW_ENCODER,
} oled_view_t;

typedef enum {
    RGB_EFFECT_WILD = 0,
    RGB_EFFECT_BREATHING,
    RGB_EFFECT_RUNNING,
    RGB_EFFECT_TWINKLE,
    RGB_EFFECT_PULSE,
    RGB_EFFECT_SOLID,
    RGB_EFFECT_COMET,
    RGB_EFFECT_SCAN,
    RGB_EFFECT_RAINBOW,
    RGB_EFFECT_STACK,
    RGB_EFFECT_OFF,
    RGB_EFFECT_COUNT
} rgb_effect_mode_t;

typedef enum {
    TEXT_MODE_WIN,
    TEXT_MODE_ACTIONS,
    TEXT_MODE_EDIT,
} text_mode_t;

typedef enum {
    WINDOW_MODE_WIN,
    WINDOW_MODE_BROWSER,
} window_mode_t;

typedef enum {
    GAME_MODE_NAV,
    GAME_MODE_WASD,
} game_mode_t;

typedef enum {
    PROMPT_MODE_BASE,
    PROMPT_MODE_PICS,
    PROMPT_MODE_ETSY,
} prompt_mode_t;

// ── State ───────────────────────────────────────────────────
static uint16_t last_keycode              = KC_NO;
static uint8_t  last_key_layer            = _BASE;
static uint8_t  last_row                  = 0;
static uint8_t  last_col                  = 0;
static uint8_t  selector_target           = _BASE;
static uint8_t  select_cursor             = 0;
static uint32_t rgb_frame_timer           = 0;
static uint32_t boot_start                = 0;
static oled_view_t oled_view              = OLED_VIEW_LEGEND;
static vsc_mode_t vsc_mode                = VSC_MODE_NONE;
static vsc_mode_t last_vsc_mode           = VSC_MODE_BAR;
static text_mode_t text_mode              = TEXT_MODE_WIN;
static bool matrix_select_held            = false;
static bool encoder_btn_pressed           = false;
static bool encoder_btn_was_pressed       = false;
static bool encoder_btn_rotated           = false;
static bool button_clear_armed            = false;
static uint32_t button_clear_started      = 0;
static bool text_selection_pending_copy   = false;
static bool text_action_held              = false;
static bool text_edit_held                = false;
static text_mode_t last_key_text_mode     = TEXT_MODE_WIN;
static bool window_browser_held           = false;
static window_mode_t last_key_window_mode = WINDOW_MODE_WIN;
static game_mode_t game_mode              = GAME_MODE_WASD;
static game_mode_t last_key_game_mode     = GAME_MODE_WASD;
static vsc_mode_t last_key_vsc_mode       = VSC_MODE_BAR;
static prompt_mode_t prompt_mode          = PROMPT_MODE_BASE;
static prompt_mode_t last_key_prompt_mode = PROMPT_MODE_BASE;
static bool last_key_rgb_mode             = false;
static uint32_t encoder_help_started      = 0;
static uint32_t encoder_help_until        = 0;
static bool encoder_help_fired            = false;
static uint8_t selector_origin_layer      = _BASE;
static uint32_t selector_last_tap         = 0;

// The first 15 LEDs are the 3x5 key area:
//  K1 - K2 - K3
//  K4 - K5 - K6
//  K7 - K8 - K9
// Each key LED sits at positions 0,2,4 of its row; the spacer LEDs are 1 and 3.
// LEDs 16..65 are the extra frame-light chain.
static const uint8_t key_led_map[PAD_KEY_COUNT] = {
    0, 2, 4,
    5, 7, 9,
    10, 12, 14
};

typedef struct {
    uint8_t layer;
    uint8_t hue;
    uint8_t sat;
    uint8_t val;
    const char *name;
    bool selectable;
} select_slot_t;

typedef struct {
    uint8_t hue;
    uint8_t sat;
    uint8_t val;
} hsv_config_t;

#ifdef VIA_ENABLE
enum via_custom_value {
    id_via_oled_view          = 1,
    id_via_fx_mode            = 2,
    id_via_layer_color        = 3,
    id_via_layer_brightness   = 4,
    id_via_rgb_effect         = 5,
    id_via_layer_effect_speed = 6,
};

typedef struct {
    uint8_t      signature;
    uint8_t      layout_version;
    uint8_t      oled_view;
    uint8_t      fx_mode;
    uint8_t      layer_effect[VIA_LAYER_SLOT_COUNT];
    uint8_t      layer_speed[VIA_LAYER_SLOT_COUNT];
    hsv_config_t layer_palette[VIA_LAYER_SLOT_COUNT];
} via_user_config_t;

static via_user_config_t via_user_config;
#endif

// GAME / MEDIA share the old DEV slot to keep VIA layer indexing stable.
static select_slot_t select_slots[PAD_KEY_COUNT] = {
    { _SELECT,   0,   0, 120, "SELECT", false },
    { _WINDOW, 176, 240, 120, "WINDOW", true  },
    { _TEXT,    96, 220, 110, "TXT",    true  },
    { _MEDIA,   18, 255, 130, "MEDIA",  true  },
    { _BASE,   160, 220, 120, "BASE",   true  },
    { _DEV,     32, 255, 130, "GAME",   true  },
    { _VSC,    200, 255, 130, "VSC",    true  },
    { _RGB,    215, 240, 130, "RGB",    true  },
    { _PROMPT,   8, 255, 140, "PROMT",  true  },
};

static const uint8_t via_layer_slots[VIA_LAYER_SLOT_COUNT] = {
    4, 1, 2, 3, 5, 6, 7, 8
};

// Order follows via_layer_slots: BASE, WINDOW, TEXT, MEDIA, GAME(old DEV slot), VSC, RGB, PROMPT.
static const hsv_config_t via_default_palette[VIA_LAYER_SLOT_COUNT] = {
    {160, 220, 120},
    {176, 240, 120},
    { 96, 220, 110},
    { 18, 255, 130}, // MEDIA
    { 32, 255, 130}, // GAME
    {200, 255, 130},
    {215, 240, 130},
    {  8, 255, 140}, // PROMPT
};

static const char *const vsc_bar_labels[6] = {"EXPL", "SRC", "GH-A", "GHUB", "GPT", "FREE"};
static const char *const vsc_bar_functions[6] = {"Explorer", "Source control", "GitHub Actions", "GitHub", "ChatGPT", "Unused"};
static const char *const vsc_bar_commands[6] = {
    "View: Show Explorer",
    "View: Show Source Control",
    "GitHub Actions: Focus on Workflows View",
    "GitHub Pull Requests: Focus on GitHub Pull Requests View",
    "GitHub Copilot Chat: Focus on Chat View",
    ""
};

static const char *const vsc_chat_labels[6] = {"SUM", "REVW", "FIX", "TEST", "EXPL", "COMMIT"};
static const char *const vsc_chat_functions[6] = {"Summarize", "Review", "Suggest fix", "Write tests", "Explain code", "Commit message"};
static const char *const vsc_chat_macros[6] = {
    "Explain this QMK error or build output. Identify the most likely root cause, the affected file or setting, why it happens, and the safest fix. Include the exact command or file to check next. Do not modify files automatically.",

    "Perform a thorough error analysis of the project. Search for root causes, erroneous dependencies, configuration issues, broken tests, security risks, and architecture breaks. Prioritize the issues, fix them gradually, and validate any change.",

    "Review the selected QMK code or configuration for improvement opportunities, but do not change files. Suggest safe improvements for readability, maintainability, structure, naming, comments, QMK best practices, and reliability. Prioritize the suggestions by impact and risk.",

    "Create an Arduino sketch with pin output. It should show coordinate in the matrix, function, and if available and recognized, the GPIO pin. Make two versions: one as serial sketch and one as real keyboard. Include RGBs, OLED and encoder if given.",

    "Check my QMK environment in VS Code, but don't change files. Execute diagnostic commands such as qmk doctor, qmk config, and qmk userspace-doctor. Analyze OS, shell, dependencies, compilers, paths, VS Code terminal, tasks, and QMK project structure. Create a prioritized error list with specific fix steps.",

    "Repair my QMK development environment in VS Code. Use the output of qmk doctor, build errors, and VS Code configurations. Only fix secure issues such as missing paths, incorrect terminal profiles, erroneous tasks, incorrect QMK configuration, or obvious dependency issues. Then validate with qmk doctor and a test build."
};

static const char *const prompt_base_labels[6] = {"SUM", "REVW", "FIX", "TEST", "EXPL", "COMMIT"};
static const char *const prompt_base_functions[6] = {"Prompt summarize", "Prompt review", "Prompt suggest fix", "Prompt write tests", "Prompt explain code", "Prompt commit message"};

static const char *const prompt_pics_labels[6] = {"GEAR", "LETT", "BGEXT", "WALL", "SVG", "MOCK};
static const char *const prompt_pics_functions[6] = {"AI Gear", "letter tranform", "background extraction", "Clock on wall", "Svg conversion", "Mockup creation"};
static const char *const prompt_pics_macros[6] = {
    "Create a clean black-and-white vector-style image of four separate gears arranged in a 2x2 grid on a pure white background.
Each gear should be the same size, symmetrical, and visually distinct, with a clean industrial machine aesthetic.
Use only pure black and pure white. No gray, no shading, no gradients, no shadows, and no texture.
The gears must be separate from each other and not interlocking.
Design them as filled out black silhouettes with inner cutouts, chambers, and mechanical openings.
Keep the overall look orderly, precise, technical, and suitable for SVG conversion and commercial sale. technical drawing,
front-facing flat design, no gray, no gradient, no shadow, no blur, no 3D, no texture, no steampunk, no cartoon, no playful style, no background elements, no text, no logo, pure black and white only.",
    "Transform to lowercase ",
    "Extract the background and fill whole image with it.",
    "Let the clock hang on a white suble structured concrete wall. Keep lighting, dont add shadows. Make it look as real as possible. Dont change the clock or its perspective",
    "Convert to svg",
    "Create an Etsy mockup image for this product. Show the product in an appealing setting with good lighting and a complementary background. Include a styled text overlay with the product name and a catchy tagline. Make it look polished and professional, suitable for an online store listing. Do not include any logos, watermarks, or branding elements."
};

static const char *const prompt_etsy_labels[6] = {"TAGS", "TITLE", "DESC", "BULL", "LIST", "SEO"};
static const char *const prompt_etsy_functions[6] = {"Tag list", "Listing title", "Listing description", "Bullet highlights", "Listing creation", "Etsy SEO pass"};
static const char *const prompt_etsy_macros[6] = {
    "Generate Etsy tag ideas for this product. Create high-intent tags, avoid duplicates, vary phrase length, and explain which tags are strongest.",
    "Write multiple Etsy listing title options for this product. Optimize for clarity, search intent, and readability instead of stuffing every keyword.",
    "Write an Etsy listing description for this product. Start with a strong buyer-focused opening, then cover features, materials, size, usage, and care.",
    "Write concise bullet-style highlights for this Etsy product listing. Focus on benefits, materials, sizing, personalization, and gift appeal.",
    "Create a complete Etsy listing draft for this product, including title, description, tags, image plan, and quick notes on pricing or variation structure.",
    "Perform an Etsy SEO pass on this listing draft. Improve titles, tags, wording, scannability, and conversion clarity without making it sound spammy."
};

static const char *const text_win_labels[6]    = {"HOME", "UP", "END", "LEFT", "DOWN", "RGHT"};
static const char *const text_action_labels[6] = {"ALL", "COPY", "PASTE", "CUT", "UNDO", "REDO"};
static const char *const text_edit_labels[6]   = {"ENT", "BSPC", "SPC", "TAB", "SHIFT", "BTN1"};

static const char *const text_win_functions[6]    = {"Line start", "Cursor up", "Line end", "Cursor left", "Cursor down", "Cursor right"};
static const char *const text_action_functions[6] = {"Select all", "Copy", "Paste", "Cut", "Undo", "Redo"};
static const char *const text_edit_functions[6]   = {"Enter", "Backspace", "Space", "Tab", "One-shot Shift", "Mouse button 1"};

static const char *const window_win_labels[6]     = {"DESK<", "TASK", "DESK>", "WIN<", "SHOW", "WIN>"};
static const char *const window_browser_labels[6] = {"BACK", "REFR", "FWD", "TAB<", "NEW", "TAB>"};

static const char *const window_win_functions[6]     = {"Previous desktop", "Task view", "Next desktop", "Previous window", "Show desktop", "Next window"};
static const char *const window_browser_functions[6] = {"Browser back", "Refresh page", "Browser forward", "Previous tab", "New tab", "Next tab"};

static const char *const game_nav_labels[6]  = {"ESC", "UP", "ENT", "LEFT", "DOWN", "RGHT"};
static const char *const game_wasd_labels[6] = {"SHFT", "W", "SPC", "A", "S", "D"};

static const char *const game_nav_functions[6]  = {"Back out of menu", "Menu up", "Confirm or interact", "Menu left", "Menu down", "Menu right"};
static const char *const game_wasd_functions[6] = {"One-shot sprint or crouch", "Move forward", "Jump or confirm", "Move left", "Move back", "Move right"};

static const char *const layer_legend[_LAYER_COUNT][PAD_KEY_COUNT] = {
    [_BASE]   = {"SEL",  "UP",   "BSPC", "LEFT", "ENT",  "RGHT", "UNDO", "DOWN", "REDO"},
    [_WINDOW] = {"SEL",  "BRO",  "AUX",  "DESK<","TASK", "DESK>","WIN<", "SHOW", "WIN>"},
    [_TEXT]   = {"SEL",  "ACT",  "ENT",  "HOME", "UP",   "END",  "LEFT", "DOWN", "RGHT"},
    [_MEDIA]  = {"SEL",  "PREV", "NEXT", "RWND", "PLAY", "FFWD", "VOL-", "MUTE", "VOL+"},
    [_RGB]    = {"SEL",  "MODE", "TOG",   "HUE+",  "HUE-",  "VAL+",  "SAT+",  "SAT-",  "VAL-"},
    [_RGBMOD] = {"SEL",  "MOD",  "I|0",  "FRME", "KEY",  "GAP",  "FREE1", "FREE2", "FREE3"},
    [_RGBADJ] = {"SEL",  "SPD-", "ADJST", "VAL-", "HUE+", "HUE-", "VAL+", "SAT+", "SAT-"},
    [_DEV]    = {"SEL",  "NAV",  "WASD", "ESC",  "UP",   "ENT",  "LEFT", "DOWN", "RGHT"},
    [_VSC]    = {"SEL",  "BAR",  "CHAT", "EXPL", "SRC",  "GH-A", "GHUB", "GPT",  "FREE"},
    [_PROMPT] = {"SEL",  "PICS", "ETSY", "SUM",  "REVW", "FIX",  "TEST", "EXPL", "COMMIT"},
    [_SELECT] = {"SEL",  "WIN",  "TXT",  "MED",  "----", "GAME", "VSC",  "RGB",  "PROMT"},
};

static const char *const layer_function[_LAYER_COUNT][PAD_KEY_COUNT] = {
    [_BASE]   = {"Select layer", "Arrow up", "Backspace", "Arrow left", "Enter", "Arrow right", "Undo", "Arrow down", "Redo"},
    [_WINDOW] = {"Select layer", "Browser combo", "Reserved", "Prev desktop", "Task view", "Next desktop", "Prev window", "Show desktop", "Next window"},
    [_TEXT]   = {"Select layer", "Hold text actions", "Enter", "Line start", "Cursor up", "Line end", "Cursor left", "Cursor down", "Cursor right"},
    [_MEDIA]  = {"Select layer", "Previous track", "Next track", "Rewind", "Play/Pause", "Fast forward", "Volume down", "Mute", "Volume up"},
    [_RGB]    = {"Select layer", "Hold RGB zone controls", "Toggle RGB output", "Hue up", "Hue down", "Brightness up", "Saturation up", "Saturation down", "Brightness down"},
    [_RGBMOD] = {"Select layer", "Hold RGB mod layer", "Toggle all RGB groups", "Toggle frame LEDs", "Toggle key LEDs", "Toggle gap LEDs", "Free slot", "Free slot", "Free slot"},
    [_RGBADJ] = {"Select layer", "Speed down", "Hold RGB adjust layer", "Brightness down", "Hue up", "Hue down", "Brightness up", "Saturation up", "Saturation down"},
    [_DEV]    = {"Select layer", "Switch to menu navigation", "Switch to movement controls", "Back out of menu", "Menu up", "Confirm or interact", "Menu left", "Menu down", "Menu right"},
    [_VSC]    = {"Select layer", "BAR mode", "CHAT mode", "Combo target 1", "Combo target 2", "Combo target 3", "Combo target 4", "Combo target 5", "Combo target 6"},
    [_PROMPT] = {"Select layer", "Prompt picture tools", "Prompt Etsy tools", "Prompt summarize", "Prompt review", "Prompt suggest fix", "Prompt write tests", "Prompt explain code", "Prompt commit message"},
    [_SELECT] = {"Select layer", "Go to window", "Go to text", "Go to media", "Unused", "Go to game", "Go to VSC", "Go to RGB", "Go to prompt"},
};

static const char *layer_name_short(uint8_t l) {
    switch (l) {
        case _BASE:   return "BASE";
        case _WINDOW: return "WIN";
        case _TEXT:   return "TXT";
        case _MEDIA:  return "MED";
        case _RGB:    return "RGB";
        case _RGBMOD: return "MOD";
        case _RGBADJ: return "ADJ";
        case _DEV:    return "GAME";
        case _VSC:    return "VSC";
        case _PROMPT: return "PRM";
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
        case _RGBMOD: return "RGB MOD";
        case _RGBADJ: return "ADJST";
        case _DEV:    return "GAME";
        case _VSC:    return "VSC";
        case _PROMPT: return "PROMPT";
        case _SELECT: return "SELECT";
        default:      return "BASE";
    }
}

static uint8_t canonical_rgb_layer(uint8_t layer) {
    return (layer == _RGBMOD || layer == _RGBADJ) ? _RGB : layer;
}

static uint8_t active_layer_raw(void) {
    return get_highest_layer(layer_state | default_layer_state);
}

static vsc_mode_t current_vsc_preview_mode(void) {
    if (vsc_mode != VSC_MODE_NONE) return vsc_mode;
    return last_vsc_mode;
}

static text_mode_t current_text_preview_mode(void) {
    if (text_action_held) return TEXT_MODE_ACTIONS;
    if (text_edit_held) return TEXT_MODE_EDIT;
    return text_mode;
}

static window_mode_t current_window_preview_mode(void) {
    return window_browser_held ? WINDOW_MODE_BROWSER : WINDOW_MODE_WIN;
}

static game_mode_t current_game_preview_mode(void) {
    return game_mode;
}

static const char *encoder_function_for_layer(uint8_t layer) {
    switch (layer) {
        case _BASE:   return "Wheel scroll up/down";
        case _WINDOW: return window_browser_held ? "Browser page prev/next" : "Alt-Tab window switch";
        case _TEXT:   return encoder_btn_pressed ? "Select text left/right" : "Move cursor left/right";
        case _MEDIA:  return "Volume up/down";
        case _RGB:    return "RGB brightness +/-";
        case _RGBMOD: return "RGB brightness +/-";
        case _RGBADJ: return "RGB brightness +/-";
        case _DEV:    return "Weapon or inventory scroll";
        case _VSC:    return "VSCode page prev/next";
        case _PROMPT: return "VSCode page prev/next";
        case _SELECT: return encoder_btn_pressed ? "Choose target layer" : "Hold encoder btn first";
        default:      return "Encoder fallback volume";
    }
}

static const char *text_label_for_mode(text_mode_t mode, uint8_t index) {
    if (index == 0) return "SEL";
    if (index == 1) return "ACT";
    if (index == 2) return "ENT";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        switch (mode) {
            case TEXT_MODE_ACTIONS: return text_action_labels[slot];
            case TEXT_MODE_EDIT:    return text_edit_labels[slot];
            case TEXT_MODE_WIN:
            default:                return text_win_labels[slot];
        }
    }
    return "----";
}

static const char *text_function_for_mode(text_mode_t mode, uint8_t index) {
    if (index == 0) return "Select layer";
    if (index == 1) return "Hold text actions";
    if (index == 2) return "Enter";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        switch (mode) {
            case TEXT_MODE_ACTIONS: return text_action_functions[slot];
            case TEXT_MODE_EDIT:    return text_edit_functions[slot];
            case TEXT_MODE_WIN:
            default:                return text_win_functions[slot];
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
        return mode == WINDOW_MODE_BROWSER ? window_browser_labels[slot] : window_win_labels[slot];
    }
    return "----";
}

static const char *window_function_for_mode(window_mode_t mode, uint8_t index) {
    if (index == 0) return "Select layer";
    if (index == 1) return "Hold browser controls";
    if (index == 2) return "Reserved for later";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        return mode == WINDOW_MODE_BROWSER ? window_browser_functions[slot] : window_win_functions[slot];
    }
    return "Unknown";
}

static const char *text_label_for(uint8_t index) { return text_label_for_mode(current_text_preview_mode(), index); }
static const char *text_function_for(uint8_t index) { return text_function_for_mode(current_text_preview_mode(), index); }
static const char *window_label_for(uint8_t index) { return window_label_for_mode(current_window_preview_mode(), index); }
static const char *window_function_for(uint8_t index) { return window_function_for_mode(current_window_preview_mode(), index); }

static const char *rgb_label_for_mode(bool mode_held, uint8_t index) {
    static const char *const normal_labels[PAD_KEY_COUNT] = {
        "SEL", "MODE", "TOG",
        "HUE+", "HUE-", "VAL+",
        "SAT+", "SAT-", "VAL-"
    };
    static const char *const mode_labels[PAD_KEY_COUNT] = {
        "SEL", "MODE", "ALL",
        "FRME", "KEY",  "GAP",
        "----", "----", "----"
    };

    if (index >= PAD_KEY_COUNT) return "----";
    return mode_held ? mode_labels[index] : normal_labels[index];
}

static const char *rgb_function_for_mode(bool mode_held, uint8_t index) {
    static const char *const normal_functions[PAD_KEY_COUNT] = {
        "Select layer", "Hold RGB zone controls", "Toggle RGB output",
        "Hue up", "Hue down", "Brightness up",
        "Saturation up", "Saturation down", "Brightness down"
    };
    static const char *const mode_functions[PAD_KEY_COUNT] = {
        "Select layer", "Hold RGB zone controls", "Toggle all LED zones",
        "Toggle frame LEDs", "Toggle key LEDs", "Toggle gap LEDs",
        "Unused", "Unused", "Unused"
    };

    if (index >= PAD_KEY_COUNT) return "Unknown";
    return mode_held ? mode_functions[index] : normal_functions[index];
}

static const char *rgb_label_for(uint8_t index) { return rgb_label_for_mode(rgb_mode_held, index); }
static const char *rgb_function_for(uint8_t index) { return rgb_function_for_mode(rgb_mode_held, index); }

static const char *game_label_for_mode(game_mode_t mode, uint8_t index) {
    if (index == 0) return "SEL";
    if (index == 1) return "NAV";
    if (index == 2) return "WASD";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        return mode == GAME_MODE_NAV ? game_nav_labels[slot] : game_wasd_labels[slot];
    }
    return "----";
}

static const char *game_function_for_mode(game_mode_t mode, uint8_t index) {
    if (index == 0) return "Select layer";
    if (index == 1) return "Switch to menu navigation";
    if (index == 2) return "Switch to movement controls";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        return mode == GAME_MODE_NAV ? game_nav_functions[slot] : game_wasd_functions[slot];
    }
    return "Unknown";
}

static const char *game_label_for(uint8_t index) { return game_label_for_mode(current_game_preview_mode(), index); }
static const char *game_function_for(uint8_t index) { return game_function_for_mode(current_game_preview_mode(), index); }

static const char *game_mode_name(game_mode_t mode) {
    return mode == GAME_MODE_NAV ? "NAV" : "WASD";
}

static const char *prompt_label_for_mode(prompt_mode_t mode, uint8_t index) {
    if (index == 0) return "SEL";
    if (index == 1) return "PICS";
    if (index == 2) return "ETSY";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        switch (mode) {
            case PROMPT_MODE_PICS: return prompt_pics_labels[slot];
            case PROMPT_MODE_ETSY: return prompt_etsy_labels[slot];
            case PROMPT_MODE_BASE:
            default:               return prompt_base_labels[slot];
        }
    }
    return "----";
}

static const char *prompt_function_for_mode(prompt_mode_t mode, uint8_t index) {
    if (index == 0) return "Select layer";
    if (index == 1) return "Switch to picture prompts";
    if (index == 2) return "Switch to Etsy prompts";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        switch (mode) {
            case PROMPT_MODE_PICS: return prompt_pics_functions[slot];
            case PROMPT_MODE_ETSY: return prompt_etsy_functions[slot];
            case PROMPT_MODE_BASE:
            default:               return prompt_base_functions[slot];
        }
    }
    return "Unknown";
}

static const char *prompt_label_for(uint8_t index) { return prompt_label_for_mode(prompt_mode, index); }
static const char *prompt_function_for(uint8_t index) { return prompt_function_for_mode(prompt_mode, index); }

static const char *prompt_mode_name(prompt_mode_t mode) {
    switch (mode) {
        case PROMPT_MODE_PICS: return "PICS";
        case PROMPT_MODE_ETSY: return "ETSY";
        case PROMPT_MODE_BASE:
        default:               return "BASE";
    }
}

static const char *vsc_label_for(vsc_mode_t mode, uint8_t index) {
    if (index == 0) return "SEL";
    if (index == 1) return "BAR";
    if (index == 2) return "CHAT";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        return mode == VSC_MODE_CHAT ? vsc_chat_labels[slot] : vsc_bar_labels[slot];
    }
    return "----";
}

static const char *vsc_function_for(vsc_mode_t mode, uint8_t index) {
    if (index == 0) return "Select layer";
    if (index == 1) return "Hold BAR mode";
    if (index == 2) return "Hold CHAT mode";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        return mode == VSC_MODE_CHAT ? vsc_chat_functions[slot] : vsc_bar_functions[slot];
    }
    return "Unknown";
}

static void update_select_layer_state(void) {
    if (matrix_select_held) {
        layer_on(_SELECT);
    } else {
        layer_off(_SELECT);
    }
}

static uint8_t slot_for_layer(uint8_t layer) {
    layer = canonical_rgb_layer(layer);

    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        if (select_slots[i].selectable && select_slots[i].layer == layer) {
            return i;
        }
    }
    return 0;
}

static uint8_t palette_floor(uint8_t value, uint8_t minimum) {
    return value < minimum ? minimum : value;
}

static uint8_t via_palette_index_for_slot(uint8_t slot) {
    for (uint8_t i = 0; i < VIA_LAYER_SLOT_COUNT; i++) {
        if (via_layer_slots[i] == slot) return i;
    }
    return 0;
}

static hsv_config_t palette_for_layer(uint8_t layer) {
    uint8_t slot = slot_for_layer(layer);
    return select_slots[slot].selectable
        ? (hsv_config_t){select_slots[slot].hue, select_slots[slot].sat, select_slots[slot].val}
        : via_default_palette[0];
}

static uint8_t via_palette_index_for_layer(uint8_t layer) {
    return via_palette_index_for_slot(slot_for_layer(layer));
}

static rgb_effect_mode_t effect_for_layer(uint8_t layer) {
#ifdef VIA_ENABLE
    uint8_t index = via_palette_index_for_layer(layer);
    if (index < VIA_LAYER_SLOT_COUNT && via_user_config.layer_effect[index] < RGB_EFFECT_COUNT) {
        return (rgb_effect_mode_t)via_user_config.layer_effect[index];
    }
#endif
    return RGB_EFFECT_WILD;
}

static uint8_t effect_speed_for_layer(uint8_t layer) {
#ifdef VIA_ENABLE
    uint8_t index = via_palette_index_for_layer(layer);
    if (index < VIA_LAYER_SLOT_COUNT && via_user_config.layer_speed[index] >= 1) {
        return via_user_config.layer_speed[index];
    }
#endif
    return 128;
}

#ifdef RGBLIGHT_ENABLE
static void render_rgb_layer_visuals(void);
#endif

static uint16_t effect_period_for_layer(uint8_t layer, uint16_t base_period) {
    uint8_t speed = effect_speed_for_layer(layer);
    if (speed < 1) speed = 1;
    uint32_t period = ((uint32_t)base_period * 128UL) / speed;
    if (period < 40) period = 40;
    if (period > 20000) period = 20000;
    return (uint16_t)period;
}

static void adjust_layer_brightness(uint8_t layer, int16_t delta) {
    uint8_t slot = slot_for_layer(layer);
    uint16_t value = select_slots[slot].val;

    if (delta < 0) {
        uint16_t step = (uint16_t)(-delta);
        value = value > step ? (value - step) : 0;
    } else {
        value = value + (uint16_t)delta;
        if (value > UINT8_MAX) value = UINT8_MAX;
    }

    select_slots[slot].val = (uint8_t)value;

#ifdef VIA_ENABLE
    uint8_t index = via_palette_index_for_layer(layer);
    if (index < VIA_LAYER_SLOT_COUNT) {
        via_user_config.layer_palette[index].val = (uint8_t)value;
    }
#endif

#ifdef RGBLIGHT_ENABLE
    if (canonical_rgb_layer(active_layer_raw()) == canonical_rgb_layer(layer)) {
        render_rgb_layer_visuals();
    }
#endif
}

static void adjust_layer_hue(uint8_t layer, int16_t delta) {
    uint8_t slot = slot_for_layer(layer);
    uint8_t value = (uint8_t)(select_slots[slot].hue + delta);

    select_slots[slot].hue = value;

#ifdef VIA_ENABLE
    uint8_t index = via_palette_index_for_layer(layer);
    if (index < VIA_LAYER_SLOT_COUNT) {
        via_user_config.layer_palette[index].hue = value;
    }
#endif

#ifdef RGBLIGHT_ENABLE
    if (canonical_rgb_layer(active_layer_raw()) == canonical_rgb_layer(layer)) {
        render_rgb_layer_visuals();
    }
#endif
}

static void adjust_layer_saturation(uint8_t layer, int16_t delta) {
    uint8_t slot = slot_for_layer(layer);
    uint16_t value = select_slots[slot].sat;

    if (delta < 0) {
        uint16_t step = (uint16_t)(-delta);
        value = value > step ? (value - step) : 0;
    } else {
        value += (uint16_t)delta;
        if (value > UINT8_MAX) value = UINT8_MAX;
    }

    select_slots[slot].sat = (uint8_t)value;

#ifdef VIA_ENABLE
    uint8_t index = via_palette_index_for_layer(layer);
    if (index < VIA_LAYER_SLOT_COUNT) {
        via_user_config.layer_palette[index].sat = (uint8_t)value;
    }
#endif

#ifdef RGBLIGHT_ENABLE
    if (canonical_rgb_layer(active_layer_raw()) == canonical_rgb_layer(layer)) {
        render_rgb_layer_visuals();
    }
#endif
}

#ifdef VIA_ENABLE
static void apply_palette_entry(uint8_t index) {
    if (index >= VIA_LAYER_SLOT_COUNT) return;
    uint8_t slot = via_layer_slots[index];
    select_slots[slot].hue = via_user_config.layer_palette[index].hue;
    select_slots[slot].sat = via_user_config.layer_palette[index].sat;
    select_slots[slot].val = via_user_config.layer_palette[index].val;
}

static void apply_via_runtime_config(void) {
    oled_view = via_user_config.oled_view == OLED_VIEW_LAST_KEY ? OLED_VIEW_LAST_KEY : OLED_VIEW_LEGEND;
    for (uint8_t i = 0; i < VIA_LAYER_SLOT_COUNT; i++) apply_palette_entry(i);
}

static void set_via_config_defaults(void) {
    via_user_config.signature = 0x94;
    via_user_config.layout_version = REWORKED_LAYOUT_VERSION;
    via_user_config.oled_view = OLED_VIEW_LEGEND;
    via_user_config.fx_mode = 0;

    for (uint8_t i = 0; i < VIA_LAYER_SLOT_COUNT; i++) {
        via_user_config.layer_effect[i] = RGB_EFFECT_WILD;
        via_user_config.layer_speed[i] = 128;
        via_user_config.layer_palette[i] = via_default_palette[i];
    }

    apply_via_runtime_config();
}

static void save_via_config(void) {
    via_update_custom_config(&via_user_config, 0, sizeof(via_user_config));
}

static void load_via_config(void) {
    via_read_custom_config(&via_user_config, 0, sizeof(via_user_config));

    bool invalid_config = false;

    if (via_user_config.signature != 0x94) invalid_config = true;
    if (via_user_config.layout_version != REWORKED_LAYOUT_VERSION) invalid_config = true;
    if (via_user_config.oled_view > OLED_VIEW_LAST_KEY) invalid_config = true;
    if (via_user_config.fx_mode > 1) invalid_config = true;

    for (uint8_t i = 0; i < VIA_LAYER_SLOT_COUNT; i++) {
        if (via_user_config.layer_effect[i] >= RGB_EFFECT_COUNT) invalid_config = true;
        if (via_user_config.layer_speed[i] < 1) invalid_config = true;
    }

    if (invalid_config) {
        eeconfig_init();
        set_via_config_defaults();
        save_via_config();
        return;
    }

    apply_via_runtime_config();
}

static void via_config_set_value(uint8_t *data) {
    uint8_t *value_id = &data[0];
    uint8_t *value_data = &data[1];

    switch (*value_id) {
        case id_via_oled_view:
            via_user_config.oled_view = value_data[0] == OLED_VIEW_LAST_KEY ? OLED_VIEW_LAST_KEY : OLED_VIEW_LEGEND;
            oled_view = via_user_config.oled_view;
            break;
        case id_via_fx_mode:
            via_user_config.fx_mode = value_data[0] ? 1 : 0;
            break;
        case id_via_rgb_effect:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                via_user_config.layer_effect[value_data[0]] = value_data[1] < RGB_EFFECT_COUNT ? value_data[1] : RGB_EFFECT_WILD;
            }
            break;
        case id_via_layer_effect_speed:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                via_user_config.layer_speed[value_data[0]] = value_data[1] < 1 ? 1 : value_data[1];
            }
            break;
        case id_via_layer_color:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                via_user_config.layer_palette[value_data[0]].hue = value_data[1];
                via_user_config.layer_palette[value_data[0]].sat = value_data[2];
                apply_palette_entry(value_data[0]);
            }
            break;
        case id_via_layer_brightness:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                via_user_config.layer_palette[value_data[0]].val = value_data[1];
                apply_palette_entry(value_data[0]);
            }
            break;
    }
}

static void via_config_get_value(uint8_t *data) {
    uint8_t *value_id = &data[0];
    uint8_t *value_data = &data[1];

    switch (*value_id) {
        case id_via_oled_view:
            value_data[0] = via_user_config.oled_view;
            break;
        case id_via_fx_mode:
            value_data[0] = via_user_config.fx_mode;
            break;
        case id_via_rgb_effect:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                value_data[1] = via_user_config.layer_effect[value_data[0]];
            }
            break;
        case id_via_layer_effect_speed:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                value_data[1] = via_user_config.layer_speed[value_data[0]];
            }
            break;
        case id_via_layer_color:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                uint8_t index = value_data[0];
                value_data[1] = via_user_config.layer_palette[index].hue;
                value_data[2] = via_user_config.layer_palette[index].sat;
            }
            break;
        case id_via_layer_brightness:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                value_data[1] = via_user_config.layer_palette[value_data[0]].val;
            }
            break;
    }
}

void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    (void)length;

    uint8_t *command_id = &data[0];
    uint8_t *channel_id = &data[1];
    uint8_t *value_id_and_data = &data[2];

    if (*channel_id == 0) {
        switch (*command_id) {
            case id_custom_set_value: via_config_set_value(value_id_and_data); break;
            case id_custom_get_value: via_config_get_value(value_id_and_data); break;
            case id_custom_save:      save_via_config(); break;
            default:                  *command_id = id_unhandled; break;
        }
        return;
    }

    *command_id = id_unhandled;
}
#endif

static void sync_selector_target_from_cursor(void) {
    if (select_cursor < PAD_KEY_COUNT && select_slots[select_cursor].selectable) {
        selector_target = select_slots[select_cursor].layer;
    }
}

static uint8_t next_select_slot(uint8_t idx, bool clockwise) {
    for (uint8_t step = 0; step < PAD_KEY_COUNT; step++) {
        idx = clockwise ? ((idx + 1) % PAD_KEY_COUNT) : ((idx + PAD_KEY_COUNT - 1) % PAD_KEY_COUNT);
        if (select_slots[idx].selectable || idx == 4) return idx;
    }

    return idx;
}

static const char *legend_label_for(uint8_t layer, uint8_t row, uint8_t col) {
    uint8_t index = (row * MATRIX_COLS) + col;
    if (layer >= _LAYER_COUNT || index >= PAD_KEY_COUNT) return "----";
    if (layer == _TEXT) return text_label_for(index);
    if (layer == _WINDOW) return window_label_for(index);
    if (layer == _RGB) return rgb_label_for(index);
    if (layer == _DEV) return game_label_for(index);
    if (layer == _VSC) return vsc_label_for(current_vsc_preview_mode(), index);
    if (layer == _PROMPT) return prompt_label_for(index);
    return layer_legend[layer][index];
}

static const char *function_label_for(uint8_t layer, uint8_t row, uint8_t col) {
    uint8_t index = (row * MATRIX_COLS) + col;
    if (layer >= _LAYER_COUNT || index >= PAD_KEY_COUNT) return "Unknown";
    if (layer == _TEXT) return text_function_for(index);
    if (layer == _WINDOW) return window_function_for(index);
    if (layer == _RGB) return rgb_function_for(index);
    if (layer == _DEV) return game_function_for(index);
    if (layer == _VSC) return vsc_function_for(current_vsc_preview_mode(), index);
    if (layer == _PROMPT) return prompt_function_for(index);
    return layer_function[layer][index];
}

static const char *last_key_label_for(void) {
    uint8_t index = (last_row * MATRIX_COLS) + last_col;
    if (last_key_layer >= _LAYER_COUNT || index >= PAD_KEY_COUNT) return "----";
    if (last_key_layer == _TEXT) return text_label_for_mode(last_key_text_mode, index);
    if (last_key_layer == _WINDOW) return window_label_for_mode(last_key_window_mode, index);
    if (last_key_layer == _RGB) return rgb_label_for_mode(last_key_rgb_mode, index);
    if (last_key_layer == _DEV) return game_label_for_mode(last_key_game_mode, index);
    if (last_key_layer == _VSC) return vsc_label_for(last_key_vsc_mode, index);
    if (last_key_layer == _PROMPT) return prompt_label_for_mode(last_key_prompt_mode, index);
    return layer_legend[last_key_layer][index];
}

static const char *last_key_function_for(void) {
    uint8_t index = (last_row * MATRIX_COLS) + last_col;
    if (last_key_layer >= _LAYER_COUNT || index >= PAD_KEY_COUNT) return "Unknown";
    if (last_key_layer == _TEXT) return text_function_for_mode(last_key_text_mode, index);
    if (last_key_layer == _WINDOW) return window_function_for_mode(last_key_window_mode, index);
    if (last_key_layer == _RGB) return rgb_function_for_mode(last_key_rgb_mode, index);
    if (last_key_layer == _DEV) return game_function_for_mode(last_key_game_mode, index);
    if (last_key_layer == _VSC) return vsc_function_for(last_key_vsc_mode, index);
    if (last_key_layer == _PROMPT) return prompt_function_for_mode(last_key_prompt_mode, index);
    return layer_function[last_key_layer][index];
}

static void send_vsc_command(const char *command) {
    if (command == NULL || command[0] == '\0') return;

    tap_code16(C(S(KC_P)));
    wait_ms(30);
    send_string(command);
    tap_code(KC_ENT);
}

static void trigger_vsc_target(uint8_t slot) {
    if (slot >= 6) return;

    if (active_layer_raw() == _PROMPT) {
        send_vsc_command("GitHub Copilot Chat: Focus on Chat View");
        wait_ms(30);

        switch (prompt_mode) {
            case PROMPT_MODE_PICS: send_string(prompt_pics_macros[slot]); break;
            case PROMPT_MODE_ETSY: send_string(prompt_etsy_macros[slot]); break;
            case PROMPT_MODE_BASE:
            default:               send_string(vsc_chat_macros[slot]); break;
        }

        return;
    }

    vsc_mode_t mode = current_vsc_preview_mode();
    if (mode == VSC_MODE_NONE) return;

    if (mode == VSC_MODE_BAR) {
        send_vsc_command(vsc_bar_commands[slot]);
    } else if (mode == VSC_MODE_CHAT) {
        send_string(vsc_chat_macros[slot]);
    }
}

static void tap_text_target(uint8_t slot) {
    if (slot >= 6) return;

    switch (current_text_preview_mode()) {
        case TEXT_MODE_ACTIONS:
            switch (slot) {
                case 0: tap_code16(C(KC_A)); break;
                case 1: tap_code16(C(KC_C)); break;
                case 2: tap_code16(C(KC_V)); break;
                case 3: tap_code16(C(KC_X)); break;
                case 4: tap_code16(C(KC_Y)); break;
                case 5: tap_code16(C(KC_Z)); break;
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

        case TEXT_MODE_WIN:
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
    if (slot >= 6) return;

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

static void tap_game_target(uint8_t slot) {
    if (slot >= 6) return;

    if (current_game_preview_mode() == GAME_MODE_NAV) {
        switch (slot) {
            case 0: tap_code(KC_ESC); break;
            case 1: tap_code(KC_UP); break;
            case 2: tap_code(KC_ENT); break;
            case 3: tap_code(KC_LEFT); break;
            case 4: tap_code(KC_DOWN); break;
            case 5: tap_code(KC_RGHT); break;
        }
    } else {
        switch (slot) {
            case 0: set_oneshot_mods(MOD_LSFT); break;
            case 1: tap_code(KC_W); break;
            case 2: tap_code(KC_SPC); break;
            case 3: tap_code(KC_A); break;
            case 4: tap_code(KC_S); break;
            case 5: tap_code(KC_D); break;
        }
    }
}

static uint8_t triwave8_period(uint32_t now, uint16_t period_ms, uint8_t phase) {
    if (period_ms == 0) return 0;

    uint32_t t = (now + ((uint32_t)phase * period_ms) / 255) % period_ms;
    uint32_t x = (t * 510UL) / period_ms;

    return (x > 255) ? (uint8_t)(510 - x) : (uint8_t)x;
}

static uint8_t pulse_val(uint32_t now, uint16_t period, uint8_t phase, uint8_t min_v, uint8_t max_v) {
    uint8_t tri = triwave8_period(now, period, phase);
    return (uint8_t)(min_v + ((uint16_t)(max_v - min_v) * tri) / 255);
}


static bool led_is_frame(uint8_t led_index) {
    return led_index >= RGB_FRAME_LED_FIRST;
}

static bool led_is_gap(uint8_t led_index) {
    if (led_is_frame(led_index)) return false;
    uint8_t col = led_index % 5;
    return col == 1 || col == 3;
}

static bool led_is_mapped_key(uint8_t led_index) {
    if (led_is_frame(led_index)) return false;
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        if (key_led_map[i] == led_index) return true;
    }
    return false;
}

static bool rgb_zone_enabled_for_led(uint8_t led_index) {
    if (!rgb_output_enabled) return false;
    if (led_index >= RGB_FRAME_LED_FIRST) return (rgb_zone_mask & RGB_ZONE_FRAME) != 0;
    if (led_is_gap(led_index)) return (rgb_zone_mask & RGB_ZONE_GAP) != 0;
    if (led_is_mapped_key(led_index)) return (rgb_zone_mask & RGB_ZONE_KEY) != 0;
    return false;
}

static uint8_t led_row_index(uint8_t led_index) { return led_index / 5; }
static uint8_t led_col_index(uint8_t led_index) { return led_index % 5; }

static uint8_t distance_u8(uint8_t a, uint8_t b) {
    return a > b ? (a - b) : (b - a);
}

static uint8_t clamp_add_u8(uint8_t a, uint8_t b) {
    uint16_t v = (uint16_t)a + b;
    return v > 255 ? 255 : (uint8_t)v;
}

static uint8_t scale_val(uint8_t value, uint8_t scale) {
    return (uint8_t)(((uint16_t)value * scale) / 255);
}

static uint8_t wrap_distance(uint8_t a, uint8_t b, uint8_t count) {
    uint8_t d1 = a > b ? (a - b) : (b - a);
    uint8_t d2 = count - d1;
    return d1 < d2 ? d1 : d2;
}

static uint8_t ping_pong_index(uint32_t now, uint16_t period_ms, uint8_t count, uint8_t phase) {
    if (count <= 1) return 0;
    return (uint8_t)(((uint16_t)triwave8_period(now, period_ms, phase) * (count - 1)) / 255);
}

static void toggle_rgb_zone_bits(uint8_t zone_bits) {
    rgb_zone_mask ^= zone_bits;

#ifdef RGBLIGHT_ENABLE
    render_rgb_layer_visuals();
#endif
}

static void toggle_rgb_all_zones(void) {
    rgb_zone_mask = rgb_zone_mask == RGB_ZONE_ALL ? 0 : RGB_ZONE_ALL;

#ifdef RGBLIGHT_ENABLE
    render_rgb_layer_visuals();
#endif
}

static void toggle_rgb_output_state(void) {
    rgb_output_enabled = !rgb_output_enabled;

#ifdef RGBLIGHT_ENABLE
    render_rgb_layer_visuals();
#endif
}

#ifdef RGBLIGHT_ENABLE
static void flush_led_frame(void) {
    rgblight_driver.flush();
}

static void set_led_hsv(uint8_t led_index, uint8_t h, uint8_t s, uint8_t v) {
    if (led_index >= RGBLIGHT_LED_COUNT) return;
    if (!rgb_zone_enabled_for_led(led_index)) {
        rgblight_driver.set_color(led_index, 0, 0, 0);
        return;
    }
    if (v > RGBLIGHT_LIMIT_VAL) v = RGBLIGHT_LIMIT_VAL;
    rgb_t rgb = hsv_to_rgb((hsv_t){h, s, v});
    rgblight_driver.set_color(led_index, rgb.r, rgb.g, rgb.b);
}

static void set_key_hsv(uint8_t key_index, uint8_t h, uint8_t s, uint8_t v) {
    if (key_index >= PAD_KEY_COUNT) return;
    set_led_hsv(key_led_map[key_index], h, s, v);
}

static void set_gap_hsv(uint8_t row, uint8_t gap, uint8_t h, uint8_t s, uint8_t v) {
    if (row >= 3 || gap >= 2) return;
    set_led_hsv((row * 5) + 1 + (gap * 2), h, s, v);
}

static void clear_all_keys(void) {
    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        set_led_hsv(i, 0, 0, 0);
    }
}

static void render_base_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(_BASE);
    uint16_t slow = effect_period_for_layer(_BASE, 6800);
    uint16_t fast = effect_period_for_layer(_BASE, 2300);

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t row = led_row_index(i);
        uint8_t col = led_col_index(i);
        uint8_t wave_a = triwave8_period(now, slow, (uint8_t)(row * 51 + col * 23));
        uint8_t wave_b = triwave8_period(now, fast, (uint8_t)(255 - col * 37 - row * 19));
        uint8_t hue = p.hue + (wave_a / 18) + row * 4;
        uint8_t val;

        if (led_is_gap(i)) {
            hue += 14;
            val = palette_floor(scale_val(p.val, 45 + (255 - wave_b) / 3), 8);
        } else {
            val = palette_floor(scale_val(p.val, 90 + wave_b / 2), 18);
        }

        set_led_hsv(i, hue, p.sat, val);
    }

    flush_led_frame();
}

static void render_window_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(_WINDOW);
    uint8_t left_head  = ping_pong_index(now, effect_period_for_layer(_WINDOW, 1450), 5, 0);
    uint8_t right_head = 4 - ping_pong_index(now, effect_period_for_layer(_WINDOW, 1700), 5, 96);

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t row = led_row_index(i);
        uint8_t col = led_col_index(i);
        uint8_t d1 = distance_u8(col, left_head);
        uint8_t d2 = distance_u8(col, right_head);
        uint8_t d = d1 < d2 ? d1 : d2;
        uint8_t hue = p.hue + row * 5 + (led_is_gap(i) ? 18 : 0);
        uint8_t val = palette_floor(p.val / 14, 6);

        if (d == 0) val = led_is_gap(i) ? palette_floor(p.val + 34, p.val) : p.val;
        else if (d == 1) val = palette_floor((p.val * 2) / 3, 24);
        else if (d == 2) val = palette_floor(p.val / 3, 12);

        if (window_browser_held && row == 1) {
            val = clamp_add_u8(val, 28);
            hue += 12;
        }

        set_led_hsv(i, hue, p.sat, val);
    }

    flush_led_frame();
}

static void render_text_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(_TEXT);
    uint8_t mode_bias = text_action_held ? 18 : (text_edit_held ? 34 : 0);
    uint16_t period = effect_period_for_layer(_TEXT, text_action_held ? 850 : (text_edit_held ? 1050 : 1600));
    uint8_t cursor = (now / (period / PAD_KEY_COUNT + 1)) % PAD_KEY_COUNT;

    clear_all_keys();

    for (uint8_t key = 0; key < PAD_KEY_COUNT; key++) {
        uint8_t d = wrap_distance(key, cursor, PAD_KEY_COUNT);
        uint8_t val = palette_floor(p.val / 18, 4);

        if (d == 0) val = palette_floor(p.val + 28, p.val);
        else if (d == 1) val = palette_floor((p.val * 3) / 5, 22);
        else if (d == 2) val = palette_floor(p.val / 4, 10);

        set_key_hsv(key, p.hue + mode_bias + key * 2, p.sat, val);
    }

    for (uint8_t row = 0; row < 3; row++) {
        for (uint8_t gap = 0; gap < 2; gap++) {
            uint8_t left_key = row * 3 + gap;
            uint8_t right_key = left_key + 1;
            uint8_t dl = wrap_distance(left_key, cursor, PAD_KEY_COUNT);
            uint8_t dr = wrap_distance(right_key, cursor, PAD_KEY_COUNT);
            uint8_t d = dl < dr ? dl : dr;
            uint8_t val = palette_floor(p.val / 24, 3);

            if (d == 0) val = palette_floor((p.val * 4) / 5, 24);
            else if (d == 1) val = palette_floor(p.val / 3, 12);

            set_gap_hsv(row, gap, p.hue + mode_bias + 12, p.sat, val);
        }
    }

    flush_led_frame();
}

static void render_media_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(_MEDIA);

    for (uint8_t row = 0; row < 3; row++) {
        uint8_t meter = (uint8_t)(((uint16_t)triwave8_period(now, effect_period_for_layer(_MEDIA, 900 + row * 250), row * 73) * 5) / 255);
        uint8_t kick = ((now / (220 + row * 37)) % 7) == 0;

        for (uint8_t col = 0; col < 5; col++) {
            uint8_t led = row * 5 + col;
            uint8_t hue = p.hue + row * 5 + col * 3;
            uint8_t val = palette_floor(p.val / 18, 5);

            if (col <= meter) val = palette_floor(scale_val(p.val, 210 - col * 24), 18);
            if (kick && col == 2) val = palette_floor(p.val + 40, p.val);

            if (led_is_gap(led)) {
                val = clamp_add_u8(val / 2, triwave8_period(now, 700, led * 23) / 5);
                hue += 9;
            }

            set_led_hsv(led, hue, p.sat, val);
        }
    }

    flush_led_frame();
}

static void render_rgb_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(_RGB);
    uint16_t spin = effect_period_for_layer(_RGB, 2400);
    uint8_t base = (uint8_t)((now * 255UL) / spin);

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t row = led_row_index(i);
        uint8_t col = led_col_index(i);
        uint8_t spark = triwave8_period(now, 520 + i * 17, i * 29);
        uint8_t hue = base + i * 19 + row * 11 + col * 5;
        uint8_t val = palette_floor(p.val / 9, 8);
        val = clamp_add_u8(val, spark / (led_is_gap(i) ? 4 : 3));

        if ((((now / 95) + i * 5) % 17) == 0) {
            val = palette_floor(p.val + 55, p.val);
        }

        set_led_hsv(i, hue, p.sat, val);
    }

    flush_led_frame();
}

static void render_dev_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(_DEV);

    if (game_mode == GAME_MODE_NAV) {
    uint16_t period = effect_period_for_layer(_DEV, 850);

    // 0..255 Welle: Keys hell, Gaps dunkel -> danach umgekehrt
    uint8_t swap = triwave8_period(now, period, 0);

    uint8_t key_val = palette_floor(
        scale_val(p.val, 60 + swap / 2),
        10
    );

    uint8_t gap_val = palette_floor(
        scale_val(p.val, 60 + (255 - swap) / 2),
        10
    );

    // kleine Laufbewegung über die Reihen, damit es nicht nur stumpf blinkt
    uint8_t row_head = ping_pong_index(now, effect_period_for_layer(_DEV, 1400), 3, 0);

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t row = led_row_index(i);
        uint8_t row_dist = distance_u8(row, row_head);

        bool gap = led_is_gap(i);

        uint8_t hue = p.hue + row * 5 + (gap ? 14 : 0);
        uint8_t val = gap ? gap_val : key_val;

        // aktive Reihe leicht boosten
        if (row_dist == 0) {
            val = palette_floor(val + 34, val);
        } else if (row_dist == 1) {
            val = palette_floor(val + 12, val);
        }

        set_led_hsv(i, hue, p.sat, val);
    }

    flush_led_frame();
    return;
}
    uint8_t boom = triwave8_period(now, effect_period_for_layer(_DEV, 1150), 0);

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t row = led_row_index(i);
        uint8_t col = led_col_index(i);
        uint8_t dist = distance_u8(row, 1) + distance_u8(col, 2);
        int16_t energy = (int16_t)boom - (int16_t)(dist * 48);
        uint8_t val = energy > 0 ? palette_floor((uint8_t)energy, 12) : palette_floor(p.val / 20, 4);

        if (led_is_gap(i)) val = clamp_add_u8(val / 2, triwave8_period(now, 600, i * 31) / 7);

        set_led_hsv(i, p.hue - dist * 5 + (led_is_gap(i) ? 10 : 0), p.sat, val);
    }

    flush_led_frame();
}

static void render_vsc_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(_VSC);
    vsc_mode_t mode = current_vsc_preview_mode();
    uint8_t packet = (now / effect_period_for_layer(_VSC, mode == VSC_MODE_CHAT ? 115 : 170)) % RGBLIGHT_LED_COUNT;
    uint8_t target_key = mode == VSC_MODE_CHAT ? 4 : 3;

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t d = wrap_distance(i, packet, RGBLIGHT_LED_COUNT);
        uint8_t val = palette_floor(p.val / 20, 4);

        if (d == 0) val = palette_floor(p.val + 35, p.val);
        else if (d == 1) val = palette_floor((p.val * 3) / 5, 20);
        else if (d == 2) val = palette_floor(p.val / 4, 10);

        if (led_is_gap(i)) val = clamp_add_u8(val, triwave8_period(now, 900, i * 17) / 9);

        set_led_hsv(i, p.hue + (mode == VSC_MODE_CHAT ? 18 : 0) + led_row_index(i) * 3, p.sat, val);
    }

    uint8_t blink = ((now / 230) % 2) ? palette_floor(p.val + 40, p.val) : palette_floor(p.val / 5, 12);
    set_key_hsv(target_key, p.hue + (mode == VSC_MODE_CHAT ? 25 : 0), p.sat, blink);
    flush_led_frame();
}

static void render_prompt_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(_PROMPT);
    uint8_t mode_bias = prompt_mode == PROMPT_MODE_PICS ? 24 : (prompt_mode == PROMPT_MODE_ETSY ? 10 : 0);
    uint8_t orbit = (now / effect_period_for_layer(_PROMPT, 130)) % RGBLIGHT_LED_COUNT;
    uint8_t bloom = triwave8_period(now, effect_period_for_layer(_PROMPT, 1800), 0);

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t row = led_row_index(i);
        uint8_t col = led_col_index(i);
        uint8_t center_dist = distance_u8(row, 1) + distance_u8(col, 2);
        int16_t val_i = (int16_t)bloom - (int16_t)(center_dist * 38);
        uint8_t val = val_i > 0 ? palette_floor((uint8_t)val_i, 10) : palette_floor(p.val / 22, 4);
        uint8_t od = wrap_distance(i, orbit, RGBLIGHT_LED_COUNT);

        if (od == 0) val = palette_floor(p.val + 45, p.val);
        else if (od == 1 && led_is_gap(i)) val = palette_floor((p.val * 2) / 3, 22);

        set_led_hsv(i, p.hue + mode_bias + center_dist * 4, p.sat, val);
    }

    flush_led_frame();
}

static void render_select_wild(void) {
    uint32_t now = timer_read32();
    uint8_t target_slot = slot_for_layer(selector_target);
    uint8_t chase = (now / effect_period_for_layer(_SELECT, 115)) % RGBLIGHT_LED_COUNT;

    clear_all_keys();

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t row = led_row_index(i);
        uint8_t col = led_col_index(i);
        uint8_t owner_key = row * 3 + (col / 2);
        if (owner_key >= PAD_KEY_COUNT) owner_key = PAD_KEY_COUNT - 1;

        const select_slot_t *owner = &select_slots[owner_key];
        uint8_t val = led_is_gap(i) ? 7 : 3;
        uint8_t sat = led_is_gap(i) ? palette_floor(owner->sat / 2, 30) : owner->sat;
        uint8_t hue = owner->hue;

        if (i == chase && led_is_gap(i)) val = 78;

        set_led_hsv(i, hue, sat, val);
    }

    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        const select_slot_t *slot = &select_slots[i];
        uint8_t val = slot->selectable ? 28 : 10;
        uint8_t sat = slot->sat;
        uint8_t hue = slot->hue;

        if (i == 4) {
            sat = 0;
            val = 22;
        }

        if (i == target_slot && i != select_cursor && slot->selectable) val = 76;

        if (i == select_cursor) {
            if (slot->selectable) val = pulse_val(now, 720, 0, 86, 190);
            else {
                sat = 0;
                val = pulse_val(now, 720, 0, 46, 132);
            }
        }

        set_key_hsv(i, hue, sat, val);
    }

    flush_led_frame();
}

static void render_effect_solid(uint8_t layer) {
    hsv_config_t p = palette_for_layer(layer);

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        set_led_hsv(i, p.hue + (led_is_gap(i) ? 6 : 0), p.sat, led_is_gap(i) ? palette_floor((p.val * 2) / 3, 16) : p.val);
    }

    flush_led_frame();
}

static void render_effect_breathing(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 1900);
    uint8_t key_val = pulse_val(now, period, 0, palette_floor(p.val / 9, 8), p.val);
    uint8_t gap_val = pulse_val(now, period, 128, palette_floor(p.val / 12, 5), palette_floor((p.val * 3) / 4, 18));

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        set_led_hsv(i, p.hue + (led_is_gap(i) ? 10 : 0), p.sat, led_is_gap(i) ? gap_val : key_val);
    }

    flush_led_frame();
}

static void render_effect_running(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 850);
    uint8_t head = (now / (period / RGBLIGHT_LED_COUNT + 1)) % RGBLIGHT_LED_COUNT;

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t d = wrap_distance(i, head, RGBLIGHT_LED_COUNT);
        uint8_t val = palette_floor(p.val / 16, 5);

        if (d == 0) val = p.val;
        else if (d == 1) val = palette_floor((p.val * 2) / 3, 22);
        else if (d == 2) val = palette_floor(p.val / 3, 12);

        set_led_hsv(i, p.hue + d * 3, p.sat, val);
    }

    flush_led_frame();
}

static void render_effect_twinkle(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 640);
    uint8_t spark_a = ((now / (period / 3 + 1)) * 7 + 2) % RGBLIGHT_LED_COUNT;
    uint8_t spark_b = ((now / (period / 5 + 1)) * 11 + 6) % RGBLIGHT_LED_COUNT;

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t shimmer = triwave8_period(now, 700 + i * 53, i * 31);
        uint8_t val = palette_floor(p.val / 18, 4);
        val = clamp_add_u8(val, shimmer / 12);

        if (i == spark_a || i == spark_b) val = palette_floor(p.val + 48, p.val);

        set_led_hsv(i, p.hue + i * 2 + (led_is_gap(i) ? 8 : 0), p.sat, val);
    }

    flush_led_frame();
}

static void render_effect_pulse(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 1100);
    uint8_t blast = triwave8_period(now, period, 0);

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t d = distance_u8(led_row_index(i), 1) + distance_u8(led_col_index(i), 2);
        int16_t e = (int16_t)blast - (int16_t)(d * 48);
        uint8_t val = e > 0 ? palette_floor((uint8_t)e, 8) : palette_floor(p.val / 22, 4);
        set_led_hsv(i, p.hue + d * 5, p.sat, val);
    }

    flush_led_frame();
}

static void render_effect_comet(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 930);
    uint8_t head = (now / (period / RGBLIGHT_LED_COUNT + 1)) % RGBLIGHT_LED_COUNT;

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t d = (head + RGBLIGHT_LED_COUNT - i) % RGBLIGHT_LED_COUNT;
        uint8_t val = palette_floor(p.val / 20, 4);

        if (d == 0) val = p.val;
        else if (d == 1) val = palette_floor((p.val * 3) / 4, 24);
        else if (d == 2) val = palette_floor(p.val / 2, 16);
        else if (d == 3) val = palette_floor(p.val / 4, 8);

        set_led_hsv(i, p.hue + d * 4, p.sat, val);
    }

    flush_led_frame();
}

static void render_effect_scan(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint8_t col_head = ping_pong_index(now, effect_period_for_layer(layer, 1200), 5, 0);

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t d = distance_u8(led_col_index(i), col_head);
        uint8_t val = palette_floor(p.val / 18, 5);

        if (d == 0) val = p.val;
        else if (d == 1) val = palette_floor(p.val / 2, 18);

        set_led_hsv(i, p.hue + led_col_index(i) * 4, p.sat, val);
    }

    flush_led_frame();
}

static void render_effect_rainbow(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 2900);
    uint8_t offset = (uint8_t)((now * 255UL) / period);

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t val = pulse_val(now, effect_period_for_layer(layer, 1500), i * 19, palette_floor(p.val / 4, 20), p.val);
        set_led_hsv(i, offset + i * 24 + (led_is_gap(i) ? 12 : 0), p.sat, val);
    }

    flush_led_frame();
}

static void render_effect_stack(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 1600);
    uint8_t filled = ((now % period) * (RGBLIGHT_LED_COUNT + 1)) / period;
    bool reverse = ((now / period) % 2) != 0;

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t pos = reverse ? (RGBLIGHT_LED_COUNT - 1 - i) : i;
        uint8_t val = pos < filled ? p.val : palette_floor(p.val / 18, 4);

        if (pos == filled && filled < RGBLIGHT_LED_COUNT) val = palette_floor(p.val + 40, p.val);

        set_led_hsv(i, p.hue + pos * 2, p.sat, val);
    }

    flush_led_frame();
}

static void render_rgb_layer_visuals(void) {
    uint8_t layer = canonical_rgb_layer(active_layer_raw());
    rgb_effect_mode_t effect = effect_for_layer(layer);

    if (effect == RGB_EFFECT_OFF) {
        clear_all_keys();
        flush_led_frame();
        return;
    }

    switch (effect) {
        case RGB_EFFECT_BREATHING: render_effect_breathing(layer); return;
        case RGB_EFFECT_RUNNING:   render_effect_running(layer);   return;
        case RGB_EFFECT_TWINKLE:   render_effect_twinkle(layer);   return;
        case RGB_EFFECT_PULSE:     render_effect_pulse(layer);     return;
        case RGB_EFFECT_SOLID:     render_effect_solid(layer);     return;
        case RGB_EFFECT_COMET:     render_effect_comet(layer);     return;
        case RGB_EFFECT_SCAN:      render_effect_scan(layer);      return;
        case RGB_EFFECT_RAINBOW:   render_effect_rainbow(layer);   return;
        case RGB_EFFECT_STACK:     render_effect_stack(layer);     return;
        case RGB_EFFECT_WILD:
        default:
            break;
    }

    switch (layer) {
        case _BASE:   render_base_wild();   break;
        case _WINDOW: render_window_wild(); break;
        case _TEXT:   render_text_wild();   break;
        case _MEDIA:  render_media_wild();  break;
        case _RGB:    render_rgb_wild();    break;
        case _DEV:    render_dev_wild();    break;
        case _VSC:    render_vsc_wild();    break;
        case _PROMPT: render_prompt_wild(); break;
        case _SELECT: render_select_wild(); break;
        default:      render_base_wild();   break;
    }
}
#endif

// ── Keymaps ─────────────────────────────────────────────────
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(
        MO(_SELECT), KC_HOME,     KC_BSPC,
        KC_LEFT,     KC_ENT,      KC_RGHT,
        LCTL(KC_Y),  SEL_RGB,     LCTL(KC_Z)
    ),
    [_WINDOW] = LAYOUT(
        MO(_SELECT), WIN_BRO, WIN_AUX,
        WIN_1,       WIN_2,   WIN_3,
        WIN_4,       WIN_5,   WIN_6
    ),
    [_TEXT] = LAYOUT(
        MO(_SELECT), TXT_ACT, KC_ENT,
        TXT_1,       TXT_2,   TXT_3,
        TXT_4,       TXT_5,   TXT_6
    ),
    [_MEDIA] = LAYOUT(
        MO(_SELECT), KC_MPRV, KC_MNXT,
        KC_MRWD,     KC_MPLY, KC_MFFD,
        KC_VOLD,     KC_MUTE, KC_VOLU
    ),
    [_RGB] = LAYOUT(
        MO(_SELECT), RGB_MODE, RGB_TOG,
        RGB_HUEU,    RGB_HUED, RGB_VALU,
        RGB_SATU,    RGB_SATD, RGB_VALD
    ),
    [_RGBMOD] = LAYOUT(
        MO(_SELECT), KC_TRNS, KC_NO,
        KC_NO,       KC_NO,   KC_NO,
        KC_NO,       KC_NO,   KC_NO
    ),
    [_RGBADJ] = LAYOUT(
        MO(_SELECT), KC_NO,   KC_TRNS,
        KC_NO,       KC_NO,   KC_NO,
        KC_NO,       KC_NO,   KC_NO
    ),
    [_DEV] = LAYOUT(
        MO(_SELECT), GM_NAV,  GM_WASD,
        GM_1,        GM_2,    GM_3,
        GM_4,        GM_5,    GM_6
    ),
    [_VSC] = LAYOUT(
        MO(_SELECT), VSC_BAR, VSC_CHAT,
        VSC_1,       VSC_2,   VSC_3,
        VSC_4,       VSC_5,   VSC_6
    ),
    [_PROMPT] = LAYOUT(
        MO(_SELECT), PRM_PICS, PRM_ETSY,
        VSC_1,       VSC_2,   VSC_3,
        VSC_4,       VSC_5,   VSC_6
    ),
    [_SELECT] = LAYOUT(
        MO(_SELECT), SEL_WINDOW, SEL_TEXT,
        SEL_MEDIA,   KC_NO,      SEL_DEV,
        SEL_VSC,     SEL_RGB,    SEL_PROMPT
    ),
};

layer_state_t layer_state_set_user(layer_state_t state) {
    static layer_state_t last_state = 0;
    bool select_now = layer_state_cmp(state, _SELECT);
    bool select_before = layer_state_cmp(last_state, _SELECT);

    if (select_now && !select_before) {
        layer_state_t without_select = (state | default_layer_state) & ~((layer_state_t)1 << _SELECT);
        uint8_t base = canonical_rgb_layer(get_highest_layer(without_select));

        if (base >= _SELECT) base = _BASE;

        selector_target = base;
        select_cursor = slot_for_layer(selector_target);
    }

    last_state = state;
    return state;
}

static void select_target_layer(uint8_t layer) {
    selector_target = layer;
    select_cursor = slot_for_layer(selector_target);

    if (!matrix_select_held) {
        layer_move(selector_target);
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (keycode == MO(_SELECT)) {
        if (record->event.pressed) {
            selector_origin_layer = canonical_rgb_layer(active_layer_raw());
            if (selector_origin_layer >= _SELECT) selector_origin_layer = _BASE;

            matrix_select_held = true;
            update_select_layer_state();
        } else {
            matrix_select_held = false;
            update_select_layer_state();

            if (selector_target == selector_origin_layer && timer_elapsed32(selector_last_tap) <= SELECTOR_DOUBLE_TAP_MS) {
                selector_target = _BASE;
                select_cursor = slot_for_layer(selector_target);
                layer_move(_BASE);
                selector_last_tap = 0;
            } else {
                layer_move(selector_target);
                selector_last_tap = (selector_target == selector_origin_layer) ? timer_read32() : 0;
            }
        }

        return false;
    }

    if (record->event.pressed) {
        last_keycode = keycode;
        last_key_layer = active_layer_raw();
        last_row = record->event.key.row;
        last_col = record->event.key.col;
        last_key_text_mode = current_text_preview_mode();
        last_key_window_mode = current_window_preview_mode();
        last_key_rgb_mode = rgb_mode_held;
        last_key_game_mode = current_game_preview_mode();
        last_key_vsc_mode = current_vsc_preview_mode();
        last_key_prompt_mode = prompt_mode;
    }

    switch (keycode) {
        case SEL_BASE:
            if (record->event.pressed) select_target_layer(_BASE);
            return false;

        case RGB_MODE:
            rgb_mode_held = record->event.pressed;
            return false;

        case RGB_TOG:
            if (record->event.pressed) {
                if (rgb_mode_held) {
                    toggle_rgb_all_zones();
                } else {
                    toggle_rgb_output_state();
                }
            }
            return false;

        case RGB_HUEU:
            if (record->event.pressed) {
                if (rgb_mode_held) {
                    toggle_rgb_zone_bits(RGB_ZONE_FRAME);
                } else {
                    adjust_layer_hue(_RGB, RGBLIGHT_HUE_STEP);
                }
            }
            return false;

        case RGB_HUED:
            if (record->event.pressed) {
                if (rgb_mode_held) {
                    toggle_rgb_zone_bits(RGB_ZONE_KEY);
                } else {
                    adjust_layer_hue(_RGB, -RGBLIGHT_HUE_STEP);
                }
            }
            return false;

        case RGB_VALU:
            if (record->event.pressed) {
                if (rgb_mode_held) {
                    toggle_rgb_zone_bits(RGB_ZONE_GAP);
                } else {
                    adjust_layer_brightness(_RGB, RGBLIGHT_VAL_STEP);
                }
            }
            return false;

        case RGB_VALD:
            if (record->event.pressed && !rgb_mode_held) {
                adjust_layer_brightness(_RGB, -RGBLIGHT_VAL_STEP);
            }
            return false;

        case RGB_SATU:
            if (record->event.pressed && !rgb_mode_held) {
                adjust_layer_saturation(_RGB, RGBLIGHT_SAT_STEP);
            }
            return false;

        case RGB_SATD:
            if (record->event.pressed && !rgb_mode_held) {
                adjust_layer_saturation(_RGB, -RGBLIGHT_SAT_STEP);
            }
            return false;

        case SEL_WINDOW:
            if (record->event.pressed) select_target_layer(_WINDOW);
            return false;

        case SEL_TEXT:
            if (record->event.pressed) select_target_layer(_TEXT);
            return false;

        case SEL_MEDIA:
            if (record->event.pressed) select_target_layer(_MEDIA);
            return false;

        case SEL_RGB:
            if (record->event.pressed) select_target_layer(_RGB);
            return false;

        case SEL_DEV:
            if (record->event.pressed) select_target_layer(_DEV);
            return false;

        case SEL_VSC:
            if (record->event.pressed) select_target_layer(_VSC);
            return false;

        case SEL_PROMPT:
            if (record->event.pressed) {
                prompt_mode = PROMPT_MODE_BASE;
                select_target_layer(_PROMPT);
            }
            return false;

        case PRM_PICS:
            if (record->event.pressed) {
                prompt_mode = PROMPT_MODE_PICS;
                selector_target = _PROMPT;
                select_cursor = slot_for_layer(selector_target);
                layer_move(_PROMPT);
            }
            return false;

        case PRM_ETSY:
            if (record->event.pressed) {
                prompt_mode = PROMPT_MODE_ETSY;
                selector_target = _PROMPT;
                select_cursor = slot_for_layer(selector_target);
                layer_move(_PROMPT);
            }
            return false;

        case GM_NAV:
            if (record->event.pressed) game_mode = GAME_MODE_NAV;
            return false;

        case GM_WASD:
            if (record->event.pressed) game_mode = GAME_MODE_WASD;
            return false;

        case GM_1:
        case GM_2:
        case GM_3:
        case GM_4:
        case GM_5:
        case GM_6:
            if (record->event.pressed) tap_game_target((uint8_t)(keycode - GM_1));
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
            if (record->event.pressed) tap_window_target((uint8_t)(keycode - WIN_1));
            return false;

        case TXT_ACT:
            text_edit_held = false;
            text_action_held = record->event.pressed;
            return false;

        case TXT_EDT:
            text_action_held = false;
            text_edit_held = record->event.pressed;
            return false;

        case TXT_1:
        case TXT_2:
        case TXT_3:
        case TXT_4:
        case TXT_5:
        case TXT_6:
            if (record->event.pressed) tap_text_target((uint8_t)(keycode - TXT_1));
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
            if (record->event.pressed) trigger_vsc_target((uint8_t)(keycode - VSC_1));
            return false;

    }

    return true;
}

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
                encoder_btn_rotated = true;
                text_selection_pending_copy = true;
                tap_code16(clockwise ? S(KC_RGHT) : S(KC_LEFT));
            } else {
                text_selection_pending_copy = false;
                tap_code(clockwise ? KC_RGHT : KC_LEFT);
            }
            break;

        case _MEDIA:
            tap_code(clockwise ? KC_VOLU : KC_VOLD);
            break;

        case _RGB:
        case _RGBMOD:
        case _RGBADJ:
            adjust_layer_brightness(_RGB, clockwise ? RGBLIGHT_VAL_STEP : -RGBLIGHT_VAL_STEP);
            break;

        case _DEV:
            tap_code(clockwise ? MS_WHLU : MS_WHLD);
            break;

        case _VSC:
        case _PROMPT:
            tap_code16(clockwise ? C(KC_PGDN) : C(KC_PGUP));
            break;

        case _SELECT:
            if (encoder_btn_pressed) {
                select_cursor = next_select_slot(select_cursor, clockwise);
                sync_selector_target_from_cursor();
            }
            break;

        default:
            tap_code(clockwise ? KC_VOLU : KC_VOLD);
            break;
    }

    return false;
}

void matrix_scan_user(void) {
#ifdef OLED_TOGGLE_BTN_PIN
    static bool oled_toggle_was_pressed = false;
    static bool oled_toggle_combo_used = false;
    static uint32_t oled_toggle_last_action = 0;
#endif

#ifdef ENCODER_BTN_PIN
    encoder_btn_pressed = (gpio_read_pin(ENCODER_BTN_PIN) == 0);

    if (encoder_btn_pressed && !encoder_btn_was_pressed) {
        encoder_btn_rotated = false;

        if (active_layer_raw() == _TEXT && text_selection_pending_copy) {
            tap_code16(C(KC_C));
            text_selection_pending_copy = false;

            // Verhindert, dass dieser Kopierdruck noch Encoder-Help auslöst.
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
    bool oled_toggle_pressed = (gpio_read_pin(OLED_TOGGLE_BTN_PIN) == 0);

    bool clear_buttons_pressed = encoder_btn_pressed && oled_toggle_pressed;

    if (clear_buttons_pressed) {
        oled_toggle_combo_used = true;

        if (button_clear_started == 0) {
            button_clear_started = timer_read32() | 1;
        } else if (!button_clear_armed && timer_elapsed32(button_clear_started) >= CLEAR_EEPROM_HOLD_MS) {
            button_clear_armed = true;

            eeconfig_init();

#    ifdef VIA_ENABLE
            set_via_config_defaults();
            save_via_config();
#    endif

            soft_reset_keyboard();
        }
    } else {
        button_clear_started = 0;
        button_clear_armed = false;
    }

    if (!oled_toggle_pressed && oled_toggle_was_pressed) {
        if (!oled_toggle_combo_used && timer_elapsed32(oled_toggle_last_action) > BUTTON_DEBOUNCE_MS) {
            oled_view = (oled_view == OLED_VIEW_LEGEND) ? OLED_VIEW_LAST_KEY : OLED_VIEW_LEGEND;

#    ifdef VIA_ENABLE
            via_user_config.oled_view = oled_view;
            save_via_config();
#    endif

            oled_toggle_last_action = timer_read32();
        }

        oled_toggle_combo_used = false;
    }

    oled_toggle_was_pressed = oled_toggle_pressed;
#endif

#ifdef RGBLIGHT_ENABLE
    if (timer_elapsed32(rgb_frame_timer) >= RGB_FRAME_MS) {
        rgb_frame_timer = timer_read32();
        render_rgb_layer_visuals();
    }
#endif
}

void keyboard_post_init_user(void) {
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
}

#ifdef OLED_ENABLE
oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    return rotation;
}
#endif

#ifdef OLED_DISPLAY_128X32
static void write_line(uint8_t row, const char *str) {
    char buf[22];
    snprintf(buf, sizeof(buf), "%-21.21s", str);
    oled_set_cursor(0, row);
    oled_write(buf, false);
}

static void render_boot(void) {
    if (boot_start == 0) boot_start = timer_read32() | 1;

    uint32_t t = timer_elapsed32(boot_start);

    write_line(0, "");
    write_line(1, t >= 700 ? "       I AM" : "");
    write_line(2, t >= 1200 ? "      ROOT" : "");
    write_line(3, "");
}

static void render_header(uint8_t layer) {
    char line[22];

    if (layer == _SELECT) {
        snprintf(line, sizeof(line), "SEL->%-10.10s", layer_name_short(selector_target));
    } else if (layer == _PROMPT) {
        snprintf(line, sizeof(line), "PRM %-4s", prompt_mode_name(prompt_mode));
    } else if (layer == _DEV) {
        snprintf(line, sizeof(line), "GME %-4s", game_mode_name(game_mode));
    } else if (layer == _WINDOW && window_browser_held) {
        snprintf(line, sizeof(line), "%-10.10s", "WIN BRO");
    } else if (layer == _TEXT && text_action_held) {
        snprintf(line, sizeof(line), "%-10.10s", "TXT ACT");
    } else if (layer == _TEXT && text_edit_held) {
        snprintf(line, sizeof(line), "%-10.10s", "TXT EDT");
    } else if (layer == _VSC) {
        snprintf(line, sizeof(line), "VSC %-4s", current_vsc_preview_mode() == VSC_MODE_CHAT ? "CHAT" : "BAR");
    } else {
        snprintf(line, sizeof(line), "%-10.10s", layer_name_short(layer));
    }

    write_line(0, line);
}

static void render_legend_view(uint8_t layer) {
    char line[22];

    render_header(layer);

    snprintf(line, sizeof(line), "%-6.6s %-6.6s %-6.6s", legend_label_for(layer, 0, 0), legend_label_for(layer, 0, 1), legend_label_for(layer, 0, 2));
    write_line(1, line);

    snprintf(line, sizeof(line), "%-6.6s %-6.6s %-6.6s", legend_label_for(layer, 1, 0), legend_label_for(layer, 1, 1), legend_label_for(layer, 1, 2));
    write_line(2, line);

    snprintf(line, sizeof(line), "%-6.6s %-6.6s %-6.6s", legend_label_for(layer, 2, 0), legend_label_for(layer, 2, 1), legend_label_for(layer, 2, 2));
    write_line(3, line);
}

static void render_last_key_view(void) {
    char buf[22];

    snprintf(buf, sizeof(buf), "LAST %-6.6s", layer_name_short(last_key_layer));
    write_line(0, buf);

    snprintf(buf, sizeof(buf), "K:%-7.7s %04X", last_key_label_for(), last_keycode);
    write_line(1, buf);

    snprintf(buf, sizeof(buf), "F:%.18s", last_key_function_for());
    write_line(2, buf);

    snprintf(buf, sizeof(buf), "P:%u (%u,%u)", (last_row * 3) + last_col + 1, last_row, last_col);
    write_line(3, buf);
}

static void render_encoder_view(void) {
    char buf[22];
    uint8_t layer = active_layer_raw();

    render_header(layer);
    write_line(1, "ENCODER MODE");

    snprintf(buf, sizeof(buf), "L:%-6.6s", layer_name_short(layer));
    write_line(2, buf);

    snprintf(buf, sizeof(buf), "%.21s", encoder_function_for_layer(layer));
    write_line(3, buf);
}

bool oled_task_user(void) {
    if (boot_start == 0 || timer_elapsed32(boot_start) < BOOT_TOTAL_MS) {
        render_boot();
        return false;
    }

    if (oled_view == OLED_VIEW_ENCODER) {
        if (timer_read32() < encoder_help_until) {
            render_encoder_view();
            return false;
        } else {
            oled_view = OLED_VIEW_LEGEND;
        }
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

#else

static void write_line(uint8_t row, const char *str) {
    char buf[22];
    snprintf(buf, sizeof(buf), "%-21.21s", str);
    oled_set_cursor(0, row);
    oled_write(buf, false);
}

static void render_boot(void) {
    if (boot_start == 0) boot_start = timer_read32() | 1;

    uint32_t t = timer_elapsed32(boot_start);

    write_line(0, "");
    write_line(1, "");
    write_line(2, t >= 800 ? "          I AM" : "             I");
    write_line(3, t >= 1200 ? "          ROOT" : "");
    write_line(4, "");
    write_line(5, "");
    write_line(6, "");
    write_line(7, "");
}

static void render_header(uint8_t layer) {
    char line[22];

    if (layer == _PROMPT) {
        snprintf(line, sizeof(line), "PRM %-4s", prompt_mode_name(prompt_mode));
    } else {
        snprintf(line, sizeof(line), "%-10.10s", layer_name_short(layer));
    }

    write_line(0, line);
}

static void render_legend_view(uint8_t layer) {
    char line[22];

    render_header(layer);
    write_line(1, "1      2      3");

    snprintf(line, sizeof(line), "%-6.6s %-6.6s %-6.6s", legend_label_for(layer, 0, 0), legend_label_for(layer, 0, 1), legend_label_for(layer, 0, 2));
    write_line(2, line);

    write_line(3, "4      5      6");

    snprintf(line, sizeof(line), "%-6.6s %-6.6s %-6.6s", legend_label_for(layer, 1, 0), legend_label_for(layer, 1, 1), legend_label_for(layer, 1, 2));
    write_line(4, line);

    write_line(5, "7      8      9");

    snprintf(line, sizeof(line), "%-6.6s %-6.6s %-6.6s", legend_label_for(layer, 2, 0), legend_label_for(layer, 2, 1), legend_label_for(layer, 2, 2));
    write_line(6, line);

    if (layer == _SELECT) {
        snprintf(line, sizeof(line), "Cur%u Tgt->%.10s", select_cursor + 1, layer_name_long(selector_target));
    } else if (layer == _VSC) {
        snprintf(line, sizeof(line), "%s mode%s", current_vsc_preview_mode() == VSC_MODE_CHAT ? "CHAT" : "BAR", vsc_mode != VSC_MODE_NONE ? " [HELD]" : "");
    } else if (layer == _TEXT) {
        const char *mode = text_action_held ? "ACT" : (text_edit_held ? "EDT" : "WIN");
        snprintf(line, sizeof(line), "TXT %s%s", mode, (text_action_held || text_edit_held) ? " [HELD]" : "");
    } else if (layer == _WINDOW) {
        snprintf(line, sizeof(line), "WIN %s%s", window_browser_held ? "BRO" : "WIN", window_browser_held ? " [HELD]" : "");
    } else {
#ifdef OLED_TOGGLE_BTN_PIN
        snprintf(line, sizeof(line), "GP11: Lay <-> Last");
#else
        snprintf(line, sizeof(line), "Hold SEL for grid");
#endif
    }

    write_line(7, line);
}

static void render_last_key_view(void) {
    char buf[22];

    render_header(last_key_layer);
    write_line(1, "LAST KEY");

    snprintf(buf, sizeof(buf), "Layer: %s", layer_name_long(last_key_layer));
    write_line(2, buf);

    snprintf(buf, sizeof(buf), "Key:   %.14s", last_key_label_for());
    write_line(3, buf);

    snprintf(buf, sizeof(buf), "Code:  0x%04X", last_keycode);
    write_line(4, buf);

    snprintf(buf, sizeof(buf), "Func:  %.14s", last_key_function_for());
    write_line(5, buf);

    snprintf(buf, sizeof(buf), "Pos:   %u (%u,%u)", (last_row * 3) + last_col + 1, last_row, last_col);
    write_line(6, buf);

#ifdef OLED_TOGGLE_BTN_PIN
    write_line(7, "GP11: back to Lay");
#else
    write_line(7, "Last-key details");
#endif
}

static void render_encoder_view(void) {
    char buf[22];
    uint8_t layer = active_layer_raw();

    render_header(layer);
    write_line(1, "ENCODER MODE");

    snprintf(buf, sizeof(buf), "Layer: %s", layer_name_long(layer));
    write_line(2, buf);

    write_line(3, "Function:");

    snprintf(buf, sizeof(buf), "%.21s", encoder_function_for_layer(layer));
    write_line(4, buf);

    write_line(6, "Hold encoder btn");
    write_line(7, "for encoder help");
}

bool oled_task_user(void) {
    if (boot_start == 0 || timer_elapsed32(boot_start) < BOOT_TOTAL_MS) {
        render_boot();
        return false;
    }

    if (oled_view == OLED_VIEW_ENCODER) {
        if (timer_read32() < encoder_help_until) {
            render_encoder_view();
            return false;
        } else {
            oled_view = OLED_VIEW_LEGEND;
        }
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
