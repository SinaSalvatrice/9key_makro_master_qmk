#include QMK_KEYBOARD_H
#include "gpio.h"
#include "i2c_master.h"
#ifdef VIA_ENABLE
#include "via.h"
#endif
#include <stdio.h>

#if defined(__GNUC__)
#    define MAYBE_UNUSED __attribute__((unused))
#else
#    define MAYBE_UNUSED
#endif

// ============================================================
// RGB / OLED selector build
// - GP12: tap cycles view, hold returns KEYS, double-tap opens STATUS
// - Long encoder-button hold shows temporary Encoder Help view
// - Selector grid: BASE/WIN/TXT, MED/GIT/GAME, VSC/RGB/PROMPT
// ============================================================

#ifndef RGBLIGHT_LED_COUNT
#define RGBLIGHT_LED_COUNT 65
#endif

#define RGB_KEYFIELD_LED_COUNT 15
#define RGB_FRAME_LED_FIRST    RGB_KEYFIELD_LED_COUNT
#define RGB_FRAME_LED_COUNT    (RGBLIGHT_LED_COUNT - RGB_FRAME_LED_FIRST)
#define RGB_CORE_LED_COUNT     RGB_KEYFIELD_LED_COUNT
#define RGB_GAP_LED_COUNT      6

#ifndef SELECTOR_BTN_PIN
#    define SELECTOR_BTN_PIN GP12
#endif

#define PAD_KEY_COUNT          9
#define RGB_FRAME_MS           33
#define BOOT_TOTAL_MS          2800
#define BUTTON_DEBOUNCE_MS     150
#define SELECTOR_DOUBLE_TAP_MS 300
#define OLED_DOUBLE_TAP_MS     320
#define OLED_HOLD_MS           450
#define CLEAR_EEPROM_HOLD_MS   3000
#define VIA_LAYER_SLOT_COUNT   9
#define REWORKED_LAYOUT_VERSION 9
#define ENCODER_HELP_HOLD_MS   700
#define ENCODER_HELP_SHOW_MS   2500
#define RGB_FRAME_WANDER_SHOW_MS 900

#ifndef ADXL345_ENABLE
#    define ADXL345_ENABLE
#endif

#ifdef ADXL345_ENABLE
#    define ADXL345_ADDR_PRIMARY    0x53
#    define ADXL345_ADDR_ALT        0x1D
#    define ADXL345_REG_DEVID       0x00
#    define ADXL345_REG_BW_RATE     0x2C
#    define ADXL345_REG_POWER_CTL   0x2D
#    define ADXL345_REG_DATA_FORMAT 0x31
#    define ADXL345_REG_DATAX0      0x32
#    define ADXL345_DEVID           0xE5
#    define ADXL345_I2C_TIMEOUT     100
#    define ADXL345_READ_MS         8
#    define ADXL345_FLUID_SHIFT     6
#    define ADXL345_FLUID_ACCEL_DIV 9
#    define ADXL345_FLUID_DAMP_NUM  2
#    define ADXL345_FLUID_DAMP_DEN  3
#    define ADXL345_GAME_TILT_ON    58
#    define ADXL345_GAME_TILT_OFF   34
#    ifndef ADXL345_GAME_AXIS_DEBOUNCE_MS
#        define ADXL345_GAME_AXIS_DEBOUNCE_MS 60
#    endif
#    ifndef ADXL345_MOUSE_AXIS_DEBOUNCE_MS
#        define ADXL345_MOUSE_AXIS_DEBOUNCE_MS 20
#    endif
#    ifndef ADXL345_MOUSE_TILT_ON
#        define ADXL345_MOUSE_TILT_ON 52
#    endif
#    ifndef ADXL345_MOUSE_TILT_OFF
#        define ADXL345_MOUSE_TILT_OFF 28
#    endif
#    ifndef ADXL345_MOUSE_BLEND_DIV
#        define ADXL345_MOUSE_BLEND_DIV 2
#    endif
#    ifndef ADXL345_GAME_DEADZONE
#        define ADXL345_GAME_DEADZONE 5
#    endif
#    ifndef ADXL345_GAME_FILTER_DIV
#        define ADXL345_GAME_FILTER_DIV 4
#    endif
#    ifndef ADXL345_GAME_SWAP_XY
#        define ADXL345_GAME_SWAP_XY 0
#    endif
#    ifndef ADXL345_GAME_INVERT_X
#        define ADXL345_GAME_INVERT_X 0
#    endif
#    ifndef ADXL345_GAME_INVERT_Y
#        define ADXL345_GAME_INVERT_Y 1
#    endif
#endif

// ── Layer enum ──────────────────────────────────────────────
enum layers {
    _BASE,
    _WINDOW,
    _TEXT,
    _MEDIA,
    _MEDIA_GAME,
    _DEV,
    _VSC,
    _RGB,
    _RGBMOD,
    _RGBADJ,
    _MARK,
    _WORK,
    _SYS, // repurposed as NAV layer
    _PROMPT,
    _SELECT,
    _GAME,
    _LAYER_COUNT
};

// ── Custom keycodes ─────────────────────────────────────────
enum custom_keycodes {
    SEL_BASE = QK_KB_0,
    SEL_WINDOW,
    SEL_TEXT,
    SEL_MEDIA,
    SEL_WORK,
    SEL_RGB,
    SEL_DEV,
    SEL_VSC,
    SEL_PROMPT,
    PRM_PICS,
    PRM_ETSY,
    GM_NAV,
    GM_MOUSE,
    GM_TILT,
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
    // QMK reserves only 32 keyboard-level custom slots (QK_KB_0..QK_KB_31).
    // Start the remaining local-only keycodes at SAFE_RANGE to keep introspection builds valid.
    TXT_ACT = SAFE_RANGE,
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
    RGB_SATD,
    WRK_APP,
    WRK_PULL,
    WRK_COMMIT,
    WRK_PUSH,
    WRK_BRANCH,
    WRK_PR,
    WRK_SYNC
};

enum tap_dance_ids {
    TD_LAYER_SELECT,
    TD_SEL_WIN_MARK,
    TD_SEL_MEDIA_GAME,
    TD_SEL_DEV_GAME,
    TD_SEL_VSC_SYS,
};

enum rgb_zone_bits {
    RGB_ZONE_FRAME = 1 << 0,
    RGB_ZONE_KEY   = 1 << 1,
    RGB_ZONE_GAP   = 1 << 2,
    RGB_ZONE_ALL   = RGB_ZONE_FRAME | RGB_ZONE_KEY | RGB_ZONE_GAP,
};

typedef enum {
    RGB_ANIM_FX = 0,
    RGB_ANIM_TILT_PLACEHOLDER,
    RGB_ANIM_LAYER_KEYS,
    RGB_ANIM_REACTIVE,
    RGB_ANIM_FRAME_WANDER,
    RGB_ANIM_COUNT
} rgb_animation_mode_t;

static bool rgb_output_enabled = true;
static bool rgb_mode_held = false;
static uint8_t rgb_zone_mask = RGB_ZONE_ALL;
static rgb_animation_mode_t rgb_animation_mode = RGB_ANIM_FX;
static uint32_t rgb_reactive_started = 0;
static uint8_t rgb_reactive_key_index = 0;
static uint32_t rgb_frame_wander_started = 0;
static bool rgb_frame_wander_clockwise = true;

#ifdef ADXL345_ENABLE
static bool adxl345_ready = false;
static uint8_t adxl345_addr = ADXL345_ADDR_PRIMARY;
static int16_t adxl345_x = 0;
static int16_t adxl345_y = 0;
static int16_t adxl345_z = 0;
static int16_t adxl345_game_x = 0;
static int16_t adxl345_game_y = 0;
static int16_t adxl345_zero_x = 0;
static int16_t adxl345_zero_y = 0;
static int16_t adxl345_zero_z = 0;
static int16_t adxl345_last_x = 0;
static int16_t adxl345_last_y = 0;
static int16_t adxl345_last_z = 0;
static uint16_t adxl345_motion = 0;
static int16_t adxl345_fluid_x = 0;
static int16_t adxl345_fluid_y = 0;
static uint16_t adxl345_fluid_motion = 0;
static uint32_t adxl345_last_read = 0;
static int32_t adxl345_cal_sum_x = 0;
static int32_t adxl345_cal_sum_y = 0;
static int32_t adxl345_cal_sum_z = 0;
static uint8_t adxl345_cal_count = 0;
static bool adxl345_calibrated = false;
static int32_t adxl345_fluid_x_fp = 0;
static int32_t adxl345_fluid_y_fp = 0;
static int32_t adxl345_fluid_vx_fp = 0;
static int32_t adxl345_fluid_vy_fp = 0;
#endif

typedef enum {
    VSC_MODE_NONE,
    VSC_MODE_BAR,
    VSC_MODE_CHAT,
} vsc_mode_t;

typedef enum {
    OLED_VIEW_LEGEND,
    OLED_VIEW_TAP,
    OLED_VIEW_RGB_PAGE,
    OLED_VIEW_HELP,
    OLED_VIEW_ENCODER,
    OLED_VIEW_COUNT
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
    RGB_EFFECT_NAV_BLINK,
    RGB_EFFECT_TYPEWRITER,
    RGB_EFFECT_VISUALIZER,
    RGB_EFFECT_PONG,
    RGB_EFFECT_PACKET,
    RGB_EFFECT_BLOOM,
    RGB_EFFECT_SWEEP,
    RGB_EFFECT_TILT,
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
    WINDOW_MODE_SNAP,
} window_mode_t;

