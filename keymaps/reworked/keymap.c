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
#define RGBLIGHT_LED_COUNT 9
#endif
#ifndef SELECTOR_BTN_PIN
#    define SELECTOR_BTN_PIN GP12
#endif
#define PAD_KEY_COUNT        9
#define RGB_FRAME_MS         33
#define BOOT_TOTAL_MS        2800
#define BUTTON_DEBOUNCE_MS   150
#define SELECTOR_DOUBLE_TAP_MS 300
#define CLEAR_EEPROM_HOLD_MS 3000
#define VIA_LAYER_SLOT_COUNT 8
#define REWORKED_LAYOUT_VERSION 4
#define ENCODER_HELP_HOLD_MS 700
#define ENCODER_HELP_SHOW_MS 2500

// ── Layer enum ──────────────────────────────────────────────
enum layers {
    _BASE,
    _WINDOW,
    _TEXT,
    _MEDIA,
    _DEV,
    _VSC,
    _RGB,
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
    RGB_PROFILE
};

static bool rgb_minimal_mode = false;

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
static text_mode_t text_mode         = TEXT_MODE_WIN;
static bool matrix_select_held       = false;
static bool encoder_btn_pressed      = false;
static bool encoder_btn_was_pressed  = false;
static bool encoder_btn_rotated      = false;
static bool button_clear_armed       = false;
static uint32_t button_clear_started = 0;
static bool text_selection_pending_copy = false;
static bool text_action_held         = false;
static bool text_edit_held           = false;
static text_mode_t last_key_text_mode = TEXT_MODE_WIN;
static bool window_browser_held       = false;
static window_mode_t last_key_window_mode = WINDOW_MODE_WIN;
static game_mode_t game_mode          = GAME_MODE_WASD;
static game_mode_t last_key_game_mode = GAME_MODE_WASD;
static vsc_mode_t last_key_vsc_mode  = VSC_MODE_BAR;
static prompt_mode_t prompt_mode      = PROMPT_MODE_BASE;
static prompt_mode_t last_key_prompt_mode = PROMPT_MODE_BASE;
static uint32_t encoder_help_started = 0;
static uint32_t encoder_help_until   = 0;
static bool encoder_help_fired       = false;
static uint8_t selector_origin_layer = _BASE;
static uint32_t selector_last_tap    = 0;

// Physical key numbering:
//  1 2 3
//  4 5 6
//  7 8 9
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

typedef struct {
    uint8_t hue;
    uint8_t sat;
    uint8_t val;
} hsv_config_t;

#ifdef VIA_ENABLE
enum via_custom_value {
    id_via_oled_view           = 1,
    id_via_fx_mode             = 2,
    id_via_layer_color         = 3,
    id_via_layer_brightness    = 4,
    id_via_rgb_effect          = 5,
    id_via_layer_effect_speed  = 6,
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
    { _BASE,   160, 220, 120, "BASE",   true  },
    { _WINDOW, 176, 240, 120, "WINDOW", true  },
    { _TEXT,    96, 220, 110, "TXT",    true  },
    { _DEV,   32, 255, 130, "GAME", true  },
    { _SELECT,   0,   0, 120, "SELECT", false },
    { _MEDIA,     18, 255, 130, "MEDIA",    true  },
    { _VSC,    200, 255, 130, "VSC",    true  },
    { _RGB,    215, 240, 130, "RGB",    true  },
    { _PROMPT,   8, 255, 140, "PROMT",  true  },
};

