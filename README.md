# 9key Makro Master QMK

A compact 3×3 RP2040 macro pad firmware for QMK with VIA support, OLED legends, rotary encoder control, custom RGB zones, layer-specific animations, and a selector-driven workflow system.

This firmware is built around one idea: **nine keys, many contexts, no chaos**.

> Current firmware direction: visible layers stay simple; advanced layers and helper views sit behind selector/tap-dance style interactions.

---

## Highlights

- 3×3 macro pad layout
- RP2040 Zero style target
- VIA-compatible configuration
- SSD1306 OLED support
- Rotary encoder support
- 65 RGB LEDs:
  - 9 key LEDs
  - 6 gap LEDs
  - 50 frame LEDs
- Separate VIA control for:
  - Key LEDs
  - Gap LEDs
  - Frame LEDs
- Per-layer RGB effects, colors, brightness, and speed
- Layer selector system
- Hidden selector layers via double-tap concept
- RGB zone control layer
- RGB animation modes
- Encoder-triggered frame wander animation
- VS Code / Copilot helper layer
- Prompt macro layer
- Game mode with NAV/WASD modes
- Planned tap-dance launcher system for bookmarks, apps, work tools, and system helpers

---

## Hardware Overview

| Part | Configuration |
|---|---|
| MCU | RP2040 Zero / RP2040-based board |
| Matrix | 3 rows × 3 columns |
| RGB | 65 addressable LEDs |
| OLED | SSD1306, 128×32 or compatible |
| Encoder | Rotary encoder with button |
| VIA | Custom menu support |
| Main RGB pin | GP13 in current config |
| Encoder pins | GP8 / GP9 depending on keyboard config |
| Encoder button | GP10 in current config |
| OLED toggle / legend button | GP12 in current config |

The firmware assumes a 3×5 physical LED area for the keyfield:

```text
0   1   2   3   4
5   6   7   8   9
10  11  12  13  14
```

Key LEDs:

```text
0, 2, 4
5, 7, 9
10, 12, 14
```

Gap LEDs:

```text
1, 3
6, 8
11, 13
```

Frame LEDs:

```text
15–64
```

---

## Layer Philosophy

The layout is built around consistent positions:

```text
[SEL] [Mode A] [Mode B]
[Left/Prev] [Main] [Right/Next]
[Less/Back] [Alt] [More/Forward]
```

Where possible:

- Top-left is always selector/back-to-layer-control.
- Top-middle and top-right are mode/context keys.
- Center is the main action.
- Left/right positions behave spatially.
- Bottom-left/bottom-right are usually undo/redo, previous/next, lower/higher, or secondary actions.

---

## Main Layers

### BASE

General navigation and editing basics.

```text
SEL   UP    BSPC
LEFT  ENT   RGHT
UNDO  DOWN  REDO
```

---

### WINDOW

Desktop/window/browser/snap control.

Normal mode:

```text
SEL    BRO   SNAP
DESK<  TASK  DESK>
WIN<   SHOW  WIN>
```

`BRO` toggles Browser mode:

```text
SEL   BRO   SNAP
BACK  REFR  FWD
TAB<  NEW   TAB>
```

`SNAP` toggles Snap mode:

```text
SEL    BRO   SNAP
MAX    UP    CLOSE
LEFT   DOWN  RGHT
```

BRO and SNAP are toggle modes, not momentary holds. Turning one on turns the other off.

---

### TEXT

Text navigation with action/edit submodes.

Normal:

```text
SEL   ACT   EDIT
HOME  UP    END
LEFT  DOWN  RGHT
```

ACT mode:

```text
SEL   ACT   EDIT
ALL   COPY  PASTE
CUT   UNDO  REDO
```

EDIT mode:

```text
SEL   ACT   EDIT
ENT   BSPC  DEL
TAB   SPC   SHIFT
```

---

### MEDIA

Media transport and volume.

```text
SEL   PREV  NEXT
VOL-  PLAY  VOL+
RWND  MUTE  FFWD
```

---

### GAME

