# 9key_makro_master - Project Notes

## Current truth

### Canonical target

- `keymaps/reworked` is the canonical implementation target.
- The project is now a 3x3 command deck with layer selection, OLED legends, custom RGB rendering, VIA configuration, encoder actions, prompt macros, and planned Tap Dance launcher layers.
- The current design direction is: keep the visible surface simple, hide advanced layers behind double-tap selector actions, and make the OLED explain the active context.

### Hardware / setup

- RP2040 / QMK macropad.
- 3x3 key matrix.
- 65 RGB LEDs total.
  - LEDs `0..14` = keyfield.
  - 9 key LEDs = `0, 2, 4, 5, 7, 9, 10, 12, 14`.
  - 6 gap LEDs = `1, 3, 6, 8, 11, 13`.
  - LEDs `15..64` = frame light chain.
- SSD1306 OLED on I2C.
- Rotary encoder with button.
- Top-left key is the main `SEL` / selector entry.
- GP12 is intended as OLED legend-page button, not as a last-key toggle.
- Encoder button + OLED/GP button hold remains the EEPROM-clear recovery combo.

### Important config targets

When the hidden selector / Tap Dance version is active:

```c
#define DYNAMIC_KEYMAP_LAYER_COUNT 14
#define VIA_EEPROM_CUSTOM_CONFIG_SIZE 128
#define VIA_FIRMWARE_VERSION 0x0000000B
```

And in `rules.mk`:

```make
TAP_DANCE_ENABLE = yes
```

---

## Layer model

### Visible primary layers

- `BASE`
- `WINDOW`
- `TEXT`
- `MEDIA`
- `GAME` / `_DEV`
- `VSC`
- `RGB`
- `PROMPT`
- `SELECT`

### RGB helper layers

- `RGBMOD`
- `RGBADJ`

These are RGB-internal/helper layers and should be treated as part of the RGB control system, not as ordinary user-facing work layers.

### Hidden / second-level launcher layers

Planned / current v7 direction:

- `MARK` = bookmarks, apps, folders, web targets.
- `WORK` = workflow shortcuts, writing/build/listing pipelines.
- `SYS` = terminal, QMK, Git, logs, system tools.

These are intentionally hidden behind double-tap selector slots instead of being placed visibly in the main selector grid.

---

## Selector behavior

### Main selector grid

```text
[ SEL ][ WIN+ ][ TXT+  ]
[ MED ][ RGB  ][ GAME  ]
[ VSC+][ BASE ][ PROMT ]
```

### Hidden selector meaning

- `WIN` single tap = `WINDOW`.
- `WIN` double tap = `MARK`.
- `TXT` single tap = `TEXT`.
- `TXT` double tap = `WORK`.
- `VSC` single tap = `VSC`.
- `VSC` double tap = `SYS`.

The `+` in the OLED/select legend means: this selector position has a hidden double-tap layer behind it.

### Selector rules

- Holding `SEL` opens the selector grid.
- `SEL_*` target actions must only prepare `selector_target`.
- Actual layer switching happens when the physical `SEL` / `MO(_SELECT)` key is released.
- This prevents accidental jumps if a selector keycode is triggered outside the held selector state.
- Double-tapping `SEL` without changing target returns to `BASE`.
- Encoder in `SELECT` cycles target slots only while the encoder button is held.

---

## Current layer layouts

Physical grid reference:

```text
[ r0c0 ][ r0c1 ][ r0c2 ]
[ r1c0 ][ r1c1 ][ r1c2 ]
[ r2c0 ][ r2c1 ][ r2c2 ]
```

### BASE

```text
[ SEL  ][ UP   ][ BSPC ]
[ LEFT ][ ENT  ][ RGHT ]
[ UNDO ][ DOWN ][ REDO ]
```

### WINDOW

Default:

```text
[ SEL   ][ BRO  ][ SNAP ]
[ DESK< ][ TASK ][ DESK>]
[ WIN<  ][ SHOW ][ WIN> ]
```

Browser mode:

```text
[ SEL  ][ BRO  ][ SNAP ]
[ BACK ][ REFR ][ FWD  ]
[ TAB< ][ NEW  ][ TAB> ]
```

Snap mode:

```text
[ SEL  ][ BRO  ][ SNAP ]
[ MAX  ][ UP   ][ CLOSE]
[ LEFT ][ DOWN ][ RGHT ]
```

Notes:

- `BRO` is a toggle mode, not momentary.
- `SNAP` is a toggle mode, not momentary.
- `BRO` and `SNAP` are mutually exclusive.

### TEXT

Default:

```text
[ SEL  ][ ACT  ][ EDIT ]
[ HOME ][ UP   ][ END  ]
[ LEFT ][ DOWN ][ RGHT ]
```

Action mode:

```text
[ SEL ][ ACT  ][ EDIT  ]
[ ALL ][ COPY ][ PASTE ]
[ CUT ][ UNDO ][ REDO  ]
```

Edit mode:

```text
[ SEL ][ ACT  ][ EDIT ]
[ ENT ][ BSPC ][ DEL  ]
[ TAB ][ SPC  ][ SHIFT]
```

Notes:

- Encoder moves cursor left/right.
- Encoder button + encoder selects text left/right.
- Follow-up encoder-button tap can copy selected text.

### MEDIA

```text
[ SEL  ][ PREV ][ NEXT ]
[ VOL- ][ PLAY ][ VOL+ ]
[ RWND ][ MUTE ][ FFWD ]
```

### GAME / DEV

NAV mode:

```text
[ SEL  ][ NAV  ][ WASD ]
[ ESC  ][ UP   ][ ENT  ]
[ LEFT ][ DOWN ][ RGHT ]
```

WASD mode:

```text
[ SEL  ][ NAV ][ WASD ]
[ SHFT ][ W   ][ SPC  ]
[ A    ][ S   ][ D    ]
```

### VSC

Navigation mode:

```text
[ SEL  ][ NAV ][ AI  ]
[ EXPL ][ SRC ][ TERM]
[ GIT  ][ GPT ][ RUN ]
```

AI mode:

```text
[ SEL  ][ NAV  ][ AI     ]
[ SUM  ][ REVW ][ FIX    ]
[ TEST ][ EXPL ][ COMMIT ]
```

Notes:

- `NAV` is the former `BAR` concept.
- `AI` is the former `CHAT` concept.
- VS Code commands and AI prompt strings should remain centralized in editable arrays.

### PROMPT

PROMPT stays intentionally untouched unless explicitly requested.

Base mode:

```text
[ SEL  ][ PICS ][ ETSY  ]
[ SUM  ][ REVW ][ FIX   ]
[ TEST ][ EXPL ][ COMMIT]
```

PICS mode:

```text
[ SEL   ][ PICS ][ ETSY ]
[ GEAR  ][ LETT ][ BGEXT]
[ WALL  ][ SVG  ][ MOCK ]
```

ETSY mode:

```text
[ SEL  ][ PICS  ][ ETSY]
[ TAGS ][ TITLE ][ DESC]
[ BULL ][ LIST  ][ SEO ]
```

### RGB

Normal:

```text
[ SEL  ][ ZONE ][ ANIM]
[ HUE+ ][ HUE- ][ VAL+]
[ SAT+ ][ SAT- ][ VAL-]
```

ZONE held:

```text
[ SEL  ][ ZONE ][ ALL ]
[ FRME ][ KEY  ][ GAP ]
[ ---- ][ ---- ][ ----]
```

Notes:

- `TOG` was removed from the normal RGB layer because `ALL` in ZONE mode covers that purpose.
- `ANIM` cycles RGB animation modes.

### MARK

Launcher / bookmarks / apps shell:

```text
[ SEL  ][ WEB  ][ APP ]
[ SHOP ][ AI   ][ DEV ]
[ MAIL ][ FILE ][ SYS ]
```

Planned Tap Dance idea:

- single tap = most common target.
- double tap = related alternate target.
- hold = category / deeper layer / powerful action.

### WORK

Workflow shell:

```text
[ SEL  ][ PLAN ][ WRITE]
[ SHOP ][ CODE ][ BUILD]
[ IMG  ][ LIST ][ CHECK]
```

### SYS

System / maintenance shell:

```text
[ SEL  ][ TERM ][ TASK]
[ QMK  ][ GIT  ][ USB ]
[ CONF ][ LOG  ][ LOCK]
```

---

## OLED

### New direction

Last-key view is not important anymore. It is redundant because the effect of a key is visible when pressing it. The OLED should instead explain the current command context.

GP12 should cycle legend pages:

```text
Keys -> Tap Dance -> RGB Help -> System Help -> Keys
```

### OLED pages

- `Keys`: active layer 3x3 legend.
- `Tap Dance`: hidden selector / Tap Dance help.
- `RGB Help`: RGB zones and current animation mode.
- `System Help`: SEL, GP12, encoder help, reset combo.

### Encoder help

- Long encoder-button hold still shows temporary Encoder Help.
- After timeout, OLED should return to the selected legend page.

---

## RGB system

### Zones