static const uint8_t via_layer_slots[VIA_LAYER_SLOT_COUNT] = {
    0, 1, 2, 5, 3, 6, 7, 8
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
"create an Arduino sketch with pin output. it should show coordinate in the matrix, function, if available and recognized gppin. Make two versions, one as serial sketch and one as real keyboard. incule rgbs, oled and encoder if given.",
"Check my QMK environment in VS code, but don't change files. Execute diagnostic commands such as 'qmk doctor', 'qmk config', and userspace 'qmk userspace-doctor'. Analyze OS, shell, dependencies, compilers, paths, VS code terminal, tasks, and QMK project structure. Create a prioritized error list with specific fix steps."
"Repair my QMK development environment in VS code. Use the output of 'qmk doctor', build errors, and VS code configurations. Only fix secure issues such as missing paths, incorrect terminal profiles, erroneous tasks, incorrect QMK configuration, or obvious dependency issues. Then validate with 'qmk doctor' and a test build.",
"Check notes.md and start working on it.",

};

static const char *const prompt_base_labels[6] = {"SUM", "REVW", "FIX", "TEST", "EXPL", "COMMIT"};
static const char *const prompt_base_functions[6] = {"Prompt summarize", "Prompt review", "Prompt suggest fix", "Prompt write tests", "Prompt explain code", "Prompt commit message"};

static const char *const prompt_pics_labels[6] = {"TAB", "SHOT", "ALT", "SEO", "MOCK", "CHK"};
static const char *const prompt_pics_functions[6] = {"Picture tab", "Shot list", "Alt text", "SEO image text", "Mockup ideas", "Image QA checklist"};
static const char *const prompt_pics_macros[6] = {
    "Create a picture-tab plan for this Etsy listing. Propose the ideal image order for the tabs, what each image should show, and the customer goal of each tab.",
    "Create a concise Etsy product photography shot list for this item. Include hero image, scale shot, detail shots, lifestyle shots, packaging, and any trust-building images.",
    "Write Etsy-ready alt text for product listing images. Keep each line descriptive, concrete, and accessible, with no keyword stuffing.",
    "Write image overlay text and SEO-friendly captions for Etsy listing pictures. Keep them short, readable, and conversion-focused.",
    "Suggest mockup and staging ideas for Etsy listing photos. Focus on believable scenes, useful props, scale clarity, and conversion impact.",
    "Create an image QA checklist for this Etsy listing. Check clarity, cropping, lighting, consistency, branding, scale communication, and policy-safe content."
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
    [_RGB]    = {"SEL",  "SPD-", "TOG",  "HUE+", "HUE-", "VAL+", "SAT+", "SAT-", "VAL-"},
    [_DEV]    = {"SEL",  "NAV",  "WASD", "ESC",  "UP",   "ENT",  "LEFT", "DOWN", "RGHT"},
    [_VSC]    = {"SEL",  "BAR",  "CHAT", "EXPL", "SRC",  "GH-A", "GHUB", "GPT",  "FREE"},
    [_PROMPT] = {"SEL",  "PICS", "ETSY", "SUM",  "REVW", "FIX",  "TEST", "EXPL", "COMMIT"},
    [_SELECT] = {"BASE", "WIN",  "TXT",  "MED",  "FX",   "GAME", "VSC",  "RGB",  "PROMT"},
};

static const char *const layer_function[_LAYER_COUNT][PAD_KEY_COUNT] = {
    [_BASE]   = {"Select layer", "Arrow up", "Backspace", "Arrow left", "Enter", "Arrow right", "Undo", "Arrow down", "Redo"},
    [_WINDOW] = {"Select layer", "Browser combo", "Reserved", "Prev desktop", "Task view", "Next desktop", "Prev window", "Show desktop", "Next window"},
    [_TEXT]   = {"Select layer", "Hold text actions", "Enter", "Line start", "Cursor up", "Line end", "Cursor left", "Cursor down", "Cursor right"},
    [_MEDIA]  = {"Select layer", "Previous track", "Next track", "Rewind", "Play/Pause", "Fast forward", "Volume down", "Mute", "Volume up"},
    [_RGB]    = {"Select layer", "Speed down", "Toggle RGB", "Hue up", "Hue down", "Brightness up", "Saturation up", "Saturation down", "Brightness down"},
    [_DEV]    = {"Select layer", "Switch to menu navigation", "Switch to movement controls", "Back out of menu", "Menu up", "Confirm or interact", "Menu left", "Menu down", "Menu right"},
    [_VSC]    = {"Select layer", "BAR mode", "CHAT mode", "Combo target 1", "Combo target 2", "Combo target 3", "Combo target 4", "Combo target 5", "Combo target 6"},
    [_PROMPT] = {"Select layer", "Prompt picture tools", "Prompt Etsy tools", "Prompt summarize", "Prompt review", "Prompt suggest fix", "Prompt write tests", "Prompt explain code", "Prompt commit message"},
    [_SELECT] = {"Go to base", "Go to window", "Go to text", "Go to media", "Toggle FX mode", "Go to game", "Go to VSC", "Go to RGB", "Go to prompt"},
};

