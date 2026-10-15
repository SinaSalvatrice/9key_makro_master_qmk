#pragma once

#include <stdbool.h>
#include <stdint.h>

// Compatibility surface used by the existing display code in keymap.c.
// For the active reworked keymap these functions are implemented by the
// ST7789V/Quantum Painter bridge in 9key_makro_master.c.
void oled_set_cursor(uint8_t col, uint8_t line);
void oled_write(const char *data, bool invert);
void oled_write_P(const char *data, bool invert);
void oled_write_ln(const char *data, bool invert);
void oled_write_ln_P(const char *data, bool invert);
void oled_write_char(const char data, bool invert);
void oled_clear(void);