Game layer with two modes.

Top row:

```text
SEL  NAV  WASD
```

NAV mode:

```text
SEL   NAV   WASD
ESC   UP    ENT
LEFT  DOWN  RGHT
```

WASD mode:

```text
SEL   NAV   WASD
SHFT  W     SPC
A     S     D
```

---

### VSC

VS Code / development helper layer.

Normal navigation mode:

```text
SEL   NAV   AI
EXPL  SRC   TERM
GIT   GPT   RUN
```

AI mode gives prompt-style helper actions:

```text
SEL    NAV   AI
SUM    REVW  FIX
TEST   EXPL  COMMIT
```

The VSC layer includes macros for QMK diagnostics, code review, fix suggestions, test generation, code explanation, and commit-message help.

---

### PROMPT

Prompt macro layer. This layer intentionally stays focused and separate from the launcher/workflow system.

Base mode:

```text
SEL   PICS  ETSY
SUM   REVW  FIX
TEST  EXPL  COMMIT
```

PICS mode contains image/product-picture helper prompts.

ETSY mode contains Etsy listing, tag, title, description, bullet, listing, and SEO helper prompts.

---

### RGB

RGB control layer.

Normal:

```text
SEL   ZONE  ANIM
HUE+  HUE-  VAL+
SAT+  SAT-  VAL-
```

ZONE mode:

```text
SEL   ZONE  ALL
FRME  KEY   GAP
----  ----  ----
```

RGB zones:

| Zone | Meaning |
|---|---|
| KEY | 9 key LEDs |
| GAP | 6 gap LEDs |
| FRME | 50 frame LEDs |
| ALL | all RGB zones |

Animation selector modes:

| Mode | Meaning |
|---|---|
| TILT | Placeholder for future tilt sensor |
| LKEY | Only layer/mode keys light |
| RACT | Reactive key flash |
| WALK | Encoder frame wander mode |

When `WALK` is active, turning the encoder sends a temporary chase/wander pulse around the frame LEDs in the direction of rotation.

---

## Hidden Selector Layers

The selector layer keeps the visible layout compact while allowing extra layers behind double-tap selector positions.

Selector concept:

```text
SEL   WIN+  TXT+
MED   RGB   GAME
VSC+  BASE  PRM
```

Meaning:

| Selector key | Single tap | Double tap |
|---|---|---|
| WIN+ | WINDOW | MARK |
| TXT+ | TEXT | WORK |
| VSC+ | VSC | SYS |

The visible layer remains simple. Power layers live behind the obvious related keys.

---

## Planned Launcher Layers

These layers are designed as shells for tap-dance launcher actions.

### MARK

Bookmarks, apps, common locations.

```text
SEL   WEB   APP
SHOP  AI    DEV
MAIL  FILE  SYS
```

Suggested tap-dance model:

| Key | Single tap | Double tap | Hold |
|---|---|---|---|
| WEB | Browser | Search | Bookmarks |
| APP | Calculator/app | Media app | Task Manager |
| SHOP | Etsy Dashboard | Listings | PROMPT ETSY |
| AI | ChatGPT | Copilot | PROMPT |
| DEV | GitHub repo | Actions | VSC |
| MAIL | Mail | Calendar | Contacts/tasks |
| FILE | Project folder | QMK folder | Terminal |
| SYS | Settings | Task Manager | Lock/sleep |

### WORK

Workflow-oriented layer.

```text
SEL   PLAN  NOTES
SHOP  CODE  BUILD
IMG   LIST  CHECK
```

### SYS

System / maintenance / QMK helper layer.

```text
SEL   TERM  TASK
QMK   GIT   USB
CONF  LOG   LOCK
```

---

## OLED System

The OLED is used as a live legend instead of only showing last key output.

Recommended OLED pages:

| Page | Purpose |
|---|---|
| Keys | Current layer key legend |
| Tap Dance | Launcher/tap-dance help |
| RGB Help | RGB zones and animation mode help |
| System Help | Selector, encoder and system shortcuts |