static const char *layer_name_short(uint8_t l) {
    switch (l) {
        case _BASE:   return "BASE";
        case _WINDOW: return "WIN";
        case _TEXT:   return "TXT";
        case _MEDIA:  return "MED";
        case _RGB:    return "RGB";
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
        case _DEV:    return "GAME";
        case _VSC:    return "VSC";
        case _PROMPT: return "PROMPT";
        case _SELECT: return "SELECT";
        default:      return "BASE";
    }
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

static uint16_t effect_period_for_layer(uint8_t layer, uint16_t base_period) {
    uint8_t speed = effect_speed_for_layer(layer);
    if (speed < 1) speed = 1;
    uint32_t period = ((uint32_t)base_period * 128UL) / speed;
    if (period < 40) period = 40;
    if (period > 20000) period = 20000;
    return (uint16_t)period;
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
    rgb_minimal_mode = via_user_config.fx_mode != 0;
    for (uint8_t i = 0; i < VIA_LAYER_SLOT_COUNT; i++) apply_palette_entry(i);
}

static void set_via_config_defaults(void) {
    via_user_config.signature  = 0x94;
    via_user_config.layout_version = REWORKED_LAYOUT_VERSION;
    via_user_config.oled_view  = OLED_VIEW_LEGEND;
    via_user_config.fx_mode    = 0;
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

    if (via_user_config.signature != 0x94) {
        invalid_config = true;
    }

    if (via_user_config.layout_version != REWORKED_LAYOUT_VERSION) {
        invalid_config = true;
    }

    if (via_user_config.oled_view > OLED_VIEW_LAST_KEY) {
        invalid_config = true;
    }

    if (via_user_config.fx_mode > 1) {
        invalid_config = true;
    }

    for (uint8_t i = 0; i < VIA_LAYER_SLOT_COUNT; i++) {
        if (via_user_config.layer_effect[i] >= RGB_EFFECT_COUNT) {
            invalid_config = true;
        }
        if (via_user_config.layer_speed[i] < 1) {
            invalid_config = true;
        }
    }

    if (invalid_config) {
        // The layout changed; reset EEPROM-backed key assignments so new defaults apply.
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
            rgb_minimal_mode = via_user_config.fx_mode != 0;
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

#ifdef RGBLIGHT_ENABLE
static void set_key_hsv(uint8_t key_index, uint8_t h, uint8_t s, uint8_t v) {
    if (key_index >= PAD_KEY_COUNT) return;
    uint8_t led = key_led_map[key_index];
    if (led >= RGBLIGHT_LED_COUNT) return;
    rgblight_sethsv_at(h, s, v, led);
}

static void clear_all_keys(void) {
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) set_key_hsv(i, 0, 0, 0);
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
    hsv_config_t base = palette_for_layer(_BASE);
    uint8_t hue_shift = (uint8_t)(now / 64);
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t hue = hue_shift + (i * 6);
        uint8_t val = pulse_val(now, 2600, i * 11, palette_floor(base.val / 4, 18), base.val);
        set_key_hsv(i, hue, base.sat, val);
    }
}

static void render_window_wild(void) {
    uint32_t now = timer_read32();
    uint8_t spike = (now / 110) % PAD_KEY_COUNT;
    hsv_config_t window = palette_for_layer(_WINDOW);
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t dist = (i > spike) ? (i - spike) : (spike - i);
        uint8_t hue = window.hue;
        uint8_t sat = window.sat;
        uint8_t val = pulse_val(now, 1800, i * 19, palette_floor(window.val / 5, 10), palette_floor(window.val / 2, 40));
        if (dist == 0) {
            hue = window.hue - 8;
            val = palette_floor(window.val + 30, window.val);
        } else if (dist == 1) {
            hue = window.hue + 6;
            val = palette_floor(window.val - 20, palette_floor(window.val / 2, 40));
        }
        set_key_hsv(i, hue, sat, val);
    }
}