typedef enum {
    GAME_MODE_NAV,
    GAME_MODE_MOUSE,
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
static vsc_mode_t last_vsc_mode           = VSC_MODE_NONE;
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
static bool window_snap_held              = false;
static window_mode_t last_key_window_mode = WINDOW_MODE_WIN;
static game_mode_t game_mode              = GAME_MODE_MOUSE;
static game_mode_t last_key_game_mode     = GAME_MODE_MOUSE;
static bool game_tilt_enabled             = true;
static bool tilt_game_up_held             = false;
static bool tilt_game_down_held           = false;
static bool tilt_game_left_held           = false;
static bool tilt_game_right_held          = false;
static bool tilt_game_mouse_up_held       = false;
static bool tilt_game_mouse_down_held     = false;
static bool tilt_game_mouse_left_held     = false;
static bool tilt_game_mouse_right_held    = false;
static int8_t tilt_game_x_state           = 0;
static int8_t tilt_game_y_state           = 0;
static int8_t tilt_game_x_pending         = 0;
static int8_t tilt_game_y_pending         = 0;
static uint32_t tilt_game_x_pending_since = 0;
static uint32_t tilt_game_y_pending_since = 0;
static vsc_mode_t last_key_vsc_mode       = VSC_MODE_BAR;
static prompt_mode_t prompt_mode          = PROMPT_MODE_BASE;
static prompt_mode_t last_key_prompt_mode = PROMPT_MODE_BASE;
static bool last_key_rgb_mode             = false;
static uint32_t encoder_help_started      = 0;
static uint32_t encoder_help_until        = 0;
static bool encoder_help_fired            = false;
static uint8_t selector_origin_layer      = _BASE;
static uint32_t selector_last_tap         = 0;
static uint8_t scale_val(uint8_t value, uint8_t scale);
static bool layer_mode_key_for_layer(uint8_t layer, uint8_t key_index);
static void flush_led_frame(void);
static void set_key_hsv(uint8_t key_index, uint8_t h, uint8_t s, uint8_t v);
static void set_led_hsv(uint8_t led_index, uint8_t h, uint8_t s, uint8_t v);
static void clear_all_keys(void);

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

static const uint8_t gap_led_map[RGB_GAP_LED_COUNT] = {
    1, 3,
    6, 8,
    11, 13
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
    id_via_frame_color        = 7,
    id_via_frame_brightness   = 8,
    id_via_frame_effect       = 9,
    id_via_frame_effect_speed = 10,
    id_via_gap_color          = 11,
    id_via_gap_brightness     = 12,
    id_via_gap_effect         = 13,
    id_via_gap_effect_speed   = 14,
};

#define VIA_CUSTOM_CHANNEL_ID 1

typedef struct {
    uint8_t      signature;
    uint8_t      layout_version;
    uint8_t      oled_view;
    uint8_t      fx_mode;
    uint8_t      layer_effect[VIA_LAYER_SLOT_COUNT];
    uint8_t      layer_speed[VIA_LAYER_SLOT_COUNT];
    hsv_config_t layer_palette[VIA_LAYER_SLOT_COUNT];
    uint8_t      gap_effect[VIA_LAYER_SLOT_COUNT];
    uint8_t      gap_speed[VIA_LAYER_SLOT_COUNT];
    hsv_config_t gap_palette[VIA_LAYER_SLOT_COUNT];
    uint8_t      frame_effect[VIA_LAYER_SLOT_COUNT];
    uint8_t      frame_speed[VIA_LAYER_SLOT_COUNT];
    hsv_config_t frame_palette[VIA_LAYER_SLOT_COUNT];
} via_user_config_t;

static via_user_config_t via_user_config;
#endif

// Selector grid in physical key order:
// BASE WIN TXT / MED GIT GAME / VSC RGB PROMPT
static select_slot_t select_slots[PAD_KEY_COUNT] = {
    { _BASE,   128, 220, 108, "BASE",   true  },
    { _WINDOW, 176, 240, 112, "WINDOW", true  },
    { _TEXT,    90, 230, 112, "TXT",    true  },
    { _MEDIA,   18, 255, 118, "MEDIA",  true  },
    { _WORK,    86, 220, 120, "GIT",    true  },
    { _DEV,     32, 255, 118, "GAME",   true  },
    { _VSC,    166, 255, 118, "VSC",    true  },
    { _RGB,    210, 255, 124, "RGB",    true  },
    { _PROMPT,  14, 255, 124, "PROMPT", true  },
};

static const uint8_t via_layer_slots[VIA_LAYER_SLOT_COUNT] = {
    0, 1, 2, 3, 5, 6, 7, 8, 4
};

// Order follows via_layer_slots: BASE, WINDOW, TEXT, MEDIA, GAME(old DEV slot), VSC, RGB, PROMPT, GIT.
static const hsv_config_t via_default_palette[VIA_LAYER_SLOT_COUNT] = {
    {128, 220, 108}, // BASE
    {176, 240, 112}, // WINDOW
    { 90, 230, 112}, // TEXT
    { 18, 255, 118}, // MEDIA
    { 32, 255, 118}, // GAME
    {166, 255, 118}, // VSC
    {210, 255, 124}, // RGB
    { 14, 255, 124}, // PROMPT
    { 86, 220, 120}, // GIT
};

static const uint8_t via_default_layer_effect[VIA_LAYER_SLOT_COUNT] = {
    RGB_EFFECT_SOLID,
    RGB_EFFECT_SOLID,
    RGB_EFFECT_SOLID,
    RGB_EFFECT_SOLID,
    RGB_EFFECT_SOLID,
    RGB_EFFECT_SOLID,
    RGB_EFFECT_RAINBOW,
    RGB_EFFECT_SOLID,
    RGB_EFFECT_SOLID,
};

static const uint8_t via_default_layer_speed[VIA_LAYER_SLOT_COUNT] = {
    88, 104, 96, 112, 112, 98, 124, 94, 108,
};

static const uint8_t via_default_gap_effect[VIA_LAYER_SLOT_COUNT] = {
    RGB_EFFECT_OFF,
    RGB_EFFECT_SCAN,
    RGB_EFFECT_TYPEWRITER,
    RGB_EFFECT_PULSE,
    RGB_EFFECT_SCAN,
    RGB_EFFECT_PACKET,
    RGB_EFFECT_RUNNING,
    RGB_EFFECT_PULSE,
    RGB_EFFECT_TWINKLE,
};

static const uint8_t via_default_gap_speed[VIA_LAYER_SLOT_COUNT] = {
    80, 104, 96, 108, 108, 110, 130, 92, 108,
};

static const hsv_config_t via_default_gap_palette[VIA_LAYER_SLOT_COUNT] = {
    {128, 180,  28},
    {176, 220,  42},
    { 90, 220,  48},
    { 18, 255,  44},
    { 32, 255,  40},
    {166, 255,  46},
    {210, 255,  56},
    { 14, 255,  44},
    { 42, 255,  44},
};

static const uint8_t via_default_frame_effect[VIA_LAYER_SLOT_COUNT] = {
    RGB_EFFECT_BREATHING,
    RGB_EFFECT_COMET,
    RGB_EFFECT_SWEEP,
    RGB_EFFECT_BREATHING,
    RGB_EFFECT_BREATHING,
    RGB_EFFECT_BREATHING,
    RGB_EFFECT_RAINBOW,
    RGB_EFFECT_BLOOM,
    RGB_EFFECT_BREATHING,
};

static const uint8_t via_default_frame_speed[VIA_LAYER_SLOT_COUNT] = {
    92, 106, 94, 110, 108, 100, 130, 92, 108,
};

static const hsv_config_t via_default_frame_palette[VIA_LAYER_SLOT_COUNT] = {
    {128, 200,  86},
    {176, 220,  90},
    { 90, 210,  88},
    { 18, 255,  92},
    { 32, 255,  90},
    {166, 240,  90},
    {210, 255, 104},
    { 14, 240,  92},
    {210,  96,  90},
};

static const char *const vsc_main_labels[6] = {"EXPL", "SRCH", "TERM", "SRC", "GIT", "RUN"};
static const char *const vsc_main_functions[6] = {"Explorer", "Search", "Terminal", "Source control", "Git or command palette", "Run task"};
static const char *const vsc_main_commands[6] = {
    "View: Show Explorer",
    "View: Show Search",
    "Terminal: Focus Terminal",
    "View: Show Source Control",
    "GitHub Pull Requests: Focus on GitHub Pull Requests View",
    "Tasks: Run Task"
};

static const char *const vsc_nav_labels[6] = {"FILE", "SYMB", "TERM", "BACK", "CMD", "FWD"};
static const char *const vsc_nav_functions[6] = {"Quick open", "Go to symbol", "Terminal", "Navigate back", "Command palette", "Navigate forward"};
static const char *const vsc_nav_commands[6] = {
    "Go to File...",
    "Go to Symbol in Editor...",
    "Terminal: Focus Terminal",
    "Go Back",
    "Show All Commands",
    "Go Forward"
};

static const char *const vsc_chat_labels[6] = {"EXPL", "REVW", "FIX", "TEST", "DOCS", "COMMIT"};
static const char *const vsc_chat_functions[6] = {"Explain", "Review", "Fix", "Test", "Docs", "Commit"};
static const char *const vsc_chat_macros[6] = {
    "Explain this code clearly.",

    "Review this code for bugs and risks.",

    "Suggest a minimal safe fix.",

    "Write focused tests for this.",

    "Document this function briefly.",

    "Create a concise commit message."
};

static const char *const prompt_base_labels[6] = {"SUM", "REVW", "FIX", "TEST", "EXPL", "COMMIT"};
static const char *const prompt_base_functions[6] = {"Prompt summarize", "Prompt review", "Prompt suggest fix", "Prompt write tests", "Prompt explain code", "Prompt commit message"};

static const char *const prompt_pics_labels[6] = {"GEAR", "LETT", "BGEXT", "WALL", "SVG", "MOCK"};
static const char *const prompt_pics_functions[6] = {"AI Gear", "letter tranform", "background extraction", "Clock on wall", "Clean background", "Mockup creation"};
static const char *const prompt_pics_macros[6] = {
    "Create a clean black-and-white vector-style silhouette image of four separate gears arranged in a 2x2 grid on a pure white background. Each gear should be the same size, symmetrical, and visually distinct, with a clean industrial machine aesthetic. The gears must be separate from each other and not interlocking. Design them as filled out black silhouettes with inner cutouts, chambers, and mechanical openings. Keep the overall look orderly, precise, technical, and suitable for SVG conversion and commercial sale. Front-facing flat design, pure black and white only.",

    "Transform to lowercase.",

    "Extract the background and fill whole image with it.",

    "Let the clock hang on a white suble structured concrete wall. Keep lighting, dont add shadows. Make it look as real as possible. Dont change the clock or its perspective",

    "Remove the disturbing objects in the background. take the dark background and replace the whole background with it. Keep lighting, dont add shadows. Make it look as real as possible. Dont change the object in the front or its perspective",

    "Create an Etsy mockup image for this product. Show the product in an appealing setting with good lighting and a complementary background. Include a styled text overlay with the product name and a catchy tagline. Make it look polished and professional, suitable for an online store listing. Do not include any logos, watermarks, or branding elements."
};

static const char *const prompt_etsy_labels[6] = {"TAGS", "TITLE", "DESC", "BULL", "LIST", "SEO"};
static const char *const prompt_etsy_functions[6] = {"Tag list", "Listing title", "Listing description", "Bullet highlights", "Listing creation", "Etsy SEO pass"};
static const char *const prompt_etsy_macros[6] = {
    "Generate Etsy tag ideas for this product. Create high-intent tags, avoid duplicates, vary phrase length, and explain which tags are strongest. Dont overuse broad tags, and dont use competitor or brand names. Focus on descriptive, specific, and relevant keywords that a buyer would search for. list them in one rows seperated by commas.",

    "Write multiple Etsy listing title options for this product. Optimize for clarity, search intent, and readability instead of stuffing every keyword.",

    "Write an Etsy listing description for this product. Start with a strong buyer-focused opening, then cover features, materials, size, usage, and care. Keep in mind Etsy's SEO best practices, character limits, and formatting. Use clear language.",

    "Write concise highlights for this Etsy product listing. Focus on benefits, materials, sizing, personalization, and gift appeal. Use subtraction signs instead of bullet points.",

    "Create a complete Etsy listing draft for this product, including title, description, tags, image plan, and quick notes on pricing or variation structure.",

    "Perform an Etsy SEO pass on this listing draft. Improve titles, tags, wording, scannability, and conversion clarity without making it sound spammy."
};

static const char *const text_win_labels[6]    = {"HOME", "UP", "END", "LEFT", "DOWN", "RGHT"};
static const char *const text_action_labels[6] = {"COPY", "CUT", "PASTE", "UNDO", "REDO", "SAVE"};
static const char *const text_edit_labels[6]   = {"WRD<", "DELW", "WRD>", "LINE<", "DELL", "LINE>"};

static const char *const text_win_functions[6]    = {"Line start", "Cursor up", "Line end", "Cursor left", "Cursor down", "Cursor right"};
static const char *const text_action_functions[6] = {"Copy", "Cut", "Paste", "Undo", "Redo", "Save"};
static const char *const text_edit_functions[6]   = {"Word left", "Delete word", "Word right", "Line start", "Delete line", "Line end"};

static const char *const window_win_labels[6]     = {"DESK<", "TASK", "DESK>", "WIN<", "SHOW", "WIN>"};
static const char *const window_browser_labels[6] = {"BACK", "REFR", "FWD", "TAB<", "NEW", "TAB>"};
static const char *const window_snap_labels[6]    = {"MAX", "UP", "MIN", "LEFT", "DOWN", "RGHT"};

static const char *const window_win_functions[6]     = {"Previous desktop", "Task view", "Next desktop", "Previous window", "Show desktop", "Next window"};
static const char *const window_browser_functions[6] = {"Browser back", "Refresh page", "Browser forward", "Previous tab", "New tab", "Next tab"};
static const char *const window_snap_functions[6]    = {"Maximize window", "Snap up", "Minimize window", "Snap left", "Snap down", "Snap right"};

static const char *const game_nav_labels[6]   = {"ESC", "UP", "ENT", "LEFT", "DOWN", "RGHT"};
static const char *const game_mouse_labels[6] = {"SHFT", "W", "SPC", "A", "S", "D"};

static const char *const game_nav_functions[6]   = {"Back out of menu", "Menu up", "Confirm or interact", "Menu left", "Menu down", "Menu right"};
static const char *const game_mouse_functions[6] = {"One-shot sprint or crouch", "Move forward", "Jump or confirm", "Move left", "Move back", "Move right"};

static const char *const layer_legend[_LAYER_COUNT][PAD_KEY_COUNT] = {
    [_BASE]   = {"SEL",  "UP",   "BSPC", "LEFT", "ENT",  "RGHT", "UNDO", "DOWN", "REDO"},
    [_WINDOW] = {"SEL",  "BRO",  "SNAP", "DESK<","TASK", "DESK>","WIN<", "SHOW", "WIN>"},
    [_TEXT]   = {"SEL",  "ACT",  "EDIT", "HOME", "UP",   "END",  "LEFT", "DOWN", "RGHT"},
    [_MEDIA]  = {"SEL",  "PREV", "NEXT", "VOL-", "PLAY", "VOL+", "RWND", "MUTE", "FFWD"},
    [_MEDIA_GAME] = {"SEL", "L", "K", "I", "U", "H", "J", "O", "ESC"},
    [_RGB]    = {"SEL",  "ZONE", "ANIM",  "HUE+",  "HUE-",  "VAL+",  "SAT+",  "SAT-",  "VAL-"},
    [_RGBMOD] = {"SEL",  "MOD",  "I|0",  "FRME", "KEY",  "GAP",  "FREE1", "FREE2", "FREE3"},
    [_RGBADJ] = {"SEL",  "SPD-", "ADJST", "VAL-", "HUE+", "HUE-", "VAL+", "SAT+", "SAT-"},
    [_MARK]   = {"SEL",  "WEB",  "APP",   "SHOP", "AI",   "DEV",  "MAIL", "FILE", "NAV"},
    [_WORK]   = {"SEL",  "VSC",  "DESK", "PULL", "COMMIT", "PUSH", "BRCH", "PR", "SYNC"},
    // Repurpose the SYS layer as a global navigation (NAV) layer.  The
    // legend strings reflect the new key assignments: arrow keys on the
    // middle row/column plus Page Up/Down and Home/End on the corners.  The
    // center key retains its base-layer tap function (Enter) while acting as
    // the momentary switch into this NAV layer on other layers via LT().
    [_SYS]    = {"SEL",  "UP",   "PGUP",  "LEFT", "ENT", "RGHT", "HOME", "DOWN", "PGDN"},
    [_DEV]    = {"SEL",  "NAV",  "MOUSE", "ESC",  "UP",   "ENT",  "LEFT", "DOWN", "RGHT"},
    [_GAME]   = {"SEL",  "LCLK", "RCLK",  "LEFT", "UP",   "RGHT", "MCLK", "DOWN", "TILT"},
    [_VSC]    = {"SEL",  "NAV",  "AI",   "EXPL", "SRCH", "TERM", "SRC",  "GIT",  "RUN"},
    [_PROMPT] = {"SEL",  "PICS", "ETSY", "SUM",  "REVW", "FIX",  "TEST", "EXPL", "COMMIT"},
    [_SELECT] = {"BASE", "WIN+", "TXT",  "MED+", "GIT", "GAME+", "VSC+", "RGB", "PROMPT"},
};

static const char *const layer_function[_LAYER_COUNT][PAD_KEY_COUNT] = {
    [_BASE]   = {"Select layer", "Arrow up", "Backspace", "Arrow left", "Enter", "Arrow right", "Undo", "Arrow down", "Redo"},
    [_WINDOW] = {"Select layer", "Hold browser controls", "Hold snap controls", "Prev desktop", "Task view", "Next desktop", "Prev window", "Show desktop", "Next window"},
    [_TEXT]   = {"Select layer", "Hold text actions", "Hold edit tools", "Line start", "Cursor up", "Line end", "Cursor left", "Cursor down", "Cursor right"},
    [_MEDIA]  = {"Select layer", "Previous track", "Next track", "Volume down", "Play/Pause", "Volume up", "Rewind", "Mute", "Fast forward"},
    [_MEDIA_GAME] = {"Select layer", "Type L", "Type K", "Type I", "Type U", "Type H", "Type J", "Type O", "Escape"},
    [_RGB]    = {"Select layer", "Hold RGB zone controls", "Cycle RGB animation mode", "Hue up", "Hue down", "Brightness up", "Saturation up", "Saturation down", "Brightness down"},
    [_RGBMOD] = {"Select layer", "Hold RGB mod layer", "Toggle all RGB groups", "Toggle frame LEDs", "Toggle key LEDs", "Toggle gap LEDs", "Free slot", "Free slot", "Free slot"},
    [_RGBADJ] = {"Select layer", "Speed down", "Hold RGB adjust layer", "Brightness down", "Hue up", "Hue down", "Brightness up", "Saturation up", "Saturation down"},
    [_MARK]   = {"Select layer", "Web shortcuts", "App shortcuts", "Shop shortcuts", "AI shortcuts", "Dev shortcuts", "Mail/calendar", "Folders/files", "Nav shortcuts"},
    [_WORK]   = {"Select layer", "Go to VSC layer", "Open GitHub Desktop or app", "git pull", "git add/commit prompt", "git push", "Create branch", "Create PR in browser", "git status"},
    // Update function descriptions for the repurposed NAV layer.  The
    // descriptions mirror the legend above.
    [_SYS]    = {"Select layer", "Arrow up", "Page up", "Arrow left", "Enter", "Arrow right", "Home", "Arrow down", "Page down"},
    [_DEV]    = {"Select layer", "Switch to menu navigation", "Switch to movement controls", "Back out of menu", "Menu up", "Confirm or interact", "Menu left", "Menu down", "Menu right"},
    [_GAME]   = {"Select layer", "Mouse left click", "Mouse right click", "Mouse left", "Mouse up", "Mouse right", "Mouse middle click", "Mouse down", "Toggle tilt detection"},
    [_VSC]    = {"Select layer", "Hold VSC nav mode", "Hold AI prompts", "Explorer", "Search", "Terminal", "Source control", "Git or command palette", "Run task"},
    [_PROMPT] = {"Select layer", "Prompt picture tools", "Prompt Etsy tools", "Prompt summarize", "Prompt review", "Prompt suggest fix", "Prompt write tests", "Prompt explain code", "Prompt commit message"},
    // In the SELECT layer, the seventh key toggles between VSC and NAV.  Update the
    // description accordingly since SYS has been repurposed as NAV.
    [_SELECT] = {"Go to BASE layer", "1x WINDOW / 2x MARK", "Go to TXT layer", "1x MEDIA / 2x MED+", "Go to GIT layer", "1x GAME / 2x GAME+", "1x VSC / 2x NAV", "Go to RGB layer", "Go to PROMPT layer"},
};

static const char *layer_name_short(uint8_t l) {
    switch (l) {
        case _BASE:   return "BASE";
        case _WINDOW: return "WIN";
        case _TEXT:   return "TXT";
        case _MEDIA:  return "MED";
        case _MEDIA_GAME: return "MED+";
        case _RGB:    return "RGB";
        case _RGBMOD: return "MOD";
        case _RGBADJ: return "ADJ";
        case _MARK:   return "MARK";
        case _WORK:   return "GIT";
        case _SYS:    return "NAV";
        case _DEV:    return "GAME";
        case _GAME:   return "GAME+";
        case _VSC:    return "VSC";
        case _PROMPT: return "PROMPT";
        case _SELECT: return "SEL";
        default:      return "BASE";
    }
}

static MAYBE_UNUSED const char *layer_name_long(uint8_t l) {
    switch (l) {
        case _BASE:   return "BASE";
        case _WINDOW: return "WINDOW";
        case _TEXT:   return "TXT";
        case _MEDIA:  return "MEDIA";
        case _MEDIA_GAME: return "MEDIA ALT";
        case _RGB:    return "RGB";
        case _RGBMOD: return "RGB MOD";
        case _RGBADJ: return "ADJST";
        case _MARK:   return "BOOKMARK";
        case _WORK:   return "WORK";
        case _SYS:    return "NAV";
        case _DEV:    return "GAME";
        case _GAME:   return "GAME ALT";
        case _VSC:    return "VSC";
        case _PROMPT: return "PROMPT";
        case _SELECT: return "SELECT";
        default:      return "BASE";
    }
}

static uint8_t canonical_rgb_layer(uint8_t layer) {
    if (layer == _RGBMOD || layer == _RGBADJ) return _RGB;
    if (layer == _MEDIA_GAME) return _MEDIA;
    if (layer == _GAME) return _DEV;
    return layer;
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
    if (window_browser_held) return WINDOW_MODE_BROWSER;
    if (window_snap_held) return WINDOW_MODE_SNAP;
    return WINDOW_MODE_WIN;
}

static game_mode_t current_game_preview_mode(void) {
    return game_mode;
}

static const char *encoder_function_for_layer(uint8_t layer) {
    switch (layer) {
        case _BASE:   return "Wheel scroll up/down";
        case _WINDOW: return window_browser_held ? "Browser page prev/next" : (window_snap_held ? "Snap left/right" : (encoder_btn_pressed ? "Desktop prev/next" : "Alt-Tab window switch"));
        case _TEXT:   return encoder_btn_pressed ? "Select text left/right" : "Move cursor left/right";
        case _MEDIA:  return "Volume up/down";
        case _MEDIA_GAME: return "Volume up/down";
        case _RGB:    return "RGB brightness +/-";
        case _RGBMOD: return "RGB brightness +/-";
        case _RGBADJ: return "RGB brightness +/-";
        case _DEV:    return "Weapon or inventory scroll";
        case _GAME:   return "Mouse wheel up/down";
        case _VSC:    return "VSCode page prev/next";
        case _PROMPT: return "VSCode page prev/next";
        case _SELECT: return encoder_btn_pressed ? "Choose target layer" : "Hold encoder btn first";
        default:      return "Encoder fallback volume";
    }
}

static const char *text_label_for_mode(text_mode_t mode, uint8_t index) {
    if (index == 0) return "SEL";
    if (index == 1) return "ACT";
    if (index == 2) return "EDIT";
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
    if (index == 2) return "Hold edit tools";
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
    if (index == 2) return "SNAP";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        if (mode == WINDOW_MODE_BROWSER) return window_browser_labels[slot];
        if (mode == WINDOW_MODE_SNAP) return window_snap_labels[slot];
        return window_win_labels[slot];
    }
    return "----";
}

