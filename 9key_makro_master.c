#include "9key_makro_master.h"
#include "quantum.h"

#ifdef QUANTUM_PAINTER_ENABLE
#    include "qp.h"
#    include <string.h>

// Reuse QMK's bundled compact QFF font. It is small enough for the old
// 21-character OLED lines while remaining readable on the 240x135 TFT.
#    include "../boardsource/equals/graphics/thintel15.qff.c"

#define ST7789_LEGACY_COLS 21
#define ST7789_LEGACY_ROWS 4
#define ST7789_TEXT_X      8
#define ST7789_TEXT_Y      8
#define ST7789_COL_STEP    6
#define ST7789_ROW_STEP    30
#define ST7789_TASK_MS     75

static painter_device_t      st7789_display;
static painter_font_handle_t st7789_font;
static bool                  st7789_ready;
static uint8_t               st7789_cursor_col;
static uint8_t               st7789_cursor_row;
static uint32_t              st7789_task_timer;

static uint16_t st7789_text_y(uint8_t row) {
    return ST7789_TEXT_Y + ((uint16_t)row * ST7789_ROW_STEP);
}

static void st7789_clear_row(uint8_t row) {
    if (!st7789_ready || row >= ST7789_LEGACY_ROWS) {
        return;
    }

    uint16_t y = st7789_text_y(row);
    uint16_t h = st7789_font ? st7789_font->line_height : 16;
    uint16_t bottom = y + h + 2;
    uint16_t max_y = qp_get_height(st7789_display) - 1;

    if (bottom > max_y) {
        bottom = max_y;
    }

    qp_rect(st7789_display, 0, y > 1 ? y - 2 : 0, qp_get_width(st7789_display) - 1, bottom, 0, 0, 0, true);
}

void oled_set_cursor(uint8_t col, uint8_t line) {
    st7789_cursor_col = col < ST7789_LEGACY_COLS ? col : (ST7789_LEGACY_COLS - 1);
    st7789_cursor_row = line < ST7789_LEGACY_ROWS ? line : (ST7789_LEGACY_ROWS - 1);
}

void oled_write(const char *data, bool invert) {
    if (!st7789_ready || st7789_font == NULL || data == NULL) {
        return;
    }

    if (st7789_cursor_col == 0) {
        st7789_clear_row(st7789_cursor_row);
    }

    uint16_t x = ST7789_TEXT_X + ((uint16_t)st7789_cursor_col * ST7789_COL_STEP);
    uint16_t y = st7789_text_y(st7789_cursor_row);

    if (invert) {
        qp_drawtext_recolor(st7789_display, x, y, st7789_font, data, 0, 0, 0, 0, 0, 255);
    } else {
        qp_drawtext_recolor(st7789_display, x, y, st7789_font, data, 0, 0, 255, 0, 0, 0);
    }

    size_t len = strlen(data);
    uint16_t next_col = (uint16_t)st7789_cursor_col + len;
    st7789_cursor_col = next_col < ST7789_LEGACY_COLS ? (uint8_t)next_col : (ST7789_LEGACY_COLS - 1);
}

void oled_write_P(const char *data, bool invert) {
    oled_write(data, invert);
}

void oled_write_char(const char data, bool invert) {
    char text[2] = {data, '\0'};
    oled_write(text, invert);
}

void oled_write_ln(const char *data, bool invert) {
    oled_write(data, invert);
    st7789_cursor_col = 0;
    if (st7789_cursor_row + 1 < ST7789_LEGACY_ROWS) {
        st7789_cursor_row++;
    }
}

void oled_write_ln_P(const char *data, bool invert) {
    oled_write_ln(data, invert);
}

void oled_clear(void) {
    if (!st7789_ready) {
        return;
    }

    qp_rect(st7789_display, 0, 0, qp_get_width(st7789_display) - 1, qp_get_height(st7789_display) - 1, 0, 0, 0, true);
    st7789_cursor_col = 0;
    st7789_cursor_row = 0;
}

static void st7789_init(void) {
    st7789_display = qp_st7789_make_spi_device(
        ST7789_PANEL_WIDTH,
        ST7789_PANEL_HEIGHT,
        ST7789_CS_PIN,
        ST7789_DC_PIN,
        ST7789_RST_PIN,
        ST7789_SPI_DIVISOR,
        ST7789_SPI_MODE
    );

    if (st7789_display == NULL) {
        return;
    }

    if (!qp_init(st7789_display, QP_ROTATION_90)) {
        return;
    }

    qp_set_viewport_offsets(st7789_display, ST7789_OFFSET_X, ST7789_OFFSET_Y);
    qp_power(st7789_display, true);

    st7789_font = qp_load_font_mem(font_thintel15);
    if (st7789_font == NULL) {
        return;
    }

    st7789_ready = true;
    oled_clear();
    qp_flush(st7789_display);
    st7789_task_timer = timer_read32();
}

// The active reworked keymap provides a strong implementation. The weak
// fallback keeps other keymaps buildable when Quantum Painter is enabled.
__attribute__((weak)) bool oled_task_user(void) {
    return false;
}

void keyboard_post_init_kb(void) {
    st7789_init();
    keyboard_post_init_user();
}

void housekeeping_task_kb(void) {
    if (st7789_ready && timer_elapsed32(st7789_task_timer) >= ST7789_TASK_MS) {
        st7789_task_timer = timer_read32();
        oled_task_user();
        qp_flush(st7789_display);
    }

    housekeeping_task_user();
}

#endif // QUANTUM_PAINTER_ENABLE
