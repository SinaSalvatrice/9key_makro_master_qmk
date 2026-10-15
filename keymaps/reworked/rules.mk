DYNAMIC_KEYMAP_ENABLE = yes
DYNAMIC_KEYMAP_MACRO_ENABLE = yes
VIA_ENABLE = yes

# ST7789V 1.14" TFT via SPI1 / Quantum Painter.
# The old SSD1306 driver is intentionally disabled; keymap.c keeps its
# existing four-line display view logic and writes it through the TFT shim.
OLED_ENABLE = no
QUANTUM_PAINTER_ENABLE = yes
QUANTUM_PAINTER_DRIVERS += st7789_spi

# I2C stays enabled for the ADXL345 accelerometer.
I2C_ENABLE = yes
I2C_DRIVER_REQUIRED = yes

RGBLIGHT_ENABLE = yes
MOUSEKEY_ENABLE = yes
EXTRAKEY_ENABLE = yes
ENCODER_ENABLE = yes
CONSOLE_ENABLE = yes
KEYCODE_STRING_ENABLE = yes
KEYMAP_INTROSPECTION_ENABLE = no
COMBO_ENABLE = no
TAP_DANCE_ENABLE = yes