static const char *window_function_for_mode(window_mode_t mode, uint8_t index) {
    if (index == 0) return "Select layer";
    if (index == 1) return "Hold browser controls";
    if (index == 2) return "Hold snap controls";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        if (mode == WINDOW_MODE_BROWSER) return window_browser_functions[slot];
        if (mode == WINDOW_MODE_SNAP) return window_snap_functions[slot];
        return window_win_functions[slot];
    }
    return "Unknown";
}

static MAYBE_UNUSED const char *text_label_for(uint8_t index) { return text_label_for_mode(current_text_preview_mode(), index); }
static MAYBE_UNUSED const char *text_function_for(uint8_t index) { return text_function_for_mode(current_text_preview_mode(), index); }
static MAYBE_UNUSED const char *window_label_for(uint8_t index) { return window_label_for_mode(current_window_preview_mode(), index); }
static MAYBE_UNUSED const char *window_function_for(uint8_t index) { return window_function_for_mode(current_window_preview_mode(), index); }

static const char *rgb_animation_label(void) {
    switch (rgb_animation_mode) {
        case RGB_ANIM_FX:           return "FX";
        case RGB_ANIM_TILT_PLACEHOLDER: return "TILT";
        case RGB_ANIM_LAYER_KEYS:   return "LKEY";
        case RGB_ANIM_REACTIVE:     return "RACT";
        case RGB_ANIM_FRAME_WANDER: return "WALK";
        default:                    return "FX";
    }
}

static const char *rgb_animation_function(void) {
    switch (rgb_animation_mode) {
        case RGB_ANIM_FX:           return "VIA/layer effects";
        case RGB_ANIM_TILT_PLACEHOLDER: return adxl345_ready ? "Tilt-reactive ADXL345 RGB" : "Tilt sensor not found";
        case RGB_ANIM_LAYER_KEYS:   return "Mode keys only";
        case RGB_ANIM_REACTIVE:     return "Reactive key flash";
        case RGB_ANIM_FRAME_WANDER: return "Encoder frame chase";
        default:                    return "VIA/layer effects";
    }
}

static const char *rgb_label_for_mode(bool mode_held, uint8_t index) {
    static const char *const normal_labels[PAD_KEY_COUNT] = {
        "SEL", "ZONE", "ANIM",
        "HUE+", "HUE-", "VAL+",
        "SAT+", "SAT-", "VAL-"
    };
    static const char *const mode_labels[PAD_KEY_COUNT] = {
        "SEL", "ZONE", "ALL",
        "FRME", "KEY",  "GAP",
        "----", "----", "----"
    };

    if (index >= PAD_KEY_COUNT) return "----";
    if (!mode_held && index == 2) return rgb_animation_label();
    return mode_held ? mode_labels[index] : normal_labels[index];
}

static const char *rgb_function_for_mode(bool mode_held, uint8_t index) {
    static const char *const normal_functions[PAD_KEY_COUNT] = {
        "Select layer", "Hold RGB zone controls", "Cycle RGB animation mode",
        "Hue up", "Hue down", "Brightness up",
        "Saturation up", "Saturation down", "Brightness down"
    };
    static const char *const mode_functions[PAD_KEY_COUNT] = {
        "Select layer", "Hold RGB zone controls", "Toggle all LED zones",
        "Toggle frame LEDs", "Toggle key LEDs", "Toggle gap LEDs",
        "Unused", "Unused", "Unused"
    };

    if (index >= PAD_KEY_COUNT) return "Unknown";
    if (!mode_held && index == 2) return rgb_animation_function();
    return mode_held ? mode_functions[index] : normal_functions[index];
}

static MAYBE_UNUSED const char *rgb_label_for(uint8_t index) { return rgb_label_for_mode(rgb_mode_held, index); }
static MAYBE_UNUSED const char *rgb_function_for(uint8_t index) { return rgb_function_for_mode(rgb_mode_held, index); }

static const char *game_label_for_mode(game_mode_t mode, uint8_t index) {
    if (index == 0) return "SEL";
    if (index == 1) return "NAV";
    if (index == 2) return "MOUSE";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        return mode == GAME_MODE_NAV ? game_nav_labels[slot] : game_mouse_labels[slot];
    }
    return "----";
}

static const char *game_function_for_mode(game_mode_t mode, uint8_t index) {
    if (index == 0) return "Select layer";
    if (index == 1) return "Switch to menu navigation";
    if (index == 2) return "Switch to tilt mouse controls";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        return mode == GAME_MODE_NAV ? game_nav_functions[slot] : game_mouse_functions[slot];
    }
    return "Unknown";
}

static MAYBE_UNUSED const char *game_label_for(uint8_t index) { return game_label_for_mode(current_game_preview_mode(), index); }
static MAYBE_UNUSED const char *game_function_for(uint8_t index) { return game_function_for_mode(current_game_preview_mode(), index); }

static const char *game_mode_name(game_mode_t mode) {
    return mode == GAME_MODE_NAV ? "NAV" : "MOUSE";
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

static MAYBE_UNUSED const char *prompt_label_for(uint8_t index) { return prompt_label_for_mode(prompt_mode, index); }
static MAYBE_UNUSED const char *prompt_function_for(uint8_t index) { return prompt_function_for_mode(prompt_mode, index); }

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
    if (index == 1) return "NAV";
    if (index == 2) return "AI";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        if (mode == VSC_MODE_CHAT) return vsc_chat_labels[slot];
        if (mode == VSC_MODE_BAR) return vsc_nav_labels[slot];
        return vsc_main_labels[slot];
    }
    return "----";
}