GP12 cycles OLED legend pages.

Encoder button long hold temporarily displays encoder help.

---

## RGB System

The firmware uses a custom renderer instead of relying only on stock QMK RGB behavior.

RGB is split into three independently configurable zones:

1. Key LEDs
2. Gap LEDs
3. Frame LEDs

Each zone can have its own:

- Color
- Brightness
- Effect
- Speed

Supported effect family:

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
- Nav Blink
- Typewriter
- Visualizer
- Pong
- Packet
- Bloom
- Sweep

Layer-specific custom animation ideas include:

| Layer | Suggested animation character |
|---|---|
| WINDOW | navigation blink |
| TEXT | typewriter fill |
| MEDIA | music visualizer |
| GAME | Pong ball |
| VSC | packet/data stream |
| PROMPT | AI bloom/thinking dots |
| RGB | sweep/twinkle/control feedback |

---

## VIA Support

The VIA definition provides custom menus for:

- OLED default legend page
- Key LED color/effect/speed/brightness
- Gap LED color/effect/speed/brightness
- Frame LED color/effect/speed/brightness
- Custom keycodes for all layer controls and macro targets

Because the firmware stores custom VIA RGB settings, the EEPROM config size must be large enough.

Recommended config values for the current expanded firmware:

```c
#define DYNAMIC_KEYMAP_LAYER_COUNT 14
#define VIA_EEPROM_CUSTOM_CONFIG_SIZE 128
#define VIA_FIRMWARE_VERSION 0x0000000B
```

Tap Dance must be enabled if the hidden selector / launcher tap-dance system is used:

```make
TAP_DANCE_ENABLE = yes
```

---

## Build

From your QMK root:

```powershell
qmk compile -kb 9key_makro_master_qmk -km reworked
```

Or, from inside the keyboard directory, use the equivalent target path used by your local QMK setup.

---

## Flash

Typical QMK flash command:

```powershell
qmk flash -kb 9key_makro_master_qmk -km reworked
```

For RP2040 boards, you may need to enter BOOTSEL mode and copy the generated UF2 manually, depending on your setup.

---

## File Placement

Recommended repository structure:

```text
qmk_firmware/
└─ keyboards/
   └─ 9key_makro_master_qmk/
      ├─ config.h
      ├─ keyboard.json
      ├─ rules.mk
      ├─ via3x3.json
      └─ keymaps/
         └─ reworked/
            └─ keymap.c
```

---

## Safety Notes

- This keymap sends strings and shortcuts. Review macros before flashing.
- Some commands may open URLs, apps, terminals, or system dialogs.
- Do not put destructive commands on single tap.
- Keep shutdown/sleep/lock actions behind hold or double-tap interactions.
- After changing VIA config structs, bump `REWORKED_LAYOUT_VERSION` to force a clean EEPROM default reload.
- If VIA behaves strangely after firmware changes, reset EEPROM once.

---

## Development Notes

Useful workflow:

```powershell
qmk doctor
qmk compile -kb 9key_makro_master_qmk -km reworked
```

If the build breaks:

1. Check the first compiler error, not the last.
2. Check whether a helper function is used before declaration.
3. Check that custom keycodes do not exceed reserved keyboard-level ranges.
4. Check that `DYNAMIC_KEYMAP_LAYER_COUNT` matches the layer enum.
5. Check that `VIA_EEPROM_CUSTOM_CONFIG_SIZE` matches the custom config struct.

---

## Roadmap

- Fill MARK tap-dance launcher actions
- Fill WORK workflow actions
- Fill SYS maintenance actions
- Add tilt sensor handling for RGB `TILT` mode
- Add OLED tap-dance preview page
- Add optional VIA-editable launcher slots
- Polish RGB animation defaults per layer

---

## License

Add your preferred license before publishing. If unsure, MIT is simple for firmware projects, but review what fits your goals.

---

## Credits

Built with QMK Firmware and VIA-compatible custom configuration.

Project direction: compact command deck, deep layers, visible legends, and custom RGB feedback.
