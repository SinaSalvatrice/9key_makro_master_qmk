# 9key_makro_master - Project Notes

## Current Truth

### Overall status
- `keymaps/reworked` is the real source of truth and currently the **best version so far**.
- The core workflow already feels very good: selector, OLED, RGB, encoder behavior, and the `VSC` layer all work together cleanly.
- There are still a couple of unresolved hardware/behavior mismatches in the current build that need a focused debug pass.

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
- Important current issue: a non-selector key still appears to be able to activate `SELECT` on hardware even after code-side guards were added.

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
- The 128x32 OLED path was repaired and now has its own compact render flow.
- Important current issue: the old stray `WIN` text on `TXT` was removed in code, but the user reported no visible change on hardware after flashing attempts.

### RGB
- RGB is handled explicitly per key rather than relying only on stock global effects.
- Layer visuals and selector visuals are timer-driven.
- The center key in the selector grid toggles the FX profile (`RGB_PROFILE`).
- Overall feel is already strong; this is now mostly a polish/taste area.
- In `FX:Q` mode, the active `MEDIA` and `DEV` indicator positions were intentionally swapped.

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

## Current Problems To Continue Later

### Unresolved hardware-behavior mismatch
- User reports that some recent code fixes produced **no visible change on hardware**.
- This means the remaining problem is likely **not** a simple keycode-table mistake.
- Most likely causes now:
  - the flashed firmware is not the edited `keymaps/reworked` build,
  - the physical matrix mapping does not match the assumed row/col layout,
  - or a key is electrically reporting a different matrix position than expected.

### Specifically still broken
- `r0c2` / the physical key the user means by that still seems to activate selector on hardware.
- `TXT` still showed unexpected status text on hardware even after the OLED text path was corrected in code.

### Fixes already attempted in code
- Restricted custom selector handling so only the configured selector matrix position should open `SELECT`.
- Blocked stray `MO(_SELECT)` fallthrough from non-selector positions in `process_record_user`.
- Removed the default `TXT WIN` OLED text from both OLED display paths.
- Added the missing encoder button pull-up so TXT selection should only happen while the encoder button is actually pressed.
- Moved `SELECTOR_BTN_PIN` from `GP12` to `GP11` in config.

### Best next debug step
- Add temporary matrix-debug output to OLED or another visible channel:
  - show raw `row`, `col`, `keycode`, and active layer for the most recently pressed key.
- Use that to confirm what the problematic physical keys are actually reporting.
- Only after that should selector logic be changed again.

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