static const char *vsc_function_for(vsc_mode_t mode, uint8_t index) {
    if (index == 0) return "Select layer";
    if (index == 1) return "Hold VSC nav mode";
    if (index == 2) return "Hold AI prompts";
    if (index >= 3 && index < 9) {
        uint8_t slot = index - 3;
        if (mode == VSC_MODE_CHAT) return vsc_chat_functions[slot];
        if (mode == VSC_MODE_BAR) return vsc_nav_functions[slot];
        return vsc_main_functions[slot];
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

    if (layer == _MARK) layer = _WINDOW;
    if (layer == _SYS) layer = _VSC;

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
    return UINT8_MAX;
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

static bool rgb_tilt_visual_active(void) {
    if (rgb_animation_mode == RGB_ANIM_TILT_PLACEHOLDER) return true;
    if (rgb_animation_mode != RGB_ANIM_FX) return false;
    return effect_for_layer(canonical_rgb_layer(active_layer_raw())) == RGB_EFFECT_TILT;
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

static hsv_config_t gap_palette_for_layer(uint8_t layer) {
#ifdef VIA_ENABLE
    uint8_t index = via_palette_index_for_layer(layer);
    if (index < VIA_LAYER_SLOT_COUNT) {
        return via_user_config.gap_palette[index];
    }
#endif
    return palette_for_layer(layer);
}

static rgb_effect_mode_t gap_effect_for_layer(uint8_t layer) {
#ifdef VIA_ENABLE
    uint8_t index = via_palette_index_for_layer(layer);
    if (index < VIA_LAYER_SLOT_COUNT && via_user_config.gap_effect[index] < RGB_EFFECT_COUNT) {
        return (rgb_effect_mode_t)via_user_config.gap_effect[index];
    }
#endif
    return effect_for_layer(layer);
}

static uint8_t gap_speed_for_layer(uint8_t layer) {
#ifdef VIA_ENABLE
    uint8_t index = via_palette_index_for_layer(layer);
    if (index < VIA_LAYER_SLOT_COUNT && via_user_config.gap_speed[index] >= 1) {
        return via_user_config.gap_speed[index];
    }
#endif
    return effect_speed_for_layer(layer);
}

static uint16_t gap_effect_period_for_layer(uint8_t layer, uint16_t base_period) {
    uint8_t speed = gap_speed_for_layer(layer);
    if (speed < 1) speed = 1;
    uint32_t period = ((uint32_t)base_period * 128UL) / speed;
    if (period < 40) period = 40;
    if (period > 20000) period = 20000;
    return (uint16_t)period;
}

static hsv_config_t frame_palette_for_layer(uint8_t layer) {
#ifdef VIA_ENABLE
    uint8_t index = via_palette_index_for_layer(layer);
    if (index < VIA_LAYER_SLOT_COUNT) {
        return via_user_config.frame_palette[index];
    }
#endif
    return palette_for_layer(layer);
}

static rgb_effect_mode_t frame_effect_for_layer(uint8_t layer) {
#ifdef VIA_ENABLE
    uint8_t index = via_palette_index_for_layer(layer);
    if (index < VIA_LAYER_SLOT_COUNT && via_user_config.frame_effect[index] < RGB_EFFECT_COUNT) {
        return (rgb_effect_mode_t)via_user_config.frame_effect[index];
    }
#endif
    return RGB_EFFECT_BREATHING;
}

static uint8_t frame_speed_for_layer(uint8_t layer) {
#ifdef VIA_ENABLE
    uint8_t index = via_palette_index_for_layer(layer);
    if (index < VIA_LAYER_SLOT_COUNT && via_user_config.frame_speed[index] >= 1) {
        return via_user_config.frame_speed[index];
    }
#endif
    return effect_speed_for_layer(layer);
}

static uint16_t frame_effect_period_for_layer(uint8_t layer, uint16_t base_period) {
    uint8_t speed = frame_speed_for_layer(layer);
    if (speed < 1) speed = 1;
    uint32_t period = ((uint32_t)base_period * 128UL) / speed;
    if (period < 40) period = 40;
    if (period > 20000) period = 20000;
    return (uint16_t)period;
}

#ifdef RGBLIGHT_ENABLE

static void render_rgb_layer_key_mode(void) {
    uint8_t layer = canonical_rgb_layer(active_layer_raw());
    hsv_config_t p = palette_for_layer(layer);

    clear_all_keys();

    for (uint8_t key = 0; key < PAD_KEY_COUNT; key++) {
        if (layer_mode_key_for_layer(layer, key)) {
            uint8_t val = key == 0 ? p.val : palette_floor((p.val * 4) / 5, 24);
            set_key_hsv(key, p.hue, p.sat, val);
        }
    }

    flush_led_frame();
}

static void render_rgb_reactive_mode(void) {
    uint8_t layer = canonical_rgb_layer(active_layer_raw());
    hsv_config_t p = palette_for_layer(layer);
    uint32_t elapsed = timer_elapsed32(rgb_reactive_started);

    clear_all_keys();

    if (rgb_reactive_started != 0 && elapsed < 700 && rgb_reactive_key_index < PAD_KEY_COUNT) {
        uint8_t fade = (uint8_t)(255 - ((elapsed * 255UL) / 700));
        uint8_t val = palette_floor(scale_val(p.val, fade), 8);
        set_key_hsv(rgb_reactive_key_index, p.hue, p.sat, val);

        if (rgb_reactive_key_index > 0) {
            set_key_hsv(rgb_reactive_key_index - 1, p.hue, p.sat, scale_val(val, 90));
        }
        if (rgb_reactive_key_index + 1 < PAD_KEY_COUNT) {
            set_key_hsv(rgb_reactive_key_index + 1, p.hue, p.sat, scale_val(val, 90));
        }
    }

    flush_led_frame();
}

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
    oled_view = via_user_config.oled_view <= OLED_VIEW_HELP ? (oled_view_t)via_user_config.oled_view : OLED_VIEW_LEGEND;
    for (uint8_t i = 0; i < VIA_LAYER_SLOT_COUNT; i++) apply_palette_entry(i);
}

static void set_via_config_defaults(void) {
    via_user_config.signature = 0x94;
    via_user_config.layout_version = REWORKED_LAYOUT_VERSION;
    via_user_config.oled_view = OLED_VIEW_LEGEND;
    via_user_config.fx_mode = 0;

    for (uint8_t i = 0; i < VIA_LAYER_SLOT_COUNT; i++) {
        via_user_config.layer_effect[i] = via_default_layer_effect[i];
        via_user_config.layer_speed[i] = via_default_layer_speed[i];
        via_user_config.layer_palette[i] = via_default_palette[i];
        via_user_config.gap_effect[i] = via_default_gap_effect[i];
        via_user_config.gap_speed[i] = via_default_gap_speed[i];
        via_user_config.gap_palette[i] = via_default_gap_palette[i];
        via_user_config.frame_effect[i] = via_default_frame_effect[i];
        via_user_config.frame_speed[i] = via_default_frame_speed[i];
        via_user_config.frame_palette[i] = via_default_frame_palette[i];
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
    if (via_user_config.oled_view > OLED_VIEW_HELP) invalid_config = true;
    if (via_user_config.fx_mode > 1) invalid_config = true;

    for (uint8_t i = 0; i < VIA_LAYER_SLOT_COUNT; i++) {
        if (via_user_config.layer_effect[i] >= RGB_EFFECT_COUNT) invalid_config = true;
        if (via_user_config.layer_speed[i] < 1) invalid_config = true;
        if (via_user_config.gap_effect[i] >= RGB_EFFECT_COUNT) invalid_config = true;
        if (via_user_config.gap_speed[i] < 1) invalid_config = true;
        if (via_user_config.frame_effect[i] >= RGB_EFFECT_COUNT) invalid_config = true;
        if (via_user_config.frame_speed[i] < 1) invalid_config = true;
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
            via_user_config.oled_view = value_data[0] <= OLED_VIEW_HELP ? value_data[0] : OLED_VIEW_LEGEND;
            oled_view = (oled_view_t)via_user_config.oled_view;
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
        case id_via_gap_effect:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                via_user_config.gap_effect[value_data[0]] = value_data[1] < RGB_EFFECT_COUNT ? value_data[1] : RGB_EFFECT_WILD;
            }
            break;
        case id_via_gap_effect_speed:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                via_user_config.gap_speed[value_data[0]] = value_data[1] < 1 ? 1 : value_data[1];
            }
            break;
        case id_via_gap_color:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                via_user_config.gap_palette[value_data[0]].hue = value_data[1];
                via_user_config.gap_palette[value_data[0]].sat = value_data[2];
            }
            break;
        case id_via_gap_brightness:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                via_user_config.gap_palette[value_data[0]].val = value_data[1];
            }
            break;
        case id_via_frame_effect:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                via_user_config.frame_effect[value_data[0]] = value_data[1] < RGB_EFFECT_COUNT ? value_data[1] : RGB_EFFECT_BREATHING;
            }
            break;
        case id_via_frame_effect_speed:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                via_user_config.frame_speed[value_data[0]] = value_data[1] < 1 ? 1 : value_data[1];
            }
            break;
        case id_via_frame_color:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                via_user_config.frame_palette[value_data[0]].hue = value_data[1];
                via_user_config.frame_palette[value_data[0]].sat = value_data[2];
            }
            break;
        case id_via_frame_brightness:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                via_user_config.frame_palette[value_data[0]].val = value_data[1];
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
        case id_via_gap_effect:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                value_data[1] = via_user_config.gap_effect[value_data[0]];
            }
            break;
        case id_via_gap_effect_speed:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                value_data[1] = via_user_config.gap_speed[value_data[0]];
            }
            break;
        case id_via_gap_color:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                uint8_t index = value_data[0];
                value_data[1] = via_user_config.gap_palette[index].hue;
                value_data[2] = via_user_config.gap_palette[index].sat;
            }
            break;
        case id_via_gap_brightness:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                value_data[1] = via_user_config.gap_palette[value_data[0]].val;
            }
            break;
        case id_via_frame_effect:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                value_data[1] = via_user_config.frame_effect[value_data[0]];
            }
            break;
        case id_via_frame_effect_speed:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                value_data[1] = via_user_config.frame_speed[value_data[0]];
            }
            break;
        case id_via_frame_color:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                uint8_t index = value_data[0];
                value_data[1] = via_user_config.frame_palette[index].hue;
                value_data[2] = via_user_config.frame_palette[index].sat;
            }
            break;
        case id_via_frame_brightness:
            if (value_data[0] < VIA_LAYER_SLOT_COUNT) {
                value_data[1] = via_user_config.frame_palette[value_data[0]].val;
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

    if (*channel_id == VIA_CUSTOM_CHANNEL_ID) {
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
    if (mode == VSC_MODE_BAR) {
        send_vsc_command(vsc_nav_commands[slot]);
    } else if (mode == VSC_MODE_CHAT) {
        send_string(vsc_chat_macros[slot]);
    } else {
        send_vsc_command(vsc_main_commands[slot]);
    }
}

static void tap_text_target(uint8_t slot) {
    if (slot >= 6) return;

    switch (current_text_preview_mode()) {
        case TEXT_MODE_ACTIONS:
            switch (slot) {
                case 0: tap_code16(C(KC_C)); break;
                case 1: tap_code16(C(KC_X)); break;
                case 2: tap_code16(C(KC_V)); break;
                case 3: tap_code16(C(KC_Z)); break;
                case 4: tap_code16(C(KC_Y)); break;
                case 5: tap_code16(C(KC_S)); break;
            }
            break;

        case TEXT_MODE_EDIT:
            switch (slot) {
                case 0: tap_code16(C(KC_LEFT)); break;
                case 1: tap_code16(C(KC_BSPC)); break;
                case 2: tap_code16(C(KC_RGHT)); break;
                case 3: tap_code(KC_HOME); break;
                case 4:
                    tap_code(KC_HOME);
                    tap_code16(S(KC_END));
                    tap_code(KC_DEL);
                    break;
                case 5: tap_code(KC_END); break;
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
            case 3: tap_code16(C(KC_PGUP)); break;
            case 4: tap_code16(C(KC_T)); break;
            case 5: tap_code16(C(KC_PGDN)); break;
        }
        return;
    }

    if (current_window_preview_mode() == WINDOW_MODE_SNAP) {
        switch (slot) {
            case 0: tap_code16(G(KC_UP)); break;
            case 1: tap_code16(G(KC_UP)); break;
            case 2: tap_code16(G(KC_DOWN)); break;
            case 3: tap_code16(G(KC_LEFT)); break;
            case 4: tap_code16(G(KC_DOWN)); break;
            case 5: tap_code16(G(KC_RGHT)); break;
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

static void cycle_rgb_animation_mode(void) {
    rgb_animation_mode = (rgb_animation_mode_t)((rgb_animation_mode + 1) % RGB_ANIM_COUNT);
    if (rgb_animation_mode != RGB_ANIM_FRAME_WANDER) {
        rgb_frame_wander_started = 0;
    }

#ifdef RGBLIGHT_ENABLE
    render_rgb_layer_visuals();
#endif
}

static bool layer_mode_key_for_layer(uint8_t layer, uint8_t key_index) {
    layer = canonical_rgb_layer(layer);

    switch (layer) {
        case _BASE:
        case _MEDIA:
            return key_index == 0;
        case _WINDOW:
        case _TEXT:
        case _MARK:
        case _WORK:
        case _SYS:
        case _DEV:
        case _GAME:
        case _VSC:
        case _RGB:
        case _PROMPT:
            return key_index <= 2;
        case _SELECT:
            return key_index < PAD_KEY_COUNT && key_index != 0;
        default:
            return key_index == 0;
    }
}

static uint16_t abs_i16_u16(int16_t v) {
    return v < 0 ? (uint16_t)(-v) : (uint16_t)v;
}

static int8_t sign_i16(int16_t v) {
    if (v > 0) return 1;
    if (v < 0) return -1;
    return 0;
}

static MAYBE_UNUSED uint8_t clamp_u8_i16(int16_t value, uint8_t min, uint8_t max) {
    if (value < min) return min;
    if (value > max) return max;
    return (uint8_t)value;
}

#ifdef ADXL345_ENABLE
static bool adxl345_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len) {
    return i2c_read_register((uint8_t)(addr << 1), reg, data, len, ADXL345_I2C_TIMEOUT) == I2C_STATUS_SUCCESS;
}

static bool adxl345_write_reg(uint8_t addr, uint8_t reg, uint8_t value) {
    return i2c_write_register((uint8_t)(addr << 1), reg, &value, 1, ADXL345_I2C_TIMEOUT) == I2C_STATUS_SUCCESS;
}

static bool adxl345_probe(uint8_t addr) {
    uint8_t devid = 0;
    return adxl345_read_reg(addr, ADXL345_REG_DEVID, &devid, 1) && devid == ADXL345_DEVID;
}

static int16_t adxl345_apply_deadzone(int16_t value, uint8_t deadzone) {
    if (value > -(int16_t)deadzone && value < (int16_t)deadzone) return 0;
    return value;
}

static uint16_t abs_i32_u16(int32_t value) {
    if (value < 0) value = -value;
    return value > UINT16_MAX ? UINT16_MAX : (uint16_t)value;
}

static int32_t div_min_step_i32(int32_t value, int32_t divisor) {
    int32_t quotient = value / divisor;
    if (quotient == 0 && value != 0) return value > 0 ? 1 : -1;
    return quotient;
}

static void adxl345_reset_fluid_state(void) {
    adxl345_fluid_x = 0;
    adxl345_fluid_y = 0;
    adxl345_fluid_motion = 0;
    adxl345_fluid_x_fp = 0;
    adxl345_fluid_y_fp = 0;
    adxl345_fluid_vx_fp = 0;
    adxl345_fluid_vy_fp = 0;
}

static void adxl345_update_fluid_state(void) {
    int32_t target_x_fp = (int32_t)adxl345_x << ADXL345_FLUID_SHIFT;
    int32_t target_y_fp = (int32_t)adxl345_y << ADXL345_FLUID_SHIFT;
    int32_t dx_fp = target_x_fp - adxl345_fluid_x_fp;
    int32_t dy_fp = target_y_fp - adxl345_fluid_y_fp;
    uint32_t velocity = 0;

    adxl345_fluid_vx_fp += div_min_step_i32(dx_fp, ADXL345_FLUID_ACCEL_DIV);
    adxl345_fluid_vy_fp += div_min_step_i32(dy_fp, ADXL345_FLUID_ACCEL_DIV);

    adxl345_fluid_vx_fp = (adxl345_fluid_vx_fp * ADXL345_FLUID_DAMP_NUM) / ADXL345_FLUID_DAMP_DEN;
    adxl345_fluid_vy_fp = (adxl345_fluid_vy_fp * ADXL345_FLUID_DAMP_NUM) / ADXL345_FLUID_DAMP_DEN;

    adxl345_fluid_x_fp += adxl345_fluid_vx_fp;
    adxl345_fluid_y_fp += adxl345_fluid_vy_fp;

    adxl345_fluid_x = (int16_t)(adxl345_fluid_x_fp >> ADXL345_FLUID_SHIFT);
    adxl345_fluid_y = (int16_t)(adxl345_fluid_y_fp >> ADXL345_FLUID_SHIFT);

    velocity = (uint32_t)abs_i32_u16(adxl345_fluid_vx_fp) + (uint32_t)abs_i32_u16(adxl345_fluid_vy_fp);
    velocity >>= ADXL345_FLUID_SHIFT;
    if (velocity > UINT8_MAX) velocity = UINT8_MAX;
    adxl345_fluid_motion = (uint16_t)(((uint32_t)adxl345_fluid_motion * 7 + velocity) / 8);
}

static void update_tilt_game_control(uint16_t keycode, bool *held, bool pressed) {
    if (pressed == *held) return;

    if (pressed) {
        register_code(keycode);
    } else {
        unregister_code(keycode);
    }

    *held = pressed;
}

static int8_t adxl345_axis_desired_state(int16_t value, int8_t current_state, uint16_t on_threshold, uint16_t off_threshold) {
    uint16_t magnitude = abs_i16_u16(value);
    int8_t sign = sign_i16(value);

    if (current_state == 0) {
        if (sign == 0 || magnitude < on_threshold) return 0;
        return sign;
    }

    if (magnitude <= off_threshold) return 0;
    if (sign == 0 || sign == current_state) return current_state;

    // If we cross through zero with enough magnitude, switch immediately.
    return magnitude >= on_threshold ? sign : current_state;
}

static int8_t adxl345_axis_update_state(int16_t value, int8_t *state, int8_t *pending, uint32_t *pending_since, uint16_t debounce_ms, uint16_t on_threshold, uint16_t off_threshold) {
    int8_t desired = adxl345_axis_desired_state(value, *state, on_threshold, off_threshold);
    if (desired == *state) {
        *pending = *state;
        *pending_since = 0;
        return *state;
    }

    if (*pending_since == 0 || *pending != desired) {
        *pending = desired;
        *pending_since = timer_read32() | 1;
        return *state;
    }

    if (timer_elapsed32(*pending_since) >= debounce_ms) {
        *state = desired;
        *pending_since = 0;
    }

    return *state;
}

static void update_game_tilt_arrows(void) {
    uint8_t top_layer = active_layer_raw();
    bool tilt_layer_active = top_layer == _DEV || top_layer == _GAME;
    bool tilt_active = adxl345_ready && game_tilt_enabled && tilt_layer_active;
    bool nav_tilt_active = tilt_active && top_layer == _DEV && game_mode == GAME_MODE_NAV;
    bool mouse_tilt_active = tilt_active && (top_layer == _GAME || (top_layer == _DEV && game_mode == GAME_MODE_MOUSE));
    bool press_up = false;
    bool press_down = false;
    bool press_left = false;
    bool press_right = false;

    if (!tilt_active) {
        tilt_game_x_state = 0;
        tilt_game_y_state = 0;
        tilt_game_x_pending = 0;
        tilt_game_y_pending = 0;
        tilt_game_x_pending_since = 0;
        tilt_game_y_pending_since = 0;
    } else {
        int16_t axis_x = adxl345_game_x;
        int16_t axis_y = adxl345_game_y;
        uint16_t debounce_ms = ADXL345_GAME_AXIS_DEBOUNCE_MS;
        uint16_t on_threshold = ADXL345_GAME_TILT_ON;
        uint16_t off_threshold = ADXL345_GAME_TILT_OFF;

        if (mouse_tilt_active) {
            // IMPORTANT: adxl345_game_x/y are mapped into "game space" (swap/invert).
            // adxl345_fluid_x/y are kept in raw sensor space for smooth RGB visuals.
            // For mouse tilt we blend the two, so we must first map the fluid axes
            // into the same space to avoid sign/axis cancellation.
            int16_t fluid_x = adxl345_fluid_x;
            int16_t fluid_y = adxl345_fluid_y;

#if ADXL345_GAME_SWAP_XY
            int16_t tmp = fluid_x;
            fluid_x = fluid_y;
            fluid_y = tmp;
#endif
#if ADXL345_GAME_INVERT_X
            fluid_x = (int16_t)-fluid_x;
#endif
#if ADXL345_GAME_INVERT_Y
            fluid_y = (int16_t)-fluid_y;
#endif

#if ADXL345_MOUSE_BLEND_DIV <= 1
            axis_x = fluid_x;
            axis_y = fluid_y;
#else
            axis_x = (int16_t)(((int32_t)adxl345_game_x * (ADXL345_MOUSE_BLEND_DIV - 1) + fluid_x) / ADXL345_MOUSE_BLEND_DIV);
            axis_y = (int16_t)(((int32_t)adxl345_game_y * (ADXL345_MOUSE_BLEND_DIV - 1) + fluid_y) / ADXL345_MOUSE_BLEND_DIV);
#endif
            debounce_ms = ADXL345_MOUSE_AXIS_DEBOUNCE_MS;
            on_threshold = ADXL345_MOUSE_TILT_ON;
            off_threshold = ADXL345_MOUSE_TILT_OFF;
        }

        tilt_game_x_state = adxl345_axis_update_state(axis_x, &tilt_game_x_state, &tilt_game_x_pending, &tilt_game_x_pending_since, debounce_ms, on_threshold, off_threshold);
        tilt_game_y_state = adxl345_axis_update_state(axis_y, &tilt_game_y_state, &tilt_game_y_pending, &tilt_game_y_pending_since, debounce_ms, on_threshold, off_threshold);

        press_up = tilt_game_y_state > 0;
        press_down = tilt_game_y_state < 0;
        press_left = tilt_game_x_state < 0;
        press_right = tilt_game_x_state > 0;
    }

    update_tilt_game_control(KC_UP, &tilt_game_up_held, nav_tilt_active && press_up);
    update_tilt_game_control(KC_DOWN, &tilt_game_down_held, nav_tilt_active && press_down);
    update_tilt_game_control(KC_LEFT, &tilt_game_left_held, nav_tilt_active && press_left);
    update_tilt_game_control(KC_RGHT, &tilt_game_right_held, nav_tilt_active && press_right);

    update_tilt_game_control(MS_UP, &tilt_game_mouse_up_held, mouse_tilt_active && press_up);
    update_tilt_game_control(MS_DOWN, &tilt_game_mouse_down_held, mouse_tilt_active && press_down);
    update_tilt_game_control(MS_LEFT, &tilt_game_mouse_left_held, mouse_tilt_active && press_right);
    update_tilt_game_control(MS_RGHT, &tilt_game_mouse_right_held, mouse_tilt_active && press_left);
}

static uint8_t adxl345_map_axis_to_span(int16_t value, uint8_t span_len) {
    const int16_t range = 220;

    if (span_len <= 1) return 0;
    if (value < -range) value = -range;
    if (value > range) value = range;

    int32_t scaled = ((int32_t)(value + range) * (span_len - 1)) / (range * 2);
    if (scaled < 0) scaled = 0;
    if (scaled >= span_len) scaled = span_len - 1;
    return (uint8_t)scaled;
}

static uint8_t adxl345_frame_head(void) {
    const uint8_t top_len = (RGB_FRAME_LED_COUNT + 2) / 4;
    const uint8_t right_len = RGB_FRAME_LED_COUNT / 4;
    const uint8_t bottom_len = (RGB_FRAME_LED_COUNT + 1) / 4;
    const uint8_t left_len = RGB_FRAME_LED_COUNT - top_len - right_len - bottom_len;
    int16_t visual_y = -adxl345_fluid_y;
    uint16_t ax = abs_i16_u16(adxl345_fluid_x);
    uint16_t ay = abs_i16_u16(visual_y);

    if (RGB_FRAME_LED_COUNT == 0) return 0;

    if (ay >= ax) {
        if (visual_y >= 0) {
            return adxl345_map_axis_to_span(adxl345_fluid_x, top_len);
        }

        return (uint8_t)(top_len + right_len + adxl345_map_axis_to_span(-adxl345_fluid_x, bottom_len));
    }

    if (adxl345_fluid_x >= 0) {
        return (uint8_t)(top_len + adxl345_map_axis_to_span(-visual_y, right_len));
    }

    return (uint8_t)(top_len + right_len + bottom_len + adxl345_map_axis_to_span(visual_y, left_len));
}

static void adxl345_init(void) {
    adxl345_ready = false;
    adxl345_addr = ADXL345_ADDR_PRIMARY;
    adxl345_x = 0;
    adxl345_y = 0;
    adxl345_z = 0;
    adxl345_game_x = 0;
    adxl345_game_y = 0;
    adxl345_last_x = 0;
    adxl345_last_y = 0;
    adxl345_last_z = 0;
    adxl345_motion = 0;
    adxl345_zero_x = 0;
    adxl345_zero_y = 0;
    adxl345_zero_z = 0;
    adxl345_cal_sum_x = 0;
    adxl345_cal_sum_y = 0;
    adxl345_cal_sum_z = 0;
    adxl345_cal_count = 0;
    adxl345_calibrated = false;
    adxl345_reset_fluid_state();

    i2c_init();

    if (adxl345_probe(ADXL345_ADDR_PRIMARY)) {
        adxl345_addr = ADXL345_ADDR_PRIMARY;
    } else if (adxl345_probe(ADXL345_ADDR_ALT)) {
        adxl345_addr = ADXL345_ADDR_ALT;
    } else {
        return;
    }

    // Full resolution, +/-2g range. Good for tilt without needing scaling changes.
    if (!adxl345_write_reg(adxl345_addr, ADXL345_REG_DATA_FORMAT, 0x08)) return;
    // 100 Hz output data rate. We read at ~50 Hz.
    if (!adxl345_write_reg(adxl345_addr, ADXL345_REG_BW_RATE, 0x0A)) return;
    // Measurement mode.
    if (!adxl345_write_reg(adxl345_addr, ADXL345_REG_POWER_CTL, 0x08)) return;

    adxl345_ready = true;
    adxl345_last_read = 0;
}

static void adxl345_task(void) {
    if (!adxl345_ready) return;
    if (adxl345_last_read != 0 && timer_elapsed32(adxl345_last_read) < ADXL345_READ_MS) return;
    adxl345_last_read = timer_read32();

    uint8_t data[6] = {0};
    if (!adxl345_read_reg(adxl345_addr, ADXL345_REG_DATAX0, data, sizeof(data))) {
        adxl345_ready = false;
        return;
    }

    int16_t x = (int16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
    int16_t y = (int16_t)((uint16_t)data[2] | ((uint16_t)data[3] << 8));
    int16_t z = (int16_t)((uint16_t)data[4] | ((uint16_t)data[5] << 8));

    if (!adxl345_calibrated) {
        adxl345_cal_sum_x += x;
        adxl345_cal_sum_y += y;
        adxl345_cal_sum_z += z;
        adxl345_cal_count++;

        if (adxl345_cal_count >= 16) {
            adxl345_zero_x = (int16_t)(adxl345_cal_sum_x / (int32_t)adxl345_cal_count);
            adxl345_zero_y = (int16_t)(adxl345_cal_sum_y / (int32_t)adxl345_cal_count);
            adxl345_zero_z = (int16_t)(adxl345_cal_sum_z / (int32_t)adxl345_cal_count);
            adxl345_calibrated = true;
        }

        adxl345_x = 0;
        adxl345_y = 0;
        adxl345_z = 0;
        adxl345_game_x = 0;
        adxl345_game_y = 0;
        adxl345_motion = 0;
        adxl345_last_x = 0;
        adxl345_last_y = 0;
        adxl345_last_z = 0;
        adxl345_reset_fluid_state();
        return;
    }

    x -= adxl345_zero_x;
    y -= adxl345_zero_y;
    z -= adxl345_zero_z;

    x = adxl345_apply_deadzone(x, 4);
    y = adxl345_apply_deadzone(y, 4);
    z = adxl345_apply_deadzone(z, 4);

    x = (int16_t)(((int32_t)adxl345_x + x) / 2);
    y = (int16_t)(((int32_t)adxl345_y + y) / 2);
    z = (int16_t)(((int32_t)adxl345_z + z) / 2);

    uint16_t motion = abs_i16_u16(x - adxl345_last_x) + abs_i16_u16(y - adxl345_last_y) + abs_i16_u16(z - adxl345_last_z);
    motion = motion > 2 ? (uint16_t)(motion - 2) : 0;
    if (motion > UINT8_MAX) motion = UINT8_MAX;

    adxl345_x = x;
    adxl345_y = y;
    adxl345_z = z;
    adxl345_motion = motion;
    adxl345_last_x = x;
    adxl345_last_y = y;
    adxl345_last_z = z;
    adxl345_update_fluid_state();

    int16_t game_x = x;
    int16_t game_y = y;
#if ADXL345_GAME_SWAP_XY
    int16_t tmp = game_x;
    game_x = game_y;
    game_y = tmp;
#endif
#if ADXL345_GAME_INVERT_X
    game_x = (int16_t)-game_x;
#endif
#if ADXL345_GAME_INVERT_Y
    game_y = (int16_t)-game_y;
#endif

    game_x = adxl345_apply_deadzone(game_x, ADXL345_GAME_DEADZONE);
    game_y = adxl345_apply_deadzone(game_y, ADXL345_GAME_DEADZONE);

#if ADXL345_GAME_FILTER_DIV <= 1
    adxl345_game_x = game_x;
    adxl345_game_y = game_y;
#else
    adxl345_game_x = (int16_t)(((int32_t)adxl345_game_x * (ADXL345_GAME_FILTER_DIV - 1) + game_x) / ADXL345_GAME_FILTER_DIV);
    adxl345_game_y = (int16_t)(((int32_t)adxl345_game_y * (ADXL345_GAME_FILTER_DIV - 1) + game_y) / ADXL345_GAME_FILTER_DIV);
#endif
}

static MAYBE_UNUSED const char *adxl345_status_label(void) {
    return adxl345_ready ? "ADXL:OK" : "ADXL:NO";
}
#else
static void adxl345_init(void) {}
static void adxl345_task(void) {}
static MAYBE_UNUSED const char *adxl345_status_label(void) { return "ADXL:OFF"; }
#endif

#ifdef RGBLIGHT_ENABLE
static bool rgb_rendering_frame = false;

static void set_led_hsv(uint8_t led_index, uint8_t h, uint8_t s, uint8_t v) {
    if (led_index >= RGBLIGHT_LED_COUNT) return;

    bool is_frame = led_is_frame(led_index);
    if (is_frame && !rgb_rendering_frame) return;
    if (!is_frame && rgb_rendering_frame) return;

    if (!rgb_zone_enabled_for_led(led_index)) {
        rgblight_driver.set_color(led_index, 0, 0, 0);
        return;
    }

    if (v > RGBLIGHT_LIMIT_VAL) v = RGBLIGHT_LIMIT_VAL;
    rgb_t rgb = hsv_to_rgb((hsv_t){h, s, v});
    rgblight_driver.set_color(led_index, rgb.r, rgb.g, rgb.b);
}

static void clear_frame_light(void) {
    rgb_rendering_frame = true;
    for (uint8_t i = 0; i < RGB_FRAME_LED_COUNT; i++) {
        set_led_hsv(RGB_FRAME_LED_FIRST + i, 0, 0, 0);
    }
    rgb_rendering_frame = false;
}

static void clear_gap_light(void) {
    for (uint8_t i = 0; i < RGB_GAP_LED_COUNT; i++) {
        set_led_hsv(gap_led_map[i], 0, 0, 0);
    }
}

static void render_gap_light_group(void) {
    uint8_t layer = canonical_rgb_layer(active_layer_raw());
    rgb_effect_mode_t effect = gap_effect_for_layer(layer);
    hsv_config_t p = gap_palette_for_layer(layer);
    uint32_t now = timer_read32();

    if (rgb_animation_mode == RGB_ANIM_LAYER_KEYS || !rgb_output_enabled || !(rgb_zone_mask & RGB_ZONE_GAP) || effect == RGB_EFFECT_OFF) {
        clear_gap_light();
        return;
    }

    uint16_t slow = gap_effect_period_for_layer(layer, 1800);
    uint16_t fast = gap_effect_period_for_layer(layer, 650);
    uint8_t head = (now / (fast / RGB_GAP_LED_COUNT + 1)) % RGB_GAP_LED_COUNT;
    uint8_t scan = ping_pong_index(now, slow, RGB_GAP_LED_COUNT, 0);

    for (uint8_t i = 0; i < RGB_GAP_LED_COUNT; i++) {
        uint8_t led = gap_led_map[i];
        uint8_t hue = p.hue;
        uint8_t sat = p.sat;
        uint8_t val = palette_floor(p.val / 18, 4);
        uint8_t d;

        switch (effect) {
            case RGB_EFFECT_SOLID:
                val = palette_floor((p.val * 3) / 4, 16);
                break;

            case RGB_EFFECT_BREATHING:
                val = pulse_val(now, slow, i * 42, palette_floor(p.val / 12, 5), palette_floor((p.val * 3) / 4, 18));
                break;

            case RGB_EFFECT_RUNNING:
            case RGB_EFFECT_PACKET:
            case RGB_EFFECT_COMET:
                d = wrap_distance(i, head, RGB_GAP_LED_COUNT);
                if (d == 0) val = palette_floor((p.val * 4) / 5, 24);
                else if (d == 1) val = palette_floor(p.val / 2, 12);
                else val = palette_floor(p.val / 22, 3);
                break;

            case RGB_EFFECT_TWINKLE:
                val = palette_floor(p.val / 20, 4);
                val = clamp_add_u8(val, triwave8_period(now, 900 + i * 67, i * 37) / 6);
                if ((((now / 190) + i * 7) % 17) == 0) val = palette_floor((p.val * 4) / 5, 24);
                break;

            case RGB_EFFECT_PULSE:
            case RGB_EFFECT_BLOOM:
            case RGB_EFFECT_VISUALIZER:
                val = pulse_val(now, slow, i * 36, palette_floor(p.val / 14, 5), palette_floor((p.val * 4) / 5, 20));
                val = clamp_add_u8(val, triwave8_period(now, fast + i * 19, i * 23) / 10);
                break;

            case RGB_EFFECT_SCAN:
            case RGB_EFFECT_NAV_BLINK:
            case RGB_EFFECT_SWEEP:
                d = distance_u8(i, scan);
                if (d == 0) val = palette_floor((p.val * 4) / 5, 24);
                else if (d == 1) val = palette_floor(p.val / 2, 12);
                else val = palette_floor(p.val / 24, 3);
                break;

            case RGB_EFFECT_RAINBOW:
                hue = (uint8_t)((now * 255UL) / gap_effect_period_for_layer(layer, 2800)) + i * 31;
                sat = 255;
                val = pulse_val(now, slow, i * 19, palette_floor(p.val / 5, 16), palette_floor((p.val * 3) / 4, 20));
                break;

            case RGB_EFFECT_STACK:
            case RGB_EFFECT_TYPEWRITER:
                {
                    uint16_t period = gap_effect_period_for_layer(layer, 1400);
                    uint8_t filled = ((now % period) * (RGB_GAP_LED_COUNT + 1)) / period;
                    bool reverse = ((now / period) % 2) != 0;
                    uint8_t pos = reverse ? (RGB_GAP_LED_COUNT - 1 - i) : i;
                    val = pos < filled ? palette_floor((p.val * 2) / 3, 18) : palette_floor(p.val / 24, 3);
                    if (pos == filled && filled < RGB_GAP_LED_COUNT) val = palette_floor((p.val * 4) / 5, 24);
                }
                break;

            case RGB_EFFECT_PONG:
                d = distance_u8(i, scan);
                if (d == 0) val = palette_floor((p.val * 4) / 5, 24);
                else if (d == 1) val = palette_floor(p.val / 2, 12);
                else val = palette_floor(p.val / 26, 3);
                break;

            case RGB_EFFECT_TILT:
#ifdef ADXL345_ENABLE
                if (adxl345_ready) {
                    int16_t bias = (i < 3) ? adxl345_fluid_x : -adxl345_fluid_x;
                    if (bias < 0) bias = -bias;
                    val = palette_floor(scale_val(p.val, 60 + (uint8_t)(bias > 180 ? 120 : (bias * 2) / 3)), 8);
                    val = clamp_add_u8(val, adxl345_fluid_motion > 120 ? 40 : (uint8_t)(adxl345_fluid_motion / 4));
                } else
#endif
                {
                    val = pulse_val(now, slow, i * 53, palette_floor(p.val / 16, 4), palette_floor((p.val * 3) / 4, 18));
                }
                break;

            case RGB_EFFECT_WILD:
            default:
                val = pulse_val(now, slow, i * 53, palette_floor(p.val / 16, 4), palette_floor((p.val * 3) / 4, 18));
                val = clamp_add_u8(val, triwave8_period(now, fast + i * 31, i * 43) / 10);
                break;
        }

        set_led_hsv(led, hue, sat, val);
    }
}

static void render_encoder_frame_wander_group(uint8_t layer, hsv_config_t p, uint32_t now) {
    uint32_t elapsed = timer_elapsed32(rgb_frame_wander_started);
    bool active = rgb_frame_wander_started != 0 && elapsed < RGB_FRAME_WANDER_SHOW_MS;

    rgb_rendering_frame = true;

    for (uint8_t i = 0; i < RGB_FRAME_LED_COUNT; i++) {
        uint8_t val = palette_floor(p.val / 28, 2);
        uint8_t hue = p.hue;

        if (active) {
            uint8_t step = (uint8_t)((elapsed / 24) % RGB_FRAME_LED_COUNT);
            uint8_t head = rgb_frame_wander_clockwise ? step : (RGB_FRAME_LED_COUNT - 1 - step);
            uint8_t d = wrap_distance(i, head, RGB_FRAME_LED_COUNT);

            if (d == 0) val = p.val;
            else if (d == 1) val = palette_floor((p.val * 3) / 4, 22);
            else if (d == 2) val = palette_floor(p.val / 2, 14);
            else if (d == 3) val = palette_floor(p.val / 4, 6);
            hue = p.hue + d * 2;
        }

        set_led_hsv(RGB_FRAME_LED_FIRST + i, hue, p.sat, val);
    }

    rgb_rendering_frame = false;

    if (rgb_frame_wander_started != 0 && elapsed >= RGB_FRAME_WANDER_SHOW_MS) {
        rgb_frame_wander_started = 0;
    }
}

static void render_frame_light_group(void) {
    uint8_t layer = canonical_rgb_layer(active_layer_raw());
    rgb_effect_mode_t effect = frame_effect_for_layer(layer);
    hsv_config_t p = frame_palette_for_layer(layer);
    uint32_t now = timer_read32();

    if (RGB_FRAME_LED_COUNT == 0) return;

    if (rgb_animation_mode == RGB_ANIM_LAYER_KEYS || !rgb_output_enabled || !(rgb_zone_mask & RGB_ZONE_FRAME) || effect == RGB_EFFECT_OFF) {
        clear_frame_light();
        return;
    }

    if (rgb_animation_mode == RGB_ANIM_FRAME_WANDER) {
        render_encoder_frame_wander_group(layer, p, now);
        return;
    }

    uint16_t slow = frame_effect_period_for_layer(layer, 2400);
    uint16_t fast = frame_effect_period_for_layer(layer, 900);
    uint8_t head = (now / (fast / RGB_FRAME_LED_COUNT + 1)) % RGB_FRAME_LED_COUNT;
    uint8_t scan = ping_pong_index(now, slow, RGB_FRAME_LED_COUNT, 0);

    rgb_rendering_frame = true;

    for (uint8_t i = 0; i < RGB_FRAME_LED_COUNT; i++) {
        uint8_t hue = p.hue;
        uint8_t sat = p.sat;
        uint8_t val = palette_floor(p.val / 18, 4);
        uint8_t d;

        switch (effect) {
            case RGB_EFFECT_SOLID:
                val = p.val;
                break;

            case RGB_EFFECT_BREATHING:
                val = pulse_val(now, slow, i * 3, palette_floor(p.val / 10, 6), p.val);
                break;

            case RGB_EFFECT_RUNNING:
            case RGB_EFFECT_PACKET:
            case RGB_EFFECT_COMET:
                d = wrap_distance(i, head, RGB_FRAME_LED_COUNT);
                if (d == 0) val = p.val;
                else if (d == 1) val = palette_floor((p.val * 3) / 4, 24);
                else if (d == 2) val = palette_floor(p.val / 2, 16);
                else if (d == 3) val = palette_floor(p.val / 4, 8);
                else val = palette_floor(p.val / 24, 3);
                break;

            case RGB_EFFECT_TWINKLE:
                val = palette_floor(p.val / 20, 4);
                val = clamp_add_u8(val, triwave8_period(now, 1100 + i * 37, i * 19) / 7);
                if ((((now / 170) + i * 11) % 29) == 0) val = p.val;
                break;

            case RGB_EFFECT_PULSE:
            case RGB_EFFECT_BLOOM:
            case RGB_EFFECT_VISUALIZER:
                val = pulse_val(now, slow, i * 7, palette_floor(p.val / 14, 5), p.val);
                val = clamp_add_u8(val, triwave8_period(now, fast + i * 13, i * 17) / 9);
                break;

            case RGB_EFFECT_SCAN:
            case RGB_EFFECT_NAV_BLINK:
            case RGB_EFFECT_SWEEP:
                d = distance_u8(i, scan);
                if (d == 0) val = p.val;
                else if (d == 1) val = palette_floor((p.val * 2) / 3, 18);
                else if (d == 2) val = palette_floor(p.val / 3, 10);
                else val = palette_floor(p.val / 24, 3);
                break;

            case RGB_EFFECT_RAINBOW:
                hue = (uint8_t)((now * 255UL) / frame_effect_period_for_layer(layer, 3400)) + i * 3;
                sat = 255;
                val = pulse_val(now, slow, i * 5, palette_floor(p.val / 5, 16), p.val);
                break;

            case RGB_EFFECT_STACK:
            case RGB_EFFECT_TYPEWRITER:
                {
                    uint16_t period = frame_effect_period_for_layer(layer, 1900);
                    uint8_t filled = ((now % period) * (RGB_FRAME_LED_COUNT + 1)) / period;
                    bool reverse = ((now / period) % 2) != 0;
                    uint8_t pos = reverse ? (RGB_FRAME_LED_COUNT - 1 - i) : i;
                    val = pos < filled ? palette_floor((p.val * 3) / 4, 20) : palette_floor(p.val / 24, 3);
                    if (pos == filled && filled < RGB_FRAME_LED_COUNT) val = p.val;
                }
                break;

            case RGB_EFFECT_PONG:
                d = distance_u8(i, scan);
                if (d == 0) val = p.val;
                else if (d == 1) val = palette_floor((p.val * 3) / 5, 18);
                else val = palette_floor(p.val / 26, 3);
                break;

            case RGB_EFFECT_TILT:
#ifdef ADXL345_ENABLE
                if (adxl345_ready) {
                    uint8_t tilt_head = adxl345_frame_head();
                    d = wrap_distance(i, tilt_head, RGB_FRAME_LED_COUNT);
                    if (d == 0) val = p.val;
                    else if (d == 1) val = palette_floor((p.val * 3) / 4, 22);
                    else if (d == 2) val = palette_floor(p.val / 2, 14);
                    else val = palette_floor(p.val / 28, 3);
                    val = clamp_add_u8(val, adxl345_fluid_motion > 120 ? 28 : (uint8_t)(adxl345_fluid_motion / 6));
                } else
#endif
                {
                    val = pulse_val(now, slow, i * 9, palette_floor(p.val / 12, 6), palette_floor((p.val * 5) / 6, 32));
                }
                break;

            case RGB_EFFECT_WILD:
            default:
                val = pulse_val(now, slow, i * 9, palette_floor(p.val / 12, 6), palette_floor((p.val * 5) / 6, 32));
                val = clamp_add_u8(val, triwave8_period(now, fast + i * 23, i * 31) / 10);
                break;
        }

        set_led_hsv(RGB_FRAME_LED_FIRST + i, hue, sat, val);
    }

    rgb_rendering_frame = false;
}

static void flush_led_frame(void) {
    render_gap_light_group();
    render_frame_light_group();
    rgblight_driver.flush();
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

static void render_effect_nav_blink(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    static const uint8_t seq[] = {1, 6, 3, 8, 11, 13, 6, 1};
    uint16_t period = effect_period_for_layer(layer, 1350);
    uint8_t step = (now / (period / sizeof(seq) + 1)) % sizeof(seq);
    uint8_t active = seq[step];

    for (uint8_t i = 0; i < RGB_KEYFIELD_LED_COUNT; i++) {
        uint8_t val = led_is_gap(i) ? palette_floor(p.val / 20, 4) : palette_floor(p.val / 36, 2);
        if (i == active) {
            val = pulse_val(now, 300, 0, palette_floor((p.val * 2) / 3, 28), p.val);
        } else if (led_is_gap(i) && distance_u8(led_row_index(i), led_row_index(active)) <= 1) {
            val = palette_floor(p.val / 5, 12);
        }
        set_led_hsv(i, p.hue, p.sat, val);
    }

    flush_led_frame();
}

static void render_effect_typewriter(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 2100);
    uint16_t t = now % period;
    uint8_t typed = 0;
    uint16_t cursor = 0;

    for (uint8_t i = 0; i < RGB_KEYFIELD_LED_COUNT; i++) {
        uint16_t beat = 80 + ((i * 37) % 45);
        if (i == 5 || i == 10) beat += 170;
        if (t >= cursor + beat) typed = i + 1;
        cursor += beat;
    }

    if (t > cursor) typed = RGB_KEYFIELD_LED_COUNT;

    for (uint8_t i = 0; i < RGB_KEYFIELD_LED_COUNT; i++) {
        uint8_t val = 0;
        if (i < typed) val = led_is_gap(i) ? palette_floor(p.val / 5, 10) : palette_floor((p.val * 2) / 5, 18);
        if (i == typed && typed < RGB_KEYFIELD_LED_COUNT) val = pulse_val(now, 260, i * 13, palette_floor((p.val * 2) / 3, 26), p.val);
        if (typed >= RGB_KEYFIELD_LED_COUNT && t > cursor + 300) val = scale_val(val, 100);
        set_led_hsv(i, p.hue, p.sat, val);
    }

    flush_led_frame();
}

static void render_effect_visualizer(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);

    for (uint8_t row = 0; row < 3; row++) {
        uint8_t wave_a = triwave8_period(now, effect_period_for_layer(layer, 620 + row * 170), row * 41);
        uint8_t wave_b = triwave8_period(now, effect_period_for_layer(layer, 1030 + row * 210), 170 - row * 37);
        uint8_t meter = (uint8_t)(((uint16_t)((wave_a + wave_b) / 2) * 5) / 255);
        bool kick = ((now / (240 + row * 43)) % 7) == 0;

        for (uint8_t col = 0; col < 5; col++) {
            uint8_t led = row * 5 + col;
            uint8_t val = palette_floor(p.val / 22, 4);
            if (col <= meter) val = palette_floor(scale_val(p.val, 220 - col * 26), 18);
            if (kick && col == 2) val = p.val;
            if (led_is_gap(led)) val = scale_val(val, 150);
            set_led_hsv(led, p.hue, p.sat, val);
        }
    }

    flush_led_frame();
}

static void render_effect_pong(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint8_t x = ping_pong_index(now, effect_period_for_layer(layer, 1450), 5, 0);
    uint8_t y = ping_pong_index(now, effect_period_for_layer(layer, 1050), 3, 96);

    for (uint8_t i = 0; i < RGB_KEYFIELD_LED_COUNT; i++) {
        uint8_t d = distance_u8(led_col_index(i), x) + distance_u8(led_row_index(i), y);
        uint8_t val = palette_floor(p.val / 28, 3);
        if (d == 0) val = p.val;
        else if (d == 1) val = palette_floor((p.val * 3) / 5, 18);
        else if (d == 2) val = palette_floor(p.val / 4, 8);
        if (led_is_gap(i)) val = scale_val(val, 150);
        set_led_hsv(i, p.hue, p.sat, val);
    }

    flush_led_frame();
}

static void render_effect_packet(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 1250);
    uint8_t packet = (now / (period / RGB_KEYFIELD_LED_COUNT + 1)) % RGB_KEYFIELD_LED_COUNT;

    for (uint8_t i = 0; i < RGB_KEYFIELD_LED_COUNT; i++) {
        uint8_t d = wrap_distance(i, packet, RGB_KEYFIELD_LED_COUNT);
        uint8_t val = palette_floor(p.val / 24, 4);
        if (d == 0) val = p.val;
        else if (d == 1) val = palette_floor((p.val * 3) / 5, 18);
        else if (d == 2) val = palette_floor(p.val / 4, 8);
        if (led_is_gap(i)) val = scale_val(val, 165);
        set_led_hsv(i, p.hue, p.sat, val);
    }

    flush_led_frame();
}

static void render_effect_bloom(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint8_t bloom = triwave8_period(now, effect_period_for_layer(layer, 1800), 0);
    uint8_t dot = (now / (effect_period_for_layer(layer, 420) / 3 + 1)) % 3;

    for (uint8_t i = 0; i < RGB_KEYFIELD_LED_COUNT; i++) {
        uint8_t dist = distance_u8(led_row_index(i), 1) + distance_u8(led_col_index(i), 2);
        int16_t energy = (int16_t)bloom - (int16_t)(dist * 42);
        uint8_t val = energy > 0 ? palette_floor((uint8_t)energy, 8) : palette_floor(p.val / 28, 3);
        if ((i == 5 + dot * 2) || (i == 6 + dot * 2)) val = clamp_add_u8(val, p.val / 3);
        set_led_hsv(i, p.hue, p.sat, val);
    }

    flush_led_frame();
}

static void render_effect_sweep(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint8_t row_head = ping_pong_index(now, effect_period_for_layer(layer, 2200), 3, 0);

    for (uint8_t i = 0; i < RGB_KEYFIELD_LED_COUNT; i++) {
        uint8_t d = distance_u8(led_row_index(i), row_head);
        uint8_t val = pulse_val(now, effect_period_for_layer(layer, 2600), led_row_index(i) * 72, palette_floor(p.val / 18, 4), p.val);
        if (d > 0) val = scale_val(val, d == 1 ? 130 : 70);
        val = clamp_add_u8(val, triwave8_period(now, 1600 + i * 43, i * 29) / 11);
        set_led_hsv(i, p.hue, p.sat, val);
    }

    flush_led_frame();
}

static void render_rgb_tilt_mode(void) {
    uint8_t layer = canonical_rgb_layer(active_layer_raw());
    hsv_config_t p = palette_for_layer(layer);
    uint32_t now = timer_read32();

    clear_all_keys();

#ifndef ADXL345_ENABLE
    render_effect_sweep(layer);
    return;
#else
    if (!adxl345_ready) {
        uint8_t pulse = pulse_val(now, effect_period_for_layer(layer, 1800), 0, palette_floor(p.val / 16, 5), palette_floor(p.val / 2, 18));
        set_key_hsv(4, p.hue, p.sat, pulse);
        set_key_hsv(1, p.hue, p.sat, scale_val(pulse, 110));
        set_key_hsv(7, p.hue, p.sat, scale_val(pulse, 110));
        flush_led_frame();
        return;
    }

    // Render from the higher-resolution fluid state so tilt glides continuously.
    int16_t center_x_fp = (int16_t)(2 * 256 + ((int32_t)adxl345_fluid_x_fp * 256) / (110 << ADXL345_FLUID_SHIFT));
    int16_t center_y_fp = (int16_t)(1 * 256 + ((int32_t)adxl345_fluid_y_fp * 256) / (130 << ADXL345_FLUID_SHIFT));
    uint8_t motion_boost = adxl345_fluid_motion > 120 ? 52 : (uint8_t)(adxl345_fluid_motion / 3);

    if (center_x_fp < 0) center_x_fp = 0;
    if (center_x_fp > 4 * 256) center_x_fp = 4 * 256;
    if (center_y_fp < 0) center_y_fp = 0;
    if (center_y_fp > 2 * 256) center_y_fp = 2 * 256;

    for (uint8_t led = 0; led < RGB_KEYFIELD_LED_COUNT; led++) {
        uint8_t row = led_row_index(led);
        uint8_t col = led_col_index(led);
        int16_t led_x_fp = (int16_t)col * 256;
        int16_t led_y_fp = (int16_t)row * 256;
        uint16_t dx = abs_i16_u16(led_x_fp - center_x_fp);
        uint16_t dy = abs_i16_u16(led_y_fp - center_y_fp);
        uint16_t major = dx > dy ? dx : dy;
        uint16_t minor = dx > dy ? dy : dx;
        uint16_t distance_fp = major + minor / 2;
        uint8_t fade = distance_fp >= 768 ? 0 : (uint8_t)(255 - ((distance_fp * 255) / 768));
        uint8_t base = palette_floor(p.val / 24, 3);
        uint8_t peak = clamp_add_u8(p.val, motion_boost);
        uint8_t span = peak > base ? (uint8_t)(peak - base) : 0;
        uint8_t val = clamp_add_u8(base, scale_val(span, fade));

        if (led_is_gap(led)) {
            val = scale_val(val, 150);
            val = clamp_add_u8(val, triwave8_period(now, 900 + led * 31, led * 17) / 12);
        }

        set_led_hsv(led, p.hue, p.sat, val);
    }

    flush_led_frame();
#endif
}

static void render_rgb_layer_visuals(void) {
    uint8_t layer = canonical_rgb_layer(active_layer_raw());
    rgb_effect_mode_t effect = effect_for_layer(layer);

    if (effect == RGB_EFFECT_OFF) {
        clear_all_keys();
        flush_led_frame();
        return;
    }

    if (rgb_animation_mode == RGB_ANIM_TILT_PLACEHOLDER) {
        render_rgb_tilt_mode();
        return;
    }

    if (rgb_animation_mode == RGB_ANIM_LAYER_KEYS) {
        render_rgb_layer_key_mode();
        return;
    }

    if (rgb_animation_mode == RGB_ANIM_REACTIVE) {
        render_rgb_reactive_mode();
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
        case RGB_EFFECT_RAINBOW:    render_effect_rainbow(layer);    return;
        case RGB_EFFECT_STACK:      render_effect_stack(layer);      return;
        case RGB_EFFECT_NAV_BLINK:  render_effect_nav_blink(layer);  return;
        case RGB_EFFECT_TYPEWRITER: render_effect_typewriter(layer); return;
        case RGB_EFFECT_VISUALIZER: render_effect_visualizer(layer); return;
        case RGB_EFFECT_PONG:       render_effect_pong(layer);       return;
        case RGB_EFFECT_PACKET:     render_effect_packet(layer);     return;
        case RGB_EFFECT_BLOOM:      render_effect_bloom(layer);      return;
        case RGB_EFFECT_SWEEP:      render_effect_sweep(layer);      return;
        case RGB_EFFECT_TILT:       render_rgb_tilt_mode();          return;
        case RGB_EFFECT_WILD:
        default:
            break;
    }

    switch (layer) {
        case _BASE:   render_base_wild();   break;
        case _WINDOW: render_window_wild(); break;
        case _TEXT:   render_text_wild();   break;
        case _MEDIA:  render_media_wild();  break;
        case _WORK:   render_effect_solid(_WORK); break;
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
        TD(TD_LAYER_SELECT), KC_HOME,       KC_BSPC,
        KC_LEFT,     LT(_SYS, KC_ENT),      KC_RGHT,
        LCTL(KC_Z),  KC_END,     LCTL(KC_Y)
    ),

    [_WINDOW] = LAYOUT(
        TD(TD_LAYER_SELECT), WIN_BRO, WIN_AUX,
        WIN_1,       WIN_2,   WIN_3,
        WIN_4,       WIN_5,   WIN_6
    ),

    [_TEXT] = LAYOUT(
        TD(TD_LAYER_SELECT), TXT_ACT, TXT_EDT,
        TXT_1,       LT(_SYS, TXT_2),   TXT_3,
        TXT_4,       TXT_5,   TXT_6
    ),

    [_MEDIA] = LAYOUT(
        TD(TD_LAYER_SELECT), KC_MPRV, KC_MNXT,
        KC_VOLD,     KC_MPLY, KC_VOLU,
        KC_MRWD,     KC_MUTE, KC_MFFD
    ),

    [_MEDIA_GAME] = LAYOUT(
        TD(TD_LAYER_SELECT), KC_L,    KC_K,
        KC_I,        KC_U,   KC_H,
        KC_J,        KC_O,   KC_ESC
    ),

    [_RGB] = LAYOUT(
        TD(TD_LAYER_SELECT), RGB_MODE, RGB_TOG,
        RGB_HUEU,    RGB_HUED, RGB_VALU,
        RGB_SATU,    RGB_SATD, RGB_VALD
    ),

    [_RGBMOD] = LAYOUT(
        TD(TD_LAYER_SELECT), KC_TRNS, KC_NO,
        KC_NO,       KC_NO,   KC_NO,
        KC_NO,       KC_NO,   KC_NO
    ),

    [_RGBADJ] = LAYOUT(
        TD(TD_LAYER_SELECT), KC_NO,   KC_TRNS,
        KC_NO,       KC_NO,   KC_NO,
        KC_NO,       KC_NO,   KC_NO
    ),

    [_MARK] = LAYOUT(
        TD(TD_LAYER_SELECT), KC_NO,   KC_NO,
        KC_NO,       KC_NO,   KC_NO,
        KC_NO,       KC_NO,   KC_NO
    ),

    [_WORK] = LAYOUT(
        TD(TD_LAYER_SELECT), SEL_VSC,   WRK_APP,
        WRK_PULL,    WRK_COMMIT, WRK_PUSH,
        WRK_BRANCH,  WRK_PR,     WRK_SYNC
    ),

    [_SYS] = LAYOUT(
        TD(TD_LAYER_SELECT), KC_UP,   KC_PGUP,
        KC_LEFT,     KC_ENT,  KC_RGHT,
        KC_HOME,     KC_DOWN, KC_PGDN
    ),

    [_DEV] = LAYOUT(
        TD(TD_LAYER_SELECT), GM_NAV,  GM_MOUSE,
        GM_1,        GM_2,    GM_3,
        GM_4,        GM_5,    GM_6
    ),

    [_GAME] = LAYOUT(
     TD(TD_LAYER_SELECT), MS_BTN1, MS_BTN2,
     MS_LEFT,             LT(_SYS, MS_UP),   MS_RGHT,
     MS_BTN3,             MS_DOWN, GM_TILT
    ),

    [_VSC] = LAYOUT(
        TD(TD_LAYER_SELECT), VSC_BAR, VSC_CHAT,
        VSC_1,       VSC_2,   VSC_3,
        VSC_4,       VSC_5,   VSC_6
    ),

    [_PROMPT] = LAYOUT(
        TD(TD_LAYER_SELECT), PRM_PICS, PRM_ETSY,
        VSC_1,       VSC_2,   VSC_3,
        VSC_4,       VSC_5,   VSC_6
    ),

    [_SELECT] = LAYOUT(
        TD(TD_LAYER_SELECT), TD(TD_SEL_WIN_MARK), SEL_TEXT,
        TD(TD_SEL_MEDIA_GAME), SEL_WORK, TD(TD_SEL_DEV_GAME),
        TD(TD_SEL_VSC_SYS), SEL_RGB,     SEL_PROMPT
    ),
};

layer_state_t layer_state_set_user(layer_state_t state) {
    static layer_state_t last_state = 0;
    static uint8_t last_active_layer = _BASE;
    bool select_now = layer_state_cmp(state, _SELECT);
    bool select_before = layer_state_cmp(last_state, _SELECT);

    if (select_now && !select_before) {
        layer_state_t without_select = (state | default_layer_state) & ~((layer_state_t)1 << _SELECT);
        uint8_t base = canonical_rgb_layer(get_highest_layer(without_select));

        if (base >= _SELECT) base = _BASE;

        selector_target = base;
        select_cursor = slot_for_layer(selector_target);
    }

    uint8_t active = get_highest_layer(state | default_layer_state);
    if (active != last_active_layer) {
        // Changing top layer should always clear transient sub-modes.
        text_action_held = false;
        text_edit_held = false;
        text_selection_pending_copy = false;
        window_browser_held = false;
        window_snap_held = false;
        vsc_mode = VSC_MODE_NONE;
        rgb_mode_held = false;

        switch (active) {
            case _TEXT:
                text_mode = TEXT_MODE_WIN;
                break;
            case _VSC:
                last_vsc_mode = VSC_MODE_NONE;
                break;
            case _PROMPT:
                prompt_mode = PROMPT_MODE_BASE;
                break;
            case _DEV:
                game_mode = GAME_MODE_NAV;
                break;
            default:
                break;
        }

        last_active_layer = active;
    }

    last_state = state;
    return state;
}

static void select_target_layer(uint8_t layer) {
    selector_target = layer;
    select_cursor = slot_for_layer(selector_target);

    if (!matrix_select_held) {
        layer_move(selector_target);
        selector_last_tap = 0;
    }
}


static void selector_hold_begin(void) {
    selector_origin_layer = canonical_rgb_layer(active_layer_raw());
    if (selector_origin_layer >= _SELECT) selector_origin_layer = _BASE;

    matrix_select_held = true;
    update_select_layer_state();
}

static void selector_hold_end(void) {
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

static uint8_t tap_dance_layer_for_count(uint8_t count) {
    switch (count) {
        case 1: return _BASE;
        case 2: return _WINDOW;
        case 3: return _TEXT;
        case 4: return _MEDIA;
        case 5: return _DEV;
        case 6: return _VSC;
        case 7: return _RGB;
        default: return _PROMPT;
    }
}

#ifdef TAP_DANCE_ENABLE
static void td_select_win_mark_finished(tap_dance_state_t *state, void *user_data) {
    (void)user_data;
    select_target_layer(state->count >= 2 ? _MARK : _WINDOW);
}

static void td_select_media_game_finished(tap_dance_state_t *state, void *user_data) {
    (void)user_data;
    select_target_layer(state->count >= 2 ? _MEDIA_GAME : _MEDIA);
}

static void td_select_dev_game_finished(tap_dance_state_t *state, void *user_data) {
    (void)user_data;
    select_target_layer(state->count >= 2 ? _GAME : _DEV);
}

static void td_select_vsc_sys_finished(tap_dance_state_t *state, void *user_data) {
    (void)user_data;
    select_target_layer(state->count >= 2 ? _SYS : _VSC);
}

static bool td_layer_select_held = false;

static void td_layer_select_finished(tap_dance_state_t *state, void *user_data) {
    (void)user_data;

    if (state->pressed) {
        td_layer_select_held = true;
        selector_hold_begin();
        return;
    }

    select_target_layer(tap_dance_layer_for_count(state->count));
}

static void td_layer_select_reset(tap_dance_state_t *state, void *user_data) {
    (void)state;
    (void)user_data;

    if (td_layer_select_held) {
        td_layer_select_held = false;
        selector_hold_end();
    }
}

tap_dance_action_t tap_dance_actions[] = {
    [TD_LAYER_SELECT] = ACTION_TAP_DANCE_FN_ADVANCED(NULL, td_layer_select_finished, td_layer_select_reset),
    [TD_SEL_WIN_MARK] = ACTION_TAP_DANCE_FN_ADVANCED(NULL, td_select_win_mark_finished, NULL),
    [TD_SEL_MEDIA_GAME] = ACTION_TAP_DANCE_FN_ADVANCED(NULL, td_select_media_game_finished, NULL),
    [TD_SEL_DEV_GAME] = ACTION_TAP_DANCE_FN_ADVANCED(NULL, td_select_dev_game_finished, NULL),
    [TD_SEL_VSC_SYS]  = ACTION_TAP_DANCE_FN_ADVANCED(NULL, td_select_vsc_sys_finished, NULL),

};
#endif

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (keycode == TD(TD_LAYER_SELECT)) {
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
        rgb_reactive_started = timer_read32() | 1;
        rgb_reactive_key_index = (last_row * MATRIX_COLS) + last_col;
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
                    cycle_rgb_animation_mode();
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

        case SEL_WORK:
            if (record->event.pressed) select_target_layer(_WORK);
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

        case GM_MOUSE:
            if (record->event.pressed) game_mode = GAME_MODE_MOUSE;
            return false;

        case GM_TILT:
            if (record->event.pressed) game_tilt_enabled = !game_tilt_enabled;
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
            if (record->event.pressed) {
                window_browser_held = !window_browser_held;
                if (window_browser_held) window_snap_held = false;
            }
            return false;

        case WIN_AUX:
            if (record->event.pressed) {
                window_snap_held = !window_snap_held;
                if (window_snap_held) window_browser_held = false;
            }
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
    if (record->event.pressed) {
        text_edit_held = false;
        text_action_held = !text_action_held;
    }
    return false;

case TXT_EDT:
    if (record->event.pressed) {
        text_action_held = false;
        text_edit_held = !text_edit_held;
    }
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
            } else {
                vsc_mode = VSC_MODE_NONE;
            }
            return false;

        case VSC_CHAT:
            if (record->event.pressed) {
                vsc_mode = VSC_MODE_CHAT;
            } else {
                vsc_mode = VSC_MODE_NONE;
            }
            return false;

        case WRK_APP:
            if (record->event.pressed) {
                send_string("github");
                tap_code(KC_ENT);
            }
            return false;

        case WRK_PULL:
            if (record->event.pressed) {
                send_string("git pull");
                tap_code(KC_ENT);
            }
            return false;

        case WRK_COMMIT:
            if (record->event.pressed) {
                send_string("git add -A && git commit -m \"");
            }
            return false;

        case WRK_PUSH:
            if (record->event.pressed) {
                send_string("git push");
                tap_code(KC_ENT);
            }
            return false;

        case WRK_BRANCH:
            if (record->event.pressed) {
                send_string("git checkout -b ");
            }
            return false;

        case WRK_PR:
            if (record->event.pressed) {
                send_string("gh pr create --web");
                tap_code(KC_ENT);
            }
            return false;

        case WRK_SYNC:
            if (record->event.pressed) {
                send_string("git status");
                tap_code(KC_ENT);
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

        case _MARK:
            tap_code(clockwise ? KC_VOLU : KC_VOLD);
            break;

        case _WINDOW:
            if (current_window_preview_mode() == WINDOW_MODE_BROWSER) {
                tap_code16(clockwise ? C(KC_PGDN) : C(KC_PGUP));
            } else if (current_window_preview_mode() == WINDOW_MODE_SNAP) {
                tap_code16(clockwise ? G(KC_RGHT) : G(KC_LEFT));
            } else if (encoder_btn_pressed) {
                tap_code16(clockwise ? G(C(KC_RGHT)) : G(C(KC_LEFT)));
            } else if (clockwise) {
                tap_code16(A(KC_TAB));
            } else {
                tap_code16(S(A(KC_TAB)));
            }
            break;

        case _WORK:
            tap_code(clockwise ? KC_RGHT : KC_LEFT);
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
            if (rgb_animation_mode == RGB_ANIM_FRAME_WANDER && rgb_mode_held) {
                rgb_frame_wander_clockwise = clockwise;
                rgb_frame_wander_started = timer_read32() | 1;
#ifdef RGBLIGHT_ENABLE
                render_rgb_layer_visuals();
#endif
            } else {
                adjust_layer_brightness(_RGB, clockwise ? RGBLIGHT_VAL_STEP : -RGBLIGHT_VAL_STEP);
            }
            break;

        case _DEV:
            tap_code(clockwise ? MS_WHLU : MS_WHLD);
            break;

        case _GAME:
            tap_code(clockwise ? MS_WHLU : MS_WHLD);
            break;

        case _VSC:
        case _SYS:
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
    static uint32_t oled_toggle_last_tap = 0;
    static uint32_t oled_toggle_pressed_at = 0;
    static bool oled_toggle_hold_handled = false;
#endif

    adxl345_task();
    update_game_tilt_arrows();

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

    if (oled_toggle_pressed && !oled_toggle_was_pressed) {
        oled_toggle_pressed_at = timer_read32() | 1;
        oled_toggle_hold_handled = false;
    }

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

    if (oled_toggle_pressed && !clear_buttons_pressed && !oled_toggle_hold_handled && oled_toggle_pressed_at != 0 && timer_elapsed32(oled_toggle_pressed_at) >= OLED_HOLD_MS) {
        oled_view = OLED_VIEW_LEGEND;
        oled_toggle_hold_handled = true;

#    ifdef VIA_ENABLE
        via_user_config.oled_view = oled_view;
        save_via_config();
#    endif
    }

    if (!oled_toggle_pressed && oled_toggle_was_pressed) {
        if (!oled_toggle_combo_used && !oled_toggle_hold_handled && timer_elapsed32(oled_toggle_last_action) > BUTTON_DEBOUNCE_MS) {
            uint32_t now = timer_read32();

            if (oled_toggle_last_tap != 0 && timer_elapsed32(oled_toggle_last_tap) <= OLED_DOUBLE_TAP_MS) {
                oled_view = OLED_VIEW_HELP;
                oled_toggle_last_tap = 0;
            } else {
                oled_view = (oled_view_t)((oled_view + 1) % OLED_VIEW_ENCODER);
                oled_toggle_last_tap = now;
            }

#    ifdef VIA_ENABLE
            via_user_config.oled_view = oled_view;
            save_via_config();
#    endif

            oled_toggle_last_action = now;
        }

        oled_toggle_pressed_at = 0;
        oled_toggle_hold_handled = false;
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

    adxl345_init();

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
        snprintf(line, sizeof(line), "MODES -> %-8.8s", layer_name_short(selector_target));
    } else if (layer == _PROMPT) {
        snprintf(line, sizeof(line), "PROMPT %-4.4s", prompt_mode_name(prompt_mode));
    } else if (layer == _WORK) {
        snprintf(line, sizeof(line), "GIT");
    } else if (layer == _DEV) {
        snprintf(line, sizeof(line), "GAME %-5.5s", game_mode_name(game_mode));
    } else if (layer == _GAME) {
        snprintf(line, sizeof(line), "GAME+ TLT %s", game_tilt_enabled ? "ON" : "OFF");
    } else if (layer == _WINDOW && window_browser_held) {
        snprintf(line, sizeof(line), "%-10.10s", "WIN BRO");
    } else if (layer == _WINDOW && window_snap_held) {
        snprintf(line, sizeof(line), "%-10.10s", "WIN SNAP");
    } else if (layer == _WINDOW) {
        snprintf(line, sizeof(line), "%-10.10s", "WIN APP");
    } else if (layer == _TEXT && text_action_held) {
        snprintf(line, sizeof(line), "%-10.10s", "TXT ACT");
    } else if (layer == _TEXT && text_edit_held) {
        snprintf(line, sizeof(line), "%-10.10s", "TXT EDIT");
    } else if (layer == _TEXT) {
        snprintf(line, sizeof(line), "%-10.10s", "TXT MOVE");
    } else if (layer == _VSC) {
        if (current_vsc_preview_mode() == VSC_MODE_CHAT) {
            snprintf(line, sizeof(line), "VSC AI");
        } else if (current_vsc_preview_mode() == VSC_MODE_BAR) {
            snprintf(line, sizeof(line), "VSC NAV");
        } else {
            snprintf(line, sizeof(line), "VSC MAIN");
        }
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

    snprintf(buf, sizeof(buf), "STATUS %-6.6s", layer_name_short(last_key_layer));
    write_line(0, buf);

    snprintf(buf, sizeof(buf), "K:%-7.7s %04X", last_key_label_for(), last_keycode);
    write_line(1, buf);

    snprintf(buf, sizeof(buf), "F:%.18s", last_key_function_for());
    write_line(2, buf);

    snprintf(buf, sizeof(buf), "P:%u (%u,%u)", (last_row * 3) + last_col + 1, last_row, last_col);
    write_line(3, buf);
}

static void render_tap_view(uint8_t layer) {
    char line[22];

    if (layer == _SELECT) {
        write_line(0, "MODES");
        write_line(1, "WIN/TXT/MED/GAME 2x");
        // SYS has been repurposed to NAV; update the SELECT help to reflect this.
        write_line(2, "MARK/MED+/GAME+/NAV");
        write_line(3, "GIT RGB VSC PROMPT");
        return;
    }

    if (layer == _MARK) {
        write_line(0, "MARK TAP");
        write_line(1, "WEB APP SHOP");
        write_line(2, "AI  DEV MAIL");
        // SYS has become NAV; update the last row of the MARK help.
        write_line(3, "FILE NAV");
        return;
    }

    if (layer == _WORK) {
        write_line(0, "GIT MODES");
        write_line(1, "PULL COMMIT PUSH");
        write_line(2, "BRCH PR   SYNC");
        write_line(3, "VSC / DESKTOP");
        return;
    }

    if (layer == _SYS) {
        // Provide help for the NAV (formerly SYS) layer.  Show the arrow and
        // paging/home keys available on this layer.
        write_line(0, "NAV TAP");
        write_line(1, "SEL  UP  PGUP");
        write_line(2, "LEFT ENT RGHT");
        write_line(3, "HOME DN  PGDN");
        return;
    }

    snprintf(line, sizeof(line), "%s TAP HELP", layer_name_short(layer));
    write_line(0, line);
    write_line(1, "SEL: hold selector");
    write_line(2, "WIN/TXT/MED/GAME+");
    write_line(3, "2x hidden layer");
}

static void render_rgb_help_view(void) {
    char line[22];

    write_line(0, "RGB");
    snprintf(line, sizeof(line), "AN:%-5.5s %s", rgb_animation_label(), adxl345_ready ? "OK" : "NO");
    write_line(1, line);
#ifdef ADXL345_ENABLE
    if (rgb_tilt_visual_active() && adxl345_ready) {
        snprintf(line, sizeof(line), "X%+5d Y%+5d", adxl345_fluid_x, adxl345_fluid_y);
        write_line(2, line);
        snprintf(line, sizeof(line), "Z%+5d M%3u", adxl345_z, adxl345_fluid_motion);
        write_line(3, line);
        return;
    }
#endif
    write_line(2, "ZONE+ALL/FRME/KEY");
    write_line(3, "ENC:VAL ZONE+ENC=WALK");
}

static MAYBE_UNUSED void render_help_view(uint8_t layer) {
    char line[22];

    snprintf(line, sizeof(line), "STATUS %-6.6s", layer_name_short(layer));
    write_line(0, line);
    write_line(1, "GP12: TAP next page");
    write_line(2, "GP12: HOLD -> KEYS");
    write_line(3, "GP12: 2x -> STATUS");
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
        if (oled_view == OLED_VIEW_TAP) {
            render_tap_view(_SELECT);
        } else if (oled_view == OLED_VIEW_RGB_PAGE) {
            render_rgb_help_view();
        } else if (oled_view == OLED_VIEW_HELP) {
            render_last_key_view();
        } else {
            render_legend_view(_SELECT);
        }
    } else if (oled_view == OLED_VIEW_TAP) {
        render_tap_view(layer);
    } else if (oled_view == OLED_VIEW_RGB_PAGE) {
        render_rgb_help_view();
    } else if (oled_view == OLED_VIEW_HELP) {
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

    if (layer == _SELECT) {
        snprintf(line, sizeof(line), "MODES");
    } else if (layer == _PROMPT) {
        snprintf(line, sizeof(line), "PROMPT %-4.4s", prompt_mode_name(prompt_mode));
    } else if (layer == _WORK) {
        snprintf(line, sizeof(line), "GIT");
    } else if (layer == _GAME) {
        snprintf(line, sizeof(line), "GAME+ TLT %s", game_tilt_enabled ? "ON" : "OFF");
    } else if (layer == _WINDOW && window_browser_held) {
        snprintf(line, sizeof(line), "WIN BRO");
    } else if (layer == _WINDOW && window_snap_held) {
        snprintf(line, sizeof(line), "WIN SNAP");
    } else if (layer == _WINDOW) {
        snprintf(line, sizeof(line), "WIN APP");
    } else if (layer == _TEXT && text_action_held) {
        snprintf(line, sizeof(line), "TXT ACT");
    } else if (layer == _TEXT && text_edit_held) {
        snprintf(line, sizeof(line), "TXT EDIT");
    } else if (layer == _TEXT) {
        snprintf(line, sizeof(line), "TXT MOVE");
    } else if (layer == _VSC) {
        if (current_vsc_preview_mode() == VSC_MODE_CHAT) {
            snprintf(line, sizeof(line), "VSC AI");
        } else if (current_vsc_preview_mode() == VSC_MODE_BAR) {
            snprintf(line, sizeof(line), "VSC NAV");
        } else {
            snprintf(line, sizeof(line), "VSC MAIN");
        }
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
        const char *mode = current_vsc_preview_mode() == VSC_MODE_CHAT ? "AI" : (current_vsc_preview_mode() == VSC_MODE_BAR ? "NAV" : "MAIN");
        snprintf(line, sizeof(line), "VSC %s%s", mode, vsc_mode != VSC_MODE_NONE ? " [HELD]" : "");
    } else if (layer == _TEXT) {
        const char *mode = text_action_held ? "ACT" : (text_edit_held ? "EDIT" : "MOVE");
        snprintf(line, sizeof(line), "TXT %s%s", mode, (text_action_held || text_edit_held) ? " [HELD]" : "");
    } else if (layer == _WINDOW) {
        const char *mode = window_browser_held ? "BRO" : (window_snap_held ? "SNAP" : "APP");
        snprintf(line, sizeof(line), "WIN %s%s", mode, (window_browser_held || window_snap_held) ? " [ON]" : "");
    } else if (layer == _GAME) {
        snprintf(line, sizeof(line), "GAME+ TILT [%s]", game_tilt_enabled ? "ON" : "OFF");
    } else {
#ifdef OLED_TOGGLE_BTN_PIN
        snprintf(line, sizeof(line), "GP12: KEYS/MODES/RGB/STATUS");
#else
        snprintf(line, sizeof(line), "Hold SEL for grid");
#endif
    }

    write_line(7, line);
}

static void render_last_key_view(void) {
    char buf[22];

    render_header(last_key_layer);
    write_line(1, "STATUS");

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
    write_line(7, "GP12: 2x for STATUS");
#else
    write_line(7, "Legend details");
#endif
}

static void render_tap_view(uint8_t layer) {
    char line[22];

    if (layer == _SELECT) {
        write_line(0, "MODES");
        write_line(1, "1x WIN/TXT/MED/GAME");
        // Update the second line: SYS has become NAV
        write_line(2, "2x MARK/MED+/GAME+");
        write_line(3, "VSC->NAV / GIT RGB");
        write_line(4, "PROMPT on bottom");
        write_line(5, "Release SEL to go");
        write_line(6, "");
        write_line(7, "GP12: next page");
        return;
    }

    if (layer == _MARK) {
        write_line(0, "MARK / BOOKMARKS");
        write_line(1, "SEL  WEB  APP");
        write_line(2, "SHOP AI   DEV");
        // Reflect that SYS is now NAV in the Mark layer help
        write_line(3, "MAIL FILE NAV");
        write_line(4, "Tap actions TBD");
        write_line(5, "1x / 2x / Hold");
        write_line(6, "");
        write_line(7, "SEL: selector");
        return;
    }

    if (layer == _WORK) {
        write_line(0, "GIT / WORK");
        write_line(1, "SEL  VSC  DESK");
        write_line(2, "PULL COMMIT PUSH");
        write_line(3, "BRCH PR   SYNC");
        write_line(4, "Safe git macros");
        write_line(5, "");
        write_line(6, "");
        write_line(7, "SEL: selector");
        return;
    }

    if (layer == _SYS) {
        // Provide help for the NAV (formerly SYS) layer in the non-OLED build.
        write_line(0, "NAV / ARROWS");
        write_line(1, "SEL  UP  PGUP");
        write_line(2, "LEFT ENT RGHT");
        write_line(3, "HOME DN  PGDN");
        write_line(4, "");
        write_line(5, "");
        write_line(6, "");
        write_line(7, "SEL: selector");
        return;
    }

    snprintf(line, sizeof(line), "%s TAP HELP", layer_name_long(layer));
    write_line(0, line);
    write_line(1, "SELECT hidden:");
    write_line(2, "WIN 2x = MARK");
    write_line(3, "TXT 2x = WORK");
    write_line(4, "MED 2x = MED+");
    write_line(5, "GAME2x=GAME+ NAV");
    // SYS has been repurposed to NAV; adjust this description accordingly.
    write_line(6, "1x normal layer");
    write_line(7, "GP12: next page");
}

static void render_rgb_help_view(void) {
    char line[22];

    write_line(0, "RGB");
    snprintf(line, sizeof(line), "ANIM: %s", rgb_animation_label());
    write_line(1, line);
    write_line(2, adxl345_status_label());
#ifdef ADXL345_ENABLE
    if (rgb_tilt_visual_active() && adxl345_ready) {
        snprintf(line, sizeof(line), "X:%5d Y:%5d", adxl345_fluid_x, adxl345_fluid_y);
        write_line(3, line);
        snprintf(line, sizeof(line), "Z:%5d M:%3u", adxl345_z, adxl345_fluid_motion);
        write_line(4, line);
        write_line(5, "Tilt drives RGB");
        write_line(6, "ZONE still works");
        write_line(7, "GP12: next page");
        return;
    }
#endif
    write_line(3, "ZONE: ALL FRME");
    write_line(4, "KEY GAP");
    write_line(5, "Normal: HUE SAT");
    write_line(6, "VAL / WALK enc");
    write_line(7, "GP12: next page");
}

static MAYBE_UNUSED void render_help_view(uint8_t layer) {
    char line[22];

    snprintf(line, sizeof(line), "STATUS %s", layer_name_long(layer));
    write_line(0, line);
    write_line(1, "GP12 tap: next view");
    write_line(2, "GP12 hold: KEYS");
    write_line(3, "GP12 2x: STATUS");
    write_line(4, "SEL hold: selector");
    write_line(5, "WIN/TXT/VSC 2x hidden");
    write_line(6, "ENC hold: encoder");
    write_line(7, "Combo: clear EEPROM");
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
        if (oled_view == OLED_VIEW_TAP) {
            render_tap_view(_SELECT);
        } else if (oled_view == OLED_VIEW_RGB_PAGE) {
            render_rgb_help_view();
        } else if (oled_view == OLED_VIEW_HELP) {
            render_last_key_view();
        } else {
            render_legend_view(_SELECT);
        }
    } else if (oled_view == OLED_VIEW_TAP) {
        render_tap_view(layer);
    } else if (oled_view == OLED_VIEW_RGB_PAGE) {
        render_rgb_help_view();
    } else if (oled_view == OLED_VIEW_HELP) {
        render_last_key_view();
    } else {
        render_legend_view(layer);
    }

    return false;
}
#endif
