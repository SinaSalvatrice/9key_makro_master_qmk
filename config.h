#pragma once

#define ENCODER_BTN_PIN GP10
#define OLED_TOGGLE_BTN_PIN GP12

// Milliseconds of inactivity before the display switches to eyes mode.
#define OLED_EYES_TIMEOUT_MS 20000

// Selector is the first matrix key (top-left / row 0, col 0), not a dedicated GPIO pin.
#define SELECTOR_MATRIX_ROW 0
#define SELECTOR_MATRIX_COL 0

// Milliseconds of inactivity before sending system sleep (5 minutes)
#define IDLE_SLEEP_TIMEOUT_MS 300000

#define RGBLIGHT_LIMIT_VAL 160
#define RGBLIGHT_DEFAULT_HUE 128
#define RGBLIGHT_DEFAULT_SAT 255
#define RGBLIGHT_DEFAULT_VAL 96
#define RGBLIGHT_DEFAULT_SPD 128

// Keep ADXL345 tilt mouse movement responsive without the large default mousekey startup delay
// or the aggressive acceleration that makes cursor movement feel jumpy.
#define MOUSEKEY_DELAY 0
#define MOUSEKEY_INTERVAL 20
#define MOUSEKEY_MOVE_DELTA 2
#define MOUSEKEY_MAX_SPEED 3
#define MOUSEKEY_TIME_TO_MAX 12

#define VIA_EEPROM_CUSTOM_CONFIG_SIZE 192
#define VIA_FIRMWARE_VERSION 0x0000000B
#define ONBOARD_LED_PIN GP13
#define DYNAMIC_KEYMAP_LAYER_COUNT 17

// ADXL345 stays on I2C0.
#define I2C_DRIVER I2CD0
#define I2C1_SDA_PIN GP0
#define I2C1_SCL_PIN GP1
#define I2C1_CLOCK_SPEED 400000

// ST7789V 1.14" TFT on SPI1.
// GP14/GP15 are free in the current 9-key wiring; GP26/27/29 are control lines.
// MISO is not used by the display but QMK's SPI driver expects a valid SPI1 RX pin.
#define SPI_DRIVER SPID1
#define SPI_SCK_PIN GP14
#define SPI_MOSI_PIN GP15
#define SPI_MISO_PIN GP28

#define ST7789_CS_PIN GP26
#define ST7789_DC_PIN GP27
#define ST7789_RST_PIN GP29
#define ST7789_PANEL_WIDTH 135
#define ST7789_PANEL_HEIGHT 240
#define ST7789_OFFSET_X 40
#define ST7789_OFFSET_Y 53
#define ST7789_SPI_DIVISOR 4
#define ST7789_SPI_MODE 0

#define QUANTUM_PAINTER_DISPLAY_TIMEOUT 0
#define ST7789_NUM_DEVICES 1

// Compatibility selector for the existing four-line display layout in keymap.c.
// This no longer means an SSD1306 is attached; the text is rendered on the TFT shim.
#define OLED_DISPLAY_128X32

// Emergency only: uncomment, flash once, boot once, comment again, flash again.
// #define FORCE_EEPROM_RESET_ON_BOOT
