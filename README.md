RP2040 zero 
VIA compatible

QMK firmware for a 3×3 macropad with rotary encoder, 10 RGB LEDs, SSD1306 OLED and VIA support.
Rows: 2, 3, 4
Cols: 5, 6, 7
enc a & b: 8, 9
btn: 10
rgb: 13 (9 pixels)
## Quick start



mapping

    [_BASE] = LAYOUT(
        LGUI(KC_TAB),  KC_UP,    LALT(KC_TAB),
        KC_LEFT,       KC_ENT,   KC_RGHT,
        LCTL(KC_Z),    KC_DOWN,  LCTL(KC_R)
    ),
    [_WIN] = LAYOUT(
        LGUI(KC_TAB),  KC_UP,    LALT(KC_TAB),
        KC_LEFT,       KC_ENT,   KC_RGHT,
        LCTL(KC_Z),    KC_DOWN,  LCTL(KC_R)
    ),

    [_EDIT] = LAYOUT(
        LCTL(KC_A),         LCTL(KC_C),   LCTL(KC_V),
        LCTL(KC_X),         LCTL(KC_ENT), KC_NO,
        LCTL(LSFT(KC_Z)),   KC_SPC,       KC_BSPC
    ),
    [_MEDIA] = LAYOUT(
        KC_MPRV,  KC_MSEL,  KC_MNXT,
        KC_MRWD,  KC_MPLY,  KC_MFFD,
        KC_DOWN,  KC_MSTP,  KC_UP
    ),
    [_FN] = LAYOUT(
        KC_F13,  KC_F14,  KC_F15,
        KC_F16,  KC_F17,  KC_F18,
        KC_F19,  KC_F20,  KC_F21
    ),
    [_RGB] = LAYOUT(
        UG_SPDU,  UG_SPDD,  UG_TOGG,
        UG_HUEU,  UG_HUED,  UG_VALU,
        UG_SATU,  UG_SATD,  UG_VALD
    ),
    [_SELECT] = LAYOUT(
        TO(1),  TO(2),  TO(3),
        TO(4),  TO(0),  KC_NO,
        KC_NO,  KC_NO,  KC_NO
    ),




Encoder: btn + Encoder a or be should switch through layers. the single Encoder turn Action is dependant from the layer. 
the rgb Color is dependant on the layer. BTN IS ALWAYS LAYER SELECTOR!

BASE : wandering rainbow breathing, 
enc a+b = mousewheel 
btn = MO layer selector. 
when Encoder is turned while MO layerselector is active, layer goes to next or previous layer. this is the same on ALL layers!  


EDIT :  wandering green/yellow breathing
enc a+b = select next sign/select prevoius sign

WIN : wandering blue/turqoise breathing
enc a+b = next Window (Windows)

MEDIA :  wandering pink/yellow breathing
enc a+b = VOL up and down

FN : wandering orange/yellow  breathing
enc a+b = arrow left and right

RGB :  wandering Purple/orange breathing
enc a+b = rgb brightness

SELECT :  Little faster breathing, more like blinking once, once, Little break, once, once and so on. red



OLED

Oled is integrated. while layer selector, it Shows a Little 3x3 table with the Name of the layer on its Position. beneath it, is the Matrix coordinate written.



in General the Display Shows the active layer as head. beneath it, it Shows the pushed button, in braces the kecode, and beneath the coordinate. 