static void render_text_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t text = palette_for_layer(_TEXT);
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t drift = triwave8_period(now, 7600, i * 13);
        uint8_t hue = text.hue + (drift / 12);
        uint8_t val = pulse_val(now, 3600, i * 9, palette_floor(text.val / 5, 10), text.val);
        set_key_hsv(i, hue, text.sat, val);
    }
}

static void render_media_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t media = palette_for_layer(_MEDIA);
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t swing = triwave8_period(now, 3000, i * 18);
        uint8_t hue = media.hue + (swing / 14);
        uint8_t val = pulse_val(now, 2000, i * 14, palette_floor(media.val / 5, 14), media.val);
        set_key_hsv(i, hue, media.sat, val);
    }
}

static void render_rgb_wild(void) {
    uint32_t now = timer_read32();
    bool beatflash = ((now % 1100) < 120);
    hsv_config_t rgb = palette_for_layer(_RGB);
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t hue = (uint8_t)(rgb.hue + (now / 18) + i * 28);
        uint8_t val = pulse_val(now, 1400, i * 15, palette_floor(rgb.val / 4, 34), beatflash ? palette_floor(rgb.val + 40, rgb.val) : rgb.val);
        set_key_hsv(i, hue, rgb.sat, val);
    }
}

static void render_dev_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t dev = palette_for_layer(_DEV);
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t swing = triwave8_period(now, 2200, i * 23);
        uint8_t hue = dev.hue + (swing / 9);
        uint8_t val = pulse_val(now, 900 + (i * 40), i * 17, palette_floor(dev.val / 5, 18), palette_floor(dev.val + 20, dev.val));
        set_key_hsv(i, hue, dev.sat, val);
    }
}

static void render_vsc_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t vsc = palette_for_layer(_VSC);
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t swing = triwave8_period(now, 1800, i * 21);
        uint8_t hue = vsc.hue + (swing / 6);
        uint8_t val = pulse_val(now, 1100 + (i * 30), i * 13, palette_floor(vsc.val / 5, 18), palette_floor(vsc.val + 15, vsc.val));
        set_key_hsv(i, hue, vsc.sat, val);
    }
}

static void render_prompt_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t prompt = palette_for_layer(_PROMPT);
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t glow = triwave8_period(now, 2100, i * 19);
        uint8_t hue = prompt.hue + (glow / 5);
        uint8_t val = pulse_val(now, 1500 + (i * 35), i * 17, palette_floor(prompt.val / 5, 22), prompt.val);
        set_key_hsv(i, hue, prompt.sat, val);
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
        if (i == 4) { sat = 0; val = 24; }
        if (i == target_slot && i != select_cursor && slot->selectable && val < 72) val = 72;
        if (i == select_cursor) {
            if (slot->selectable) val = pulse_val(now, 900, 0, 80, 170);
            else { sat = 0; val = pulse_val(now, 900, 0, 46, 120); }
        }
        set_key_hsv(i, hue, sat, val);
    }
}

