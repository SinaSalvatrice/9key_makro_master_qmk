# 9key_makro_master - Project Notes

## Current Truth

### Overall status
- `keymaps/reworked` is the real source of truth and currently the **best version so far**.
- The core workflow already feels very good: selector, OLED, RGB, encoder behavior, and the `VSC` layer all work together cleanly.
- Right now there are **no blocking issues being tracked** in this notes file — only polish ideas and future suggestions.

### Hardware / setup
- RP2040 / QMK macropad
- 3x3 key matrix
- 9 RGB LEDs (`RGBLIGHT_LED_COUNT 9`)
- SSD1306 OLED on I2C
- Rotary encoder
- Top-left key (`SEL`) is the practical main layer-selector entry
- Optional extra selector/OLED toggle path via `GP12` still exists in config and code

---

## Current behavior

### Layers in use
- `BASE`
- `WINDOW`
- `TEXT` (`TXT`)
- `MEDIA`
- `DEV`
- `VSC`
- `RGB`
- `SELECT`

### Selector flow
- Key 1 on the main layers is `MO(_SELECT)` and acts as the main selector button.
- Holding `SEL` opens the `SELECT` grid.
- Releasing `SEL` moves to the currently highlighted target layer.
- Turning the encoder while in `SELECT` cycles through the available layer slots.
- The selector correctly remembers the current layer when entering the grid instead of always snapping back to `BASE`.

### Encoder behavior by layer
- `BASE` → mouse wheel up/down
- `WINDOW` → previous / next window (`Alt+Tab` style)
- `TXT` → cursor left / right
- `MEDIA` → volume down / up
- `RGB` → brightness down / up
- `DEV` → mouse wheel up/down
- `VSC` → `Ctrl+PgUp` / `Ctrl+PgDn`
- `SELECT` → move through layer targets

### OLED
- Main OLED view shows a 3x3 legend for the active layer.
- Last-key view shows layer, key label, keycode, function, and matrix position.
- While `SELECT` is active, the OLED intentionally forces the selector legend/grid view.
- On the `VSC` layer, the OLED preview changes depending on whether `BAR` or `CHAT` is the current held/last-used mode.

### RGB
- RGB is handled explicitly per key rather than relying only on stock global effects.
- Layer visuals and selector visuals are timer-driven.
- The center key in the selector grid toggles the FX profile (`RGB_PROFILE`).
- Overall feel is already strong; this is now mostly a polish/taste area.

### VSC layer
- `VSC` is an extra working layer, not a replacement for `DEV`.
- Top row remains `SEL`, `BAR`, `CHAT`.
- Lower six keys (`VSC_1` … `VSC_6`) trigger shared actions depending on the active `BAR` or `CHAT` mode.
- `BAR` strings and `CHAT` texts are already centralized in one editable block in the keymap, which is good and should stay that way.

---

## Suggestions only

### TXT layer encoder idea
- Add an enhanced text-selection behavior for `TXT`:
  - **hold button + encoder twist** should select text instead of only moving the cursor.
  - Easiest first version: **character selection** via `Shift+Left` / `Shift+Right`.
  - Nice later option: **word selection** via `Ctrl+Shift+Left` / `Ctrl+Shift+Right`, if that feels reliable on the target host.

### Nice-to-have polish
1. Tune RGB colors / animation intensity only by feel, not because of any current problem.
2. Personalize the default `CHAT` prompts if wanted.
3. Adjust any `BAR` command names only if a specific host/extension setup needs it.
4. Decide later whether the optional `GP12` path should remain as an OLED-view toggle or eventually be retired.

---

## Assumptions / direction
- Keep `keymaps/reworked` as the canonical implementation target.
- Keep behavior lightweight, direct, and non-blocking.
- Prefer clear per-layer behavior over clever but hidden complexity.
- Treat future work as refinement, not rescue.

## Shortforms
- layer legend for active layer (`btn`) = `LL`
- last keycode pressed (`btn`, toggle) = `LK`
- selector legend (momentary selector) = `SL`



## to do 
- verify RGB pixel position under the keys in layer mod.
 
- base - 0/0 
- win - 0/1 
- txt - 0/2

- med - 1/0
- Fx - 1/1 no pixel because of switching this exactly behavior, good 👍 
- dev - 1/2
- VSC - 2/0
- RGB - 2/1 

- Extend TXT layer.
- the first two availale keys are modifiers.
- The rest on no kombo pressed are defined as followed: home, up, end, left, down, right
- first (_ACT) toggles actions like select all, copy, paste, cut, undo, redo
- second (_EDT) toggles textedit keys such as enter, backspace, space, tab, shift and mouse key 1.
- IMPORTANT: enc A+B go through the text. the encoder btn should transform this movement into a selection of these characters. 

- Extend Windows layer. 
- the first two availale keys are modifiers
- the first (BRO) activates browser control, whatever that may include, surprise me.
- the second one comes later
