#pragma once

#define ENCODER_BTN_PIN GP10
#define OLED_TOGGLE_BTN_PIN GP12

// Milliseconds of inactivity before OLED switches to eyes mode
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
#define DYNAMIC_KEYMAP_LAYER_COUNT 16
#define I2C_DRIVER I2CD0
#define I2C1_SDA_PIN GP0
#define I2C1_SCL_PIN GP1
#define I2C1_CLOCK_SPEED 400000
#define OLED_DISPLAY_128X32
#define OLED_IC OLED_IC_SSD1306
#define OLED_DISPLAY_ADDRESS 0x3C
#define OLED_TIMEOUT 0

// Emergency only: uncomment, flash once, boot once, comment again, flash again.
// #define FORCE_EEPROM_RESET_ON_BOOT