static void render_effect_solid(uint8_t layer) {
    hsv_config_t p = palette_for_layer(layer);
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) { set_key_hsv(i, p.hue, p.sat, p.val); }
}

static void render_effect_breathing(uint8_t layer) {
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 2200);
    uint8_t val = pulse_val(timer_read32(), period, 0, palette_floor(p.val / 8, 8), p.val);
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) { set_key_hsv(i, p.hue, p.sat, val); }
}

static void render_effect_running(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 990);
    uint8_t head = (now / (period / PAD_KEY_COUNT + 1)) % PAD_KEY_COUNT;
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t dist = (i + PAD_KEY_COUNT - head) % PAD_KEY_COUNT;
        uint8_t val = palette_floor(p.val / 8, 8);
        if (dist == 0) val = p.val;
        else if (dist == 1 || dist == 8) val = palette_floor(p.val / 2, 30);
        else if (dist == 2 || dist == 7) val = palette_floor(p.val / 4, 16);
        set_key_hsv(i, p.hue, p.sat, val);
    }
}

static void render_effect_twinkle(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 1233);
    uint8_t sparkle = ((now / (period / 9 + 1)) * 5 + 1) % PAD_KEY_COUNT;
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t base = palette_floor(p.val / 10, 6);
        uint8_t shimmer = triwave8_period(now, effect_period_for_layer(layer, 900) + i * 73, i * 29) / 8;
        uint8_t val = palette_floor(base + shimmer, base);
        if (i == sparkle) val = p.val;
        set_key_hsv(i, p.hue + i * 2, p.sat, val);
    }
}

static void render_effect_pulse(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 1400);
    bool flash = (now % period) < (period / 10 + 10);
    uint8_t val = flash ? p.val : palette_floor(p.val / 5, 12);
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) { set_key_hsv(i, p.hue, p.sat, val); }
}

static void render_effect_comet(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 1100);
    uint8_t head = (now / (period / PAD_KEY_COUNT + 1)) % PAD_KEY_COUNT;
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t dist = (head + PAD_KEY_COUNT - i) % PAD_KEY_COUNT;
        uint8_t val = palette_floor(p.val / 12, 6);
        if (dist == 0) val = p.val;
        else if (dist == 1) val = palette_floor((p.val * 3) / 4, 24);
        else if (dist == 2) val = palette_floor(p.val / 2, 18);
        else if (dist == 3) val = palette_floor(p.val / 4, 12);
        set_key_hsv(i, p.hue + dist * 3, p.sat, val);
    }
}

static void render_effect_scan(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 1800);
    uint8_t pos = triwave8_period(now, period, 0);
    uint8_t head = ((uint16_t)pos * (PAD_KEY_COUNT - 1)) / 255;
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t dist = (i > head) ? (i - head) : (head - i);
        uint8_t val = palette_floor(p.val / 12, 8);
        if (dist == 0) val = p.val;
        else if (dist == 1) val = palette_floor(p.val / 3, 18);
        set_key_hsv(i, p.hue, p.sat, val);
    }
}

static void render_effect_rainbow(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 3200);
    uint8_t offset = (uint8_t)((now * 255UL) / period);
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t val = pulse_val(now, effect_period_for_layer(layer, 1800), i * 18, palette_floor(p.val / 4, 20), p.val);
        set_key_hsv(i, offset + i * 28, p.sat, val);
    }
}

static void render_effect_stack(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 2200);
    uint8_t filled = ((now % period) * (PAD_KEY_COUNT + 1)) / period;
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        uint8_t val = (i < filled) ? p.val : palette_floor(p.val / 12, 6);
        set_key_hsv(i, p.hue + i * 2, p.sat, val);
    }
}