RGB is split into three independently configurable VIA zones:

- Key LEDs.
- Gap LEDs.
- Frame LEDs.

Each zone should have per-layer:

- color
- brightness
- effect
- effect speed

### Effects

Base effect set:

- Wild
- Breathing
- Running
- Twinkle
- Pulse
- Solid
- Comet
- Scan
- Rainbow
- Stack
- Off

Extended effect ideas:

- Nav Blink
- Typewriter
- Visualizer
- Pong
- Packet
- Bloom
- Sweep

### RGB animation modes

`ANIM` cycles:

- `TILT`: placeholder for upcoming tilt sensor.
- `LKEY`: only layer/mode keys light.
- `RACT`: reactive key flash.
- `WALK`: encoder frame wander/chase.

### Encoder frame wander

- When `WALK` is active, encoder rotation sends a short frame chase in the rotation direction.
- When `WALK` is not active, encoder on RGB layer controls brightness.

---

## Encoder behavior by layer

- `BASE`: mouse wheel up/down.
- `WINDOW`: previous/next window; browser mode uses page/tab navigation; snap mode uses snap direction.
- `TEXT`: cursor left/right; with encoder button held = select text.
- `MEDIA`: volume down/up.
- `RGB`: brightness down/up, except in `WALK` animation mode where it triggers frame wander.
- `GAME`: weapon/inventory scroll.
- `VSC`: `Ctrl+PgUp` / `Ctrl+PgDn`.
- `PROMPT`: `Ctrl+PgUp` / `Ctrl+PgDn`.
- `SELECT`: move through selector targets only while encoder button is held.

---

## VIA

### Menus should be named by LED zone, not by layer concept

Use these three RGB menus:

- `Key LEDs`
- `Gap LEDs`
- `Frame LEDs`

Each menu contains per-layer controls:

- color
- brightness
- effect
- effect speed

### Display menu

Display menu should no longer be `Legend / Last Key`.

Use:

- Keys
- Tap Dance
- RGB Help
- System Help

---

## Tap Dance plan

### Selector Tap Dance

Only selected positions in the selector grid should use Tap Dance:

- `WIN+`: single = `WINDOW`, double = `MARK`.
- `TXT+`: single = `TEXT`, double = `WORK`.
- `VSC+`: single = `VSC`, double = `SYS`.

Important:

- Tap Dance selector actions must only set `selector_target`.
- They must not call `layer_move()` directly.
- Real layer switching still happens on selector release.

### MARK Tap Dance future plan

Example plan:

| Key | Single | Double | Hold |
| --- | --- | --- | --- |
| WEB | Browser | Search | Bookmarks |
| APP | Calculator | Media app | Task Manager |
| SHOP | Etsy Dashboard | Listings | Prompt Etsy |
| AI | ChatGPT | Copilot | Prompt Base |
| DEV | GitHub Repo | Actions | VSC |
| MAIL | Mail | Calendar | Tasks |
| FILE | Project folder | QMK folder | Terminal |
| SYS | Settings | Task Manager | Lock/Sleep |

Do not add all advanced Tap Dance actions at once. Add MARK first, test, then expand.

---

## Build / safety notes

### Local build

```powershell
qmk compile -kb 9key_makro_master_qmk -km reworked
```

### Safe development rules

- Do not refactor the whole keymap casually.
- Do not mix RGB-control fixes, OLED logic, Tap Dance, and VIA EEPROM changes in one uncontrolled pass.
- Keep `PROMPT` behavior untouched unless explicitly requested.
- Keep `keymaps/reworked` canonical.
- Prefer small, understandable version steps.
- If a patch changes hundreds or thousands of lines unexpectedly, stop and inspect before committing.

---

## Current hardware debugging note

Recent issue:

- After opening the keyboard and adding tape to darken rear keys, all keys worked except the selector at `r0c0`.
- Multimeter continuity can still pass even when a solder joint or mechanical press is unreliable.
- Likely causes to check first:
  - switch not mechanically actuating fully after reassembly
  - tape or case pressure interfering
  - cracked/cold solder joint that only contacts while probing
  - row/column pad or diode pad stressed during reassembly
- Quickest isolation:
  - open case
  - bridge selector switch pads directly with tweezers
  - if bridging works: switch/mechanics/tape
  - if bridging fails: matrix/diode/solder/firmware path

---

## Direction

The design goal is now:

```text
simple visible layers
+ hidden double-tap power layers
+ OLED context pages
+ VIA-controlled RGB zones
+ Tap Dance launcher universe
```

This is no longer just a macropad. It is a 9-key command deck.
