#!/usr/bin/env python3
from pathlib import Path

KEYMAP = Path("keymaps/reworked/keymap.c")
if not KEYMAP.exists():
    raise SystemExit(f"Nicht gefunden: {KEYMAP}")

src = KEYMAP.read_text(encoding="utf-8")
backup = KEYMAP.with_suffix(".c.bak_before_wild_fx")
if not backup.exists():
    backup.write_text(src, encoding="utf-8")

start_marker = "static bool led_is_gap(uint8_t led_index) {"
end_marker = "#endif\n\n// ── Keymaps ─────────────────────────────────────────────────"

start = src.find(start_marker)
if start == -1:
    raise SystemExit("Startmarker nicht gefunden: static bool led_is_gap(...)")

end = src.find(end_marker, start)
if end == -1:
    raise SystemExit("Endmarker nicht gefunden: #endif vor // ── Keymaps")

rgb_block = r'''
static bool led_is_gap(uint8_t led_index) {
    uint8_t col = led_index % 5;
    return col == 1 || col == 3;
}

static bool led_is_key(uint8_t led_index) {
    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        if (key_led_map[i] == led_index) return true;
    }
    return false;
}

static uint8_t led_row_index(uint8_t led_index) { return led_index / 5; }
static uint8_t led_col_index(uint8_t led_index) { return led_index % 5; }

static uint8_t distance_u8(uint8_t a, uint8_t b) {
    return a > b ? (a - b) : (b - a);
}

static uint8_t clamp_add_u8(uint8_t a, uint8_t b) {
    uint16_t v = (uint16_t)a + b;
    return v > 255 ? 255 : (uint8_t)v;
}

static uint8_t scale_val(uint8_t value, uint8_t scale) {
    return (uint8_t)(((uint16_t)value * scale) / 255);
}

static uint8_t wrap_distance(uint8_t a, uint8_t b, uint8_t count) {
    uint8_t d1 = a > b ? (a - b) : (b - a);
    uint8_t d2 = count - d1;
    return d1 < d2 ? d1 : d2;
}

static uint8_t ping_pong_index(uint32_t now, uint16_t period_ms, uint8_t count, uint8_t phase) {
    if (count <= 1) return 0;
    return (uint8_t)(((uint16_t)triwave8_period(now, period_ms, phase) * (count - 1)) / 255);
}

#ifdef RGBLIGHT_ENABLE
static void flush_led_frame(void) {
    rgblight_driver.flush();
}

static void set_led_hsv(uint8_t led_index, uint8_t h, uint8_t s, uint8_t v) {
    if (led_index >= RGBLIGHT_LED_COUNT) return;
    if (v > RGBLIGHT_LIMIT_VAL) v = RGBLIGHT_LIMIT_VAL;
    rgb_t rgb = hsv_to_rgb((hsv_t){h, s, v});
    rgblight_driver.set_color(led_index, rgb.r, rgb.g, rgb.b);
}

static void set_key_hsv(uint8_t key_index, uint8_t h, uint8_t s, uint8_t v) {
    if (key_index >= PAD_KEY_COUNT) return;
    set_led_hsv(key_led_map[key_index], h, s, v);
}

static void set_gap_hsv(uint8_t row, uint8_t gap, uint8_t h, uint8_t s, uint8_t v) {
    if (row >= 3 || gap >= 2) return;
    set_led_hsv((row * 5) + 1 + (gap * 2), h, s, v);
}

static void clear_all_keys(void) {
    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        set_led_hsv(i, 0, 0, 0);
    }
}

static void render_minimal_profile(uint8_t layer) {
    clear_all_keys();

    if (layer == _SELECT) {
        uint8_t target_slot = slot_for_layer(selector_target);
        const select_slot_t *slot = &select_slots[target_slot];
        uint8_t val = pulse_val(timer_read32(), 2200, 0, 14, 96);
        set_key_hsv(target_slot, slot->hue, slot->sat, val);
        flush_led_frame();
        return;
    }

    uint8_t slot = slot_for_layer(layer);
    const select_slot_t *info = &select_slots[slot];
    uint8_t val = pulse_val(timer_read32(), 2400, 0, 12, 90);
    set_key_hsv(slot, info->hue, info->sat, val);
    flush_led_frame();
}

static void render_base_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(_BASE);
    uint16_t slow = effect_period_for_layer(_BASE, 6800);
    uint16_t fast = effect_period_for_layer(_BASE, 2300);

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t row = led_row_index(i);
        uint8_t col = led_col_index(i);
        uint8_t wave_a = triwave8_period(now, slow, (uint8_t)(row * 51 + col * 23));
        uint8_t wave_b = triwave8_period(now, fast, (uint8_t)(255 - col * 37 - row * 19));
        uint8_t hue = p.hue + (wave_a / 18) + row * 4;
        uint8_t val;

        if (led_is_gap(i)) {
            hue += 14;
            val = palette_floor(scale_val(p.val, 45 + (255 - wave_b) / 3), 8);
        } else {
            val = palette_floor(scale_val(p.val, 90 + wave_b / 2), 18);
        }

        set_led_hsv(i, hue, p.sat, val);
    }

    flush_led_frame();
}

static void render_window_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(_WINDOW);
    uint8_t left_head  = ping_pong_index(now, effect_period_for_layer(_WINDOW, 1450), 5, 0);
    uint8_t right_head = 4 - ping_pong_index(now, effect_period_for_layer(_WINDOW, 1700), 5, 96);

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t row = led_row_index(i);
        uint8_t col = led_col_index(i);
        uint8_t d1 = distance_u8(col, left_head);
        uint8_t d2 = distance_u8(col, right_head);
        uint8_t d = d1 < d2 ? d1 : d2;
        uint8_t hue = p.hue + row * 5 + (led_is_gap(i) ? 18 : 0);
        uint8_t val = palette_floor(p.val / 14, 6);

        if (d == 0) val = led_is_gap(i) ? palette_floor(p.val + 34, p.val) : p.val;
        else if (d == 1) val = palette_floor((p.val * 2) / 3, 24);
        else if (d == 2) val = palette_floor(p.val / 3, 12);

        if (window_browser_held && row == 1) {
            val = clamp_add_u8(val, 28);
            hue += 12;
        }

        set_led_hsv(i, hue, p.sat, val);
    }

    flush_led_frame();
}

static void render_text_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(_TEXT);
    uint8_t mode_bias = text_action_held ? 18 : (text_edit_held ? 34 : 0);
    uint16_t period = effect_period_for_layer(_TEXT, text_action_held ? 850 : (text_edit_held ? 1050 : 1600));
    uint8_t cursor = (now / (period / PAD_KEY_COUNT + 1)) % PAD_KEY_COUNT;

    clear_all_keys();

    for (uint8_t key = 0; key < PAD_KEY_COUNT; key++) {
        uint8_t d = wrap_distance(key, cursor, PAD_KEY_COUNT);
        uint8_t val = palette_floor(p.val / 18, 4);

        if (d == 0) val = palette_floor(p.val + 28, p.val);
        else if (d == 1) val = palette_floor((p.val * 3) / 5, 22);
        else if (d == 2) val = palette_floor(p.val / 4, 10);

        set_key_hsv(key, p.hue + mode_bias + key * 2, p.sat, val);
    }

    for (uint8_t row = 0; row < 3; row++) {
        for (uint8_t gap = 0; gap < 2; gap++) {
            uint8_t left_key = row * 3 + gap;
            uint8_t right_key = left_key + 1;
            uint8_t dl = wrap_distance(left_key, cursor, PAD_KEY_COUNT);
            uint8_t dr = wrap_distance(right_key, cursor, PAD_KEY_COUNT);
            uint8_t d = dl < dr ? dl : dr;
            uint8_t val = palette_floor(p.val / 24, 3);

            if (d == 0) val = palette_floor((p.val * 4) / 5, 24);
            else if (d == 1) val = palette_floor(p.val / 3, 12);

            set_gap_hsv(row, gap, p.hue + mode_bias + 12, p.sat, val);
        }
    }

    flush_led_frame();
}

static void render_media_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(_MEDIA);

    for (uint8_t row = 0; row < 3; row++) {
        uint8_t meter = (uint8_t)(((uint16_t)triwave8_period(now, effect_period_for_layer(_MEDIA, 900 + row * 250), row * 73) * 5) / 255);
        uint8_t kick = ((now / (220 + row * 37)) % 7) == 0;

        for (uint8_t col = 0; col < 5; col++) {
            uint8_t led = row * 5 + col;
            uint8_t hue = p.hue + row * 5 + col * 3;
            uint8_t val = palette_floor(p.val / 18, 5);

            if (col <= meter) val = palette_floor(scale_val(p.val, 210 - col * 24), 18);
            if (kick && col == 2) val = palette_floor(p.val + 40, p.val);

            if (led_is_gap(led)) {
                val = clamp_add_u8(val / 2, triwave8_period(now, 700, led * 23) / 5);
                hue += 9;
            }

            set_led_hsv(led, hue, p.sat, val);
        }
    }

    flush_led_frame();
}

static void render_rgb_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(_RGB);
    uint16_t spin = effect_period_for_layer(_RGB, 2400);
    uint8_t base = (uint8_t)((now * 255UL) / spin);

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t row = led_row_index(i);
        uint8_t col = led_col_index(i);
        uint8_t spark = triwave8_period(now, 520 + i * 17, i * 29);
        uint8_t hue = base + i * 19 + row * 11 + col * 5;
        uint8_t val = palette_floor(p.val / 9, 8);
        val = clamp_add_u8(val, spark / (led_is_gap(i) ? 4 : 3));

        if ((((now / 95) + i * 5) % 17) == 0) {
            val = palette_floor(p.val + 55, p.val);
        }

        set_led_hsv(i, hue, p.sat, val);
    }

    flush_led_frame();
}

static void render_dev_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(_DEV);

    if (game_mode == GAME_MODE_NAV) {
        uint8_t head = ping_pong_index(now, effect_period_for_layer(_DEV, 750), 3, 0);

        for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
            uint8_t row = led_row_index(i);
            uint8_t d = distance_u8(row, head);
            uint8_t val = palette_floor(p.val / 18, 5);

            if (d == 0) val = palette_floor(p.val + 18, p.val);
            else if (d == 1) val = palette_floor(p.val / 2, 20);

            set_led_hsv(i, p.hue + row * 6 + (led_is_gap(i) ? 12 : 0), p.sat, val);
        }

        flush_led_frame();
        return;
    }

    uint8_t boom = triwave8_period(now, effect_period_for_layer(_DEV, 1150), 0);

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t row = led_row_index(i);
        uint8_t col = led_col_index(i);
        uint8_t dist = distance_u8(row, 1) + distance_u8(col, 2);
        int16_t energy = (int16_t)boom - (int16_t)(dist * 48);
        uint8_t val = energy > 0 ? palette_floor((uint8_t)energy, 12) : palette_floor(p.val / 20, 4);

        if (led_is_gap(i)) val = clamp_add_u8(val / 2, triwave8_period(now, 600, i * 31) / 7);

        set_led_hsv(i, p.hue - dist * 5 + (led_is_gap(i) ? 10 : 0), p.sat, val);
    }

    flush_led_frame();
}

static void render_vsc_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(_VSC);
    vsc_mode_t mode = current_vsc_preview_mode();
    uint8_t packet = (now / effect_period_for_layer(_VSC, mode == VSC_MODE_CHAT ? 115 : 170)) % RGBLIGHT_LED_COUNT;
    uint8_t target_key = mode == VSC_MODE_CHAT ? 4 : 3;

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t d = wrap_distance(i, packet, RGBLIGHT_LED_COUNT);
        uint8_t val = palette_floor(p.val / 20, 4);

        if (d == 0) val = palette_floor(p.val + 35, p.val);
        else if (d == 1) val = palette_floor((p.val * 3) / 5, 20);
        else if (d == 2) val = palette_floor(p.val / 4, 10);

        if (led_is_gap(i)) val = clamp_add_u8(val, triwave8_period(now, 900, i * 17) / 9);

        set_led_hsv(i, p.hue + (mode == VSC_MODE_CHAT ? 18 : 0) + led_row_index(i) * 3, p.sat, val);
    }

    uint8_t blink = ((now / 230) % 2) ? palette_floor(p.val + 40, p.val) : palette_floor(p.val / 5, 12);
    set_key_hsv(target_key, p.hue + (mode == VSC_MODE_CHAT ? 25 : 0), p.sat, blink);
    flush_led_frame();
}

static void render_prompt_wild(void) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(_PROMPT);
    uint8_t mode_bias = prompt_mode == PROMPT_MODE_PICS ? 24 : (prompt_mode == PROMPT_MODE_ETSY ? 10 : 0);
    uint8_t orbit = (now / effect_period_for_layer(_PROMPT, 130)) % RGBLIGHT_LED_COUNT;
    uint8_t bloom = triwave8_period(now, effect_period_for_layer(_PROMPT, 1800), 0);

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t row = led_row_index(i);
        uint8_t col = led_col_index(i);
        uint8_t center_dist = distance_u8(row, 1) + distance_u8(col, 2);
        int16_t val_i = (int16_t)bloom - (int16_t)(center_dist * 38);
        uint8_t val = val_i > 0 ? palette_floor((uint8_t)val_i, 10) : palette_floor(p.val / 22, 4);
        uint8_t od = wrap_distance(i, orbit, RGBLIGHT_LED_COUNT);

        if (od == 0) val = palette_floor(p.val + 45, p.val);
        else if (od == 1 && led_is_gap(i)) val = palette_floor((p.val * 2) / 3, 22);

        set_led_hsv(i, p.hue + mode_bias + center_dist * 4, p.sat, val);
    }

    flush_led_frame();
}

static void render_select_wild(void) {
    uint32_t now = timer_read32();
    uint8_t target_slot = slot_for_layer(selector_target);
    uint8_t chase = (now / effect_period_for_layer(_SELECT, 115)) % RGBLIGHT_LED_COUNT;

    clear_all_keys();

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t row = led_row_index(i);
        uint8_t col = led_col_index(i);
        uint8_t owner_key = row * 3 + (col / 2);
        if (owner_key >= PAD_KEY_COUNT) owner_key = PAD_KEY_COUNT - 1;

        const select_slot_t *owner = &select_slots[owner_key];
        uint8_t val = led_is_gap(i) ? 7 : 3;
        uint8_t sat = led_is_gap(i) ? palette_floor(owner->sat / 2, 30) : owner->sat;
        uint8_t hue = owner->hue;

        if (i == chase && led_is_gap(i)) val = 78;

        set_led_hsv(i, hue, sat, val);
    }

    for (uint8_t i = 0; i < PAD_KEY_COUNT; i++) {
        const select_slot_t *slot = &select_slots[i];
        uint8_t val = slot->selectable ? 28 : 10;
        uint8_t sat = slot->sat;
        uint8_t hue = slot->hue;

        if (i == 4) {
            sat = 0;
            val = 22;
        }

        if (i == target_slot && i != select_cursor && slot->selectable) val = 76;

        if (i == select_cursor) {
            if (slot->selectable) val = pulse_val(now, 720, 0, 86, 190);
            else {
                sat = 0;
                val = pulse_val(now, 720, 0, 46, 132);
            }
        }

        set_key_hsv(i, hue, sat, val);
    }

    flush_led_frame();
}

static void render_effect_solid(uint8_t layer) {
    hsv_config_t p = palette_for_layer(layer);

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        set_led_hsv(i, p.hue + (led_is_gap(i) ? 6 : 0), p.sat, led_is_gap(i) ? palette_floor((p.val * 2) / 3, 16) : p.val);
    }

    flush_led_frame();
}

static void render_effect_breathing(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 1900);
    uint8_t key_val = pulse_val(now, period, 0, palette_floor(p.val / 9, 8), p.val);
    uint8_t gap_val = pulse_val(now, period, 128, palette_floor(p.val / 12, 5), palette_floor((p.val * 3) / 4, 18));

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        set_led_hsv(i, p.hue + (led_is_gap(i) ? 10 : 0), p.sat, led_is_gap(i) ? gap_val : key_val);
    }

    flush_led_frame();
}

static void render_effect_running(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 850);
    uint8_t head = (now / (period / RGBLIGHT_LED_COUNT + 1)) % RGBLIGHT_LED_COUNT;

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t d = wrap_distance(i, head, RGBLIGHT_LED_COUNT);
        uint8_t val = palette_floor(p.val / 16, 5);

        if (d == 0) val = p.val;
        else if (d == 1) val = palette_floor((p.val * 2) / 3, 22);
        else if (d == 2) val = palette_floor(p.val / 3, 12);

        set_led_hsv(i, p.hue + d * 3, p.sat, val);
    }

    flush_led_frame();
}

static void render_effect_twinkle(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 640);
    uint8_t spark_a = ((now / (period / 3 + 1)) * 7 + 2) % RGBLIGHT_LED_COUNT;
    uint8_t spark_b = ((now / (period / 5 + 1)) * 11 + 6) % RGBLIGHT_LED_COUNT;

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t shimmer = triwave8_period(now, 700 + i * 53, i * 31);
        uint8_t val = palette_floor(p.val / 18, 4);
        val = clamp_add_u8(val, shimmer / 12);

        if (i == spark_a || i == spark_b) val = palette_floor(p.val + 48, p.val);

        set_led_hsv(i, p.hue + i * 2 + (led_is_gap(i) ? 8 : 0), p.sat, val);
    }

    flush_led_frame();
}

static void render_effect_pulse(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 1100);
    uint8_t blast = triwave8_period(now, period, 0);

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t d = distance_u8(led_row_index(i), 1) + distance_u8(led_col_index(i), 2);
        int16_t e = (int16_t)blast - (int16_t)(d * 48);
        uint8_t val = e > 0 ? palette_floor((uint8_t)e, 8) : palette_floor(p.val / 22, 4);
        set_led_hsv(i, p.hue + d * 5, p.sat, val);
    }

    flush_led_frame();
}

static void render_effect_comet(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 930);
    uint8_t head = (now / (period / RGBLIGHT_LED_COUNT + 1)) % RGBLIGHT_LED_COUNT;

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t d = (head + RGBLIGHT_LED_COUNT - i) % RGBLIGHT_LED_COUNT;
        uint8_t val = palette_floor(p.val / 20, 4);

        if (d == 0) val = p.val;
        else if (d == 1) val = palette_floor((p.val * 3) / 4, 24);
        else if (d == 2) val = palette_floor(p.val / 2, 16);
        else if (d == 3) val = palette_floor(p.val / 4, 8);

        set_led_hsv(i, p.hue + d * 4, p.sat, val);
    }

    flush_led_frame();
}

static void render_effect_scan(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint8_t col_head = ping_pong_index(now, effect_period_for_layer(layer, 1200), 5, 0);

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t d = distance_u8(led_col_index(i), col_head);
        uint8_t val = palette_floor(p.val / 18, 5);

        if (d == 0) val = p.val;
        else if (d == 1) val = palette_floor(p.val / 2, 18);

        set_led_hsv(i, p.hue + led_col_index(i) * 4, p.sat, val);
    }

    flush_led_frame();
}

static void render_effect_rainbow(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 2900);
    uint8_t offset = (uint8_t)((now * 255UL) / period);

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t val = pulse_val(now, effect_period_for_layer(layer, 1500), i * 19, palette_floor(p.val / 4, 20), p.val);
        set_led_hsv(i, offset + i * 24 + (led_is_gap(i) ? 12 : 0), p.sat, val);
    }

    flush_led_frame();
}

static void render_effect_stack(uint8_t layer) {
    uint32_t now = timer_read32();
    hsv_config_t p = palette_for_layer(layer);
    uint16_t period = effect_period_for_layer(layer, 1600);
    uint8_t filled = ((now % period) * (RGBLIGHT_LED_COUNT + 1)) / period;
    bool reverse = ((now / period) % 2) != 0;

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        uint8_t pos = reverse ? (RGBLIGHT_LED_COUNT - 1 - i) : i;
        uint8_t val = pos < filled ? p.val : palette_floor(p.val / 18, 4);

        if (pos == filled && filled < RGBLIGHT_LED_COUNT) val = palette_floor(p.val + 40, p.val);

        set_led_hsv(i, p.hue + pos * 2, p.sat, val);
    }

    flush_led_frame();
}

static void render_rgb_layer_visuals(void) {
    uint8_t layer = active_layer_raw();
    rgb_effect_mode_t effect = effect_for_layer(layer);

    if (effect == RGB_EFFECT_OFF) {
        clear_all_keys();
        flush_led_frame();
        return;
    }

    if (rgb_minimal_mode) {
        render_minimal_profile(layer);
        return;
    }

    switch (effect) {
        case RGB_EFFECT_BREATHING: render_effect_breathing(layer); return;
        case RGB_EFFECT_RUNNING:   render_effect_running(layer);   return;
        case RGB_EFFECT_TWINKLE:   render_effect_twinkle(layer);   return;
        case RGB_EFFECT_PULSE:     render_effect_pulse(layer);     return;
        case RGB_EFFECT_SOLID:     render_effect_solid(layer);     return;
        case RGB_EFFECT_COMET:     render_effect_comet(layer);     return;
        case RGB_EFFECT_SCAN:      render_effect_scan(layer);      return;
        case RGB_EFFECT_RAINBOW:   render_effect_rainbow(layer);   return;
        case RGB_EFFECT_STACK:     render_effect_stack(layer);     return;
        case RGB_EFFECT_WILD:
        default:
            break;
    }

    switch (layer) {
        case _BASE:   render_base_wild();   break;
        case _WINDOW: render_window_wild(); break;
        case _TEXT:   render_text_wild();   break;
        case _MEDIA:  render_media_wild();  break;
        case _RGB:    render_rgb_wild();    break;
        case _DEV:    render_dev_wild();    break;
        case _VSC:    render_vsc_wild();    break;
        case _PROMPT: render_prompt_wild(); break;
        case _SELECT: render_select_wild(); break;
        default:      render_base_wild();   break;
    }
}
#endif
'''

src = src[:start] + rgb_block + src[end + len("#endif\n"):]
KEYMAP.write_text(src, encoding="utf-8")

print("Fertig.")
print(f"Geändert: {KEYMAP}")
print(f"Backup:   {backup}")