static void render_rgb_layer_visuals(void) {
    uint8_t layer = active_layer_raw();
    rgb_effect_mode_t effect = effect_for_layer(layer);

    if (effect == RGB_EFFECT_OFF) { clear_all_keys(); return; }

    if (rgb_minimal_mode) {
        render_minimal_profile(layer);
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
        default: break;
    }

    switch (layer) {
        case _BASE:   render_base_wild(); break;
        case _WINDOW: render_window_wild(); break;
        case _TEXT:   render_text_wild(); break;
        case _MEDIA:  render_media_wild(); break;
        case _RGB:    render_rgb_wild(); break;
        case _DEV:    render_dev_wild(); break;
        case _VSC:    render_vsc_wild(); break;
        case _PROMPT: render_prompt_wild(); break;
        case _SELECT: render_select_wild(); break;
        default:      render_base_wild(); break;
    }
}
#endif

// ── Keymaps ─────────────────────────────────────────────────
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [_BASE] = LAYOUT(
        MO(_SELECT), KC_HOME,     KC_BSPC,
        KC_LEFT,     KC_ENT,    KC_RGHT,
        LCTL(KC_Y),  KC_END,   LCTL(KC_Z)
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
        MO(_SELECT), UG_SPDD, UG_TOGG,
        UG_HUEU,     UG_HUED, UG_VALU,
        UG_SATU,     UG_SATD, UG_VALD
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
        SEL_BASE,  SEL_WINDOW, SEL_TEXT,
        SEL_MEDIA, RGB_PROFILE,SEL_DEV,
        SEL_VSC,   SEL_RGB,    SEL_PROMPT
    ),
};

