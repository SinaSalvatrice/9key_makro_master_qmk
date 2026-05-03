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
- Dedicated OLED toggle path now uses `GP11`

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
- `PRMPT`
- `SELECT`

### Reworked keymap outline
- File role: `keymaps/reworked/keymap.c` is the canonical firmware implementation and owns layers, OLED, RGB, selector flow, encoder behavior, and VIA custom config.
- Top of file: compile-time constants, layer enum, custom keycodes, runtime state, selector slot metadata, and centralized labels/functions for OLED and host actions.
- Middle of file: helper functions for layer naming, OLED text lookup, VIA config load/save, selector cursor syncing, RGB palette/effect rendering, and host-action dispatch.
- Key behavior section: layer keymaps, selector target changes, prompt/VSC dispatch, per-layer encoder actions, and matrix scanning for GP11 OLED toggle plus encoder-button combos.
- Display section: boot screen, legend view, last-key view, encoder-help view, and the final OLED task switch that chooses which screen to show.
- Layer model:
- `BASE` is the default navigation layer.
- `WINDOW`, `TEXT`, `MEDIA`, `DEV`, `VSC`, and `RGB` are work layers with layer-specific encoder actions.
- `PROMPT` is a dedicated prompt launcher layer that reuses the six Copilot chat prompt strings.
- `SELECT` is the transient selector grid and FX toggle surface.
- Selector model:
- Hold `SEL` to open `SELECT`.
- Release `SEL` to move to the current selector target.
- Tap `SEL` twice without changing the selector target to jump back to `BASE`.
- The last selector slot now opens `PROMPT` instead of clearing EEPROM.
- Dedicated buttons:
- `GP11` toggles OLED legend/last-key view on tap.
- Encoder button + `GP11` held for the configured timeout clears EEPROM and resets the board.

### Selector flow
- Key 1 on the main layers is `MO(_SELECT)` and acts as the main selector button.
- Holding `SEL` opens the `SELECT` grid.
- Releasing `SEL` moves to the currently highlighted target layer.
- Turning the encoder while in `SELECT` cycles through the available layer slots.
- The selector correctly remembers the current layer when entering the grid instead of always snapping back to `BASE`.
- Double-tapping `SEL` without changing target returns to `BASE`.

### Encoder behavior by layer
- `BASE` → mouse wheel up/down
- `WINDOW` → previous / next window (`Alt+Tab` style) , with btn held down previous / next desktop
- `TXT` → cursor left / right , with btn held down, select text, further push copies text
- `MEDIA` → volume down / up , with btn held down, next track, previous track
- `RGB` → brightness down / up , with btn held down cycle throug modi
- `DEV` → mouse wheel up/down 
- `VSC` → up / down , with btn held down `Ctrl+PgUp` / `Ctrl+PgDn` ,
- `SELECT` → move through layer targets 

### OLED
- Main OLED view shows a 3x3 legend for the active layer.
- Last-key view shows layer, key label, keycode, function, and matrix position.
- While `SELECT` is active, the OLED intentionally forces the selector legend/grid view.
- On the `VSC` layer, the OLED preview changes depending on whether `BAR` or `CHAT` is the current held/last-used mode.
- `GP11` toggles between the main legend view and the last-key view.

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

### PROMPT layer
- `PRMPT` is a direct launcher for the six Copilot chat prompts already stored in `vsc_chat_macros`.
- Top row is `SEL`, `KC_NO`, `KC_NO`.
- Lower six keys send summarize, review, suggest-fix, test, explain, and commit-message prompts after focusing the Copilot Chat view.

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
4. Decide later whether `PROMPT` should keep reusing the current six chat prompts or split into its own prompt set.

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