layer_state_t layer_state_set_user(layer_state_t state) {
    static layer_state_t last_state = 0;
    bool select_now = layer_state_cmp(state, _SELECT);
    bool select_before = layer_state_cmp(last_state, _SELECT);

    if (select_now && !select_before) {
        layer_state_t without_select = (state | default_layer_state) & ~((layer_state_t)1 << _SELECT);
        uint8_t base = get_highest_layer(without_select);
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
    if (!matrix_select_held) layer_move(selector_target);
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (keycode == MO(_SELECT)) {
        if (record->event.pressed) {
            selector_origin_layer = active_layer_raw();
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
        last_key_game_mode = current_game_preview_mode();
        last_key_vsc_mode = current_vsc_preview_mode();
        last_key_prompt_mode = prompt_mode;
    }

    switch (keycode) {
        case SEL_BASE:   if (record->event.pressed) select_target_layer(_BASE); return false;
        case SEL_WINDOW: if (record->event.pressed) select_target_layer(_WINDOW); return false;
        case SEL_TEXT:   if (record->event.pressed) select_target_layer(_TEXT); return false;
        case SEL_MEDIA:  if (record->event.pressed) select_target_layer(_MEDIA); return false;
        case SEL_RGB:    if (record->event.pressed) select_target_layer(_RGB); return false;
        case SEL_DEV:    if (record->event.pressed) select_target_layer(_DEV); return false;
        case SEL_VSC:    if (record->event.pressed) select_target_layer(_VSC); return false;
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
        case GM_1: case GM_2: case GM_3: case GM_4: case GM_5: case GM_6:
            if (record->event.pressed) tap_game_target((uint8_t)(keycode - GM_1));
            return false;

        case WIN_BRO: window_browser_held = record->event.pressed; return false;
        case WIN_AUX: return false;
        case WIN_1: case WIN_2: case WIN_3: case WIN_4: case WIN_5: case WIN_6:
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
        case TXT_1: case TXT_2: case TXT_3: case TXT_4: case TXT_5: case TXT_6:
            if (record->event.pressed) tap_text_target((uint8_t)(keycode - TXT_1));
            return false;

        case VSC_BAR:
            if (record->event.pressed) { vsc_mode = VSC_MODE_BAR; last_vsc_mode = VSC_MODE_BAR; }
            else vsc_mode = VSC_MODE_NONE;
            return false;
        case VSC_CHAT:
            if (record->event.pressed) { vsc_mode = VSC_MODE_CHAT; last_vsc_mode = VSC_MODE_CHAT; }
            else vsc_mode = VSC_MODE_NONE;
            return false;
        case VSC_1: case VSC_2: case VSC_3: case VSC_4: case VSC_5: case VSC_6:
            if (record->event.pressed) trigger_vsc_target((uint8_t)(keycode - VSC_1));
            return false;

        case RGB_PROFILE:
            if (record->event.pressed) rgb_minimal_mode = !rgb_minimal_mode;
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
            if (current_window_preview_mode() == WINDOW_MODE_BROWSER) tap_code16(clockwise ? C(KC_PGDN) : C(KC_PGUP));
            else if (clockwise) tap_code16(A(KC_TAB));
            else tap_code16(S(A(KC_TAB)));
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
            tap_code16(clockwise ? UG_VALU : UG_VALD);
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
#ifdef SELECTOR_BTN_PIN
    bool gp12_pressed = false;
#endif

#ifdef ENCODER_BTN_PIN
    encoder_btn_pressed = (gpio_read_pin(ENCODER_BTN_PIN) == 0);

if (encoder_btn_pressed && !encoder_btn_was_pressed) {
    encoder_btn_rotated = false;

    if (active_layer_raw() == _TEXT && text_selection_pending_copy) {
        tap_code16(C(KC_C));
        text_selection_pending_copy = false;

        // Verhindert, dass dieser Kopierdruck noch Encoder-Help auslöst
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

#ifdef SELECTOR_BTN_PIN
    gp12_pressed = (gpio_read_pin(SELECTOR_BTN_PIN) == 0);
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
        snprintf(line, sizeof(line), "SEL->%-6.6s FX:%s", layer_name_short(selector_target), rgb_minimal_mode ? "Q" : "W");
    } else if (layer == _PROMPT) {
        snprintf(line, sizeof(line), "PRM %-4s FX:%s", prompt_mode_name(prompt_mode), rgb_minimal_mode ? "Q" : "W");
    } else if (layer == _DEV) {
        snprintf(line, sizeof(line), "GME %-4s FX:%s", game_mode_name(game_mode), rgb_minimal_mode ? "Q" : "W");
    } else if (layer == _WINDOW && window_browser_held) {
        snprintf(line, sizeof(line), "%-7s FX:%s", "WIN BRO", rgb_minimal_mode ? "Q" : "W");
    } else if (layer == _TEXT && text_action_held) {
        snprintf(line, sizeof(line), "%-7s FX:%s", "TXT ACT", rgb_minimal_mode ? "Q" : "W");
    } else if (layer == _TEXT && text_edit_held) {
        snprintf(line, sizeof(line), "%-7s FX:%s", "TXT EDT", rgb_minimal_mode ? "Q" : "W");
    } else if (layer == _VSC) {
        snprintf(line, sizeof(line), "VSC %-4s FX:%s", current_vsc_preview_mode() == VSC_MODE_CHAT ? "CHAT" : "BAR", rgb_minimal_mode ? "Q" : "W");
    } else {
        snprintf(line, sizeof(line), "%-6s FX:%s", layer_name_short(layer), rgb_minimal_mode ? "Q" : "W");
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
    if (layer == _SELECT) render_legend_view(_SELECT);
    else if (oled_view == OLED_VIEW_LAST_KEY) render_last_key_view();
    else render_legend_view(layer);
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
        snprintf(line, sizeof(line), "PRM %-4s FX:%s", prompt_mode_name(prompt_mode), rgb_minimal_mode ? "Q" : "W");
    } else {
        snprintf(line, sizeof(line), "%-6s FX:%s", layer_name_short(layer), rgb_minimal_mode ? "Q" : "W");
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
    if (layer == _SELECT) render_legend_view(_SELECT);
    else if (oled_view == OLED_VIEW_LAST_KEY) render_last_key_view();
    else render_legend_view(layer);
    return false;
}
#endif

