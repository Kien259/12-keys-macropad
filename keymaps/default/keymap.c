// Copyright 2024 Kyran Hoang
// SPDX-License-Identifier: GPL-2.0-or-later

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  PHYSICAL LAYOUT  —  3 rows × 4 cols + encoder push button             │
// │                                                                         │
// │   COL0(GP3) COL1(GP4) COL2(GP5) COL3(GP6)                             │
// │                                  ┌────────┐                            │
// │                                  │ [ENC]  │  CLK=GP7, DT=GP8          │
// │                                  │  SW    │  SW=GP9 (direct pin)      │
// │                                  │        │  Rotate: Vol+/Vol-         │
// │                                  │        │  Press:  Mute              │
// │   ┌────────┬────────┬────────┬───┴────────┤                            │
// │   │  K00   │  K01   │  K02   │  K03       │  ROW 0 (GP0)              │
// │   ├────────┼────────┼────────┼────────────┤                            │
// │   │  K10   │  K11   │  K12   │  K13       │  ROW 1 (GP1)              │
// │   ├────────┼────────┼────────┼────────────┤                            │
// │   │  K20   │  K21   │  K22   │  K23       │  ROW 2 (GP2)              │
// │   └────────┴────────┴────────┴────────────┘                            │
// │                                                                         │
// │  ENC_SW (GP9) is a DIRECT_PIN key — not part of the switch matrix.    │
// │  Diodes: 1N4148, cathode (stripe) toward ROW pin (COL2ROW).           │
// └─────────────────────────────────────────────────────────────────────────┘

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  OS PROFILE SWITCH                                                      │
// │                                                                         │
// │  KC_PRFSW cycles:  macOS → Linux → Windows → macOS …                  │
// │  Saved to EEPROM — survives power-off.                                 │
// │                                                                         │
// │  App keys send HYPER chords — guaranteed unique, nothing in any OS    │
// │  or application uses all four modifiers at once.                       │
// │                                                                         │
// │  HYPER = Ctrl + Shift + Alt + Cmd   (macOS)                           │
// │        = Ctrl + Shift + Alt + Win   (Windows)                         │
// │        = Ctrl + Shift + Alt + Super (Linux)                           │
// │                                                                         │
// │  Slot → chord:                                                         │
// │    App1 → Hyper+A    App4 → Hyper+D    App7 → Hyper+G                │
// │    App2 → Hyper+B    App5 → Hyper+E    App8 → Hyper+H                │
// │    App3 → Hyper+C    App6 → Hyper+F    App9 → Hyper+I                │
// │                                                                         │
// │  Bind these in your OS:                                                │
// │    macOS   → Raycast / BetterTouchTool / Hammerspoon                  │
// │    Linux   → sxhkd / xbindkeys / KDE custom shortcuts                 │
// │    Windows → AutoHotKey  (#^!+a::Run, "app.exe")                      │
// └─────────────────────────────────────────────────────────────────────────┘

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  DISPLAY  (320×172 px, landscape, ST7789 SPI)                          │
// │                                                                         │
// │   x=0          x=160                    x=320                          │
// │   ┌────────────────┬────────────────────┐  y=0                         │
// │   │Layout: name    │                    │                               │
// │   │  > Media       │   KEY INFO         │                               │
// │   │    App         │                    │  top 112 px                  │
// │   │    Macro       │   <key name>       │                               │
// │   │                │   dbl-tap to send  │                               │
// │   ├────────────────┴────────────────────┤  y=112                       │
// │   │                                     │                               │
// │   │           Bongo Cat 🐱              │  bottom 60 px                │
// │   │     (animated, WPM-driven)          │                               │
// │   └─────────────────────────────────────┘  y=172                       │
// │                                                                         │
// │  Top-left : layer list, ">" arrow marks the active layer               │
// │  Top-right: last pressed key name; turns green on double-tap           │
// │  Bottom   : bongo cat — idle / prep / typing based on WPM              │
// └─────────────────────────────────────────────────────────────────────────┘

#include QMK_KEYBOARD_H
#include "bongo_display.h"
#include "eeprom.h"

// ── OS profiles ───────────────────────────────────────────────────────────────
// Stored in EEPROM at a dedicated offset so the choice survives power-off.
// Change APP_KEY(n) below to remap what each App-layer key does per OS.
typedef enum {
    OS_MACOS = 0,
    OS_LINUX,
    OS_WINDOWS,
    OS_COUNT,
} os_profile_t;

static const char *os_profile_names[OS_COUNT] = {
    "macOS",
    "Linux",
    "Windows",
};

// EEPROM slot: use a fixed address well past QMK's reserved range (first 32 bytes).
#define EEPROM_OS_PROFILE_ADDR 32

static os_profile_t current_os = OS_MACOS;

static void load_os_profile(void) {
    uint8_t saved = eeprom_read_byte((uint8_t *)EEPROM_OS_PROFILE_ADDR);
    current_os = (saved < OS_COUNT) ? (os_profile_t)saved : OS_MACOS;
}

static void save_os_profile(void) {
    eeprom_update_byte((uint8_t *)EEPROM_OS_PROFILE_ADDR, (uint8_t)current_os);
}

// Returns the App keycode for slot n (0-based).
//
// All three OS profiles send identical HYPER chords:
//   Ctrl + Shift + Alt + Gui/Win/Super + letter
//
// These are universally conflict-free — no OS, browser, or app binds
// all four modifiers simultaneously.
//
// ┌──────┬────────────┐
// │  n   │  chord     │
// ├──────┼────────────┤
// │  0   │  Hyper+A   │  ← App 1
// │  1   │  Hyper+B   │  ← App 2
// │  2   │  Hyper+C   │  ← App 3
// │  3   │  Hyper+D   │  ← App 4
// │  4   │  Hyper+E   │  ← App 5
// │  5   │  Hyper+F   │  ← App 6
// │  6   │  Hyper+G   │  ← App 7
// │  7   │  Hyper+H   │  ← App 8
// │  8   │  Hyper+I   │  ← App 9
// └──────┴────────────┘
//
// To remap a slot, replace the KC_x letter in the array below.
// HYPER = LCTL + LSFT + LALT + LGUI  (all four modifiers)
#define HYPER(kc) LCTL(LSFT(LALT(LGUI(kc))))

static uint16_t app_key(uint8_t n) {
    static const uint16_t letters[9] = {
        KC_A, KC_B, KC_C,   // App 1-3
        KC_D, KC_E, KC_F,   // App 4-6
        KC_G, KC_H, KC_I,   // App 7-9
    };
    if (n >= 9) return KC_NO;
    return HYPER(letters[n]);
}

// ── Custom keycodes ───────────────────────────────────────────────────────────
enum custom_keycodes {
    KC_LYRSW = SAFE_RANGE,  // Cycle keyboard layer
    KC_PRFSW,               // Cycle OS profile (macOS / Linux / Windows)
    KC_APP0,  KC_APP1,  KC_APP2,  KC_APP3,  KC_APP4,
    KC_APP5,  KC_APP6,  KC_APP7,  KC_APP8,  KC_APP9,
};

// ── Layers ────────────────────────────────────────────────────────────────────
enum layers {
    LAYER_MEDIA = 0,
    LAYER_APP,
    LAYER_MACRO,
    LAYER_COUNT,
};

// Used by bongo_display.c via display_set_layer(); suppress unused-variable warning.
static const char *layer_names[LAYER_COUNT] __attribute__((unused)) = {
    "Media",
    "App",
    "Macro",
};

// Double-tap state
typedef struct {
    uint16_t keycode;
    uint16_t timer;
    bool     pending;
} tap_state_t;

static tap_state_t tap_state = {0, 0, false};
#define DOUBLE_TAP_TERM 300

// ── Init ──────────────────────────────────────────────────────────────────────
void keyboard_post_init_user(void) {
    load_os_profile();
    display_set_profile(current_os, os_profile_names[current_os]);
}

// Track last pressed key for display
uint16_t last_pressed_keycode = KC_NO;

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

    // ┌──────────────────────────────────────────────────────────────┐
    // │  Layer 0: MEDIA                                              │
    // │                              ┌────────┐                      │
    // │                              │ [ENC]  │ rotate=Vol±           │
    // │                              │  Mute  │ press=Mute (GP9,     │
    // │                              │        │  handled in fw)      │
    // │   ┌────────┬────────┬────────┼────────┤                      │
    // │   │ LYRSW  │  Bri+  │  Bri-  │  ---   │                      │
    // │   ├────────┼────────┼────────┼────────┤                      │
    // │   │ PRFSW  │  Vol-  │  Mute  │  Vol+  │                      │
    // │   ├────────┼────────┼────────┼────────┤                      │
    // │   │  ---   │  Prev  │  Play  │  Next  │                      │
    // │   └────────┴────────┴────────┴────────┘                      │
    // │  --- = KC_TRNS (transparent, configure freely)               │
    // └──────────────────────────────────────────────────────────────┘
    [LAYER_MEDIA] = LAYOUT(
        KC_LYRSW, KC_BRIU,  KC_BRID,  KC_TRNS,
        KC_PRFSW, KC_VOLD,  KC_MUTE,  KC_VOLU,
        KC_TRNS,  KC_MPRV,  KC_MPLY,  KC_MNXT
    ),

    // ┌──────────────────────────────────────────────────────────────┐
    // │  Layer 1: APP                                                │
    // │                              ┌────────┐                      │
    // │                              │ [ENC]  │ rotate=Vol±           │
    // │                              │  Mute  │ press=Mute (GP9,     │
    // │                              │        │  handled in fw)      │
    // │   ┌────────┬────────┬────────┼────────┤                      │
    // │   │ LYRSW  │ App 1  │ App 2  │ App 6  │                      │
    // │   ├────────┼────────┼────────┼────────┤                      │
    // │   │ PRFSW  │ App 3  │ App 4  │ App 5  │                      │
    // │   ├────────┼────────┼────────┼────────┤                      │
    // │   │  ---   │ App 7  │ App 8  │ App 9  │                      │
    // │   └────────┴────────┴────────┴────────┘                      │
    // │                                                               │
    // │  All profiles send HYPER chords (Ctrl+Shift+Alt+Gui):        │
    // │    App1=Hyper+A  App3=Hyper+C  App5=Hyper+E                  │
    // │    App2=Hyper+B  App4=Hyper+D  App6=Hyper+F                  │
    // │                               App7=Hyper+G                   │
    // │                               App8=Hyper+H  App9=Hyper+I     │
    // │                                                               │
    // │  To remap a slot, edit app_key() near the top of file.       │
    // └──────────────────────────────────────────────────────────────┘
    [LAYER_APP] = LAYOUT(
        KC_LYRSW, KC_APP0,  KC_APP1,  KC_APP5,
        KC_PRFSW, KC_APP2,  KC_APP3,  KC_APP4,
        KC_TRNS,  KC_APP6,  KC_APP7,  KC_APP8
    ),

    // ┌──────────────────────────────────────────────────────────────┐
    // │  Layer 2: MACRO                                              │
    // │                              ┌────────┐                      │
    // │                              │ [ENC]  │ rotate=Vol±           │
    // │                              │  Mute  │ press=Mute (GP9,     │
    // │                              │        │  handled in fw)      │
    // │   ┌────────┬────────┬────────┼────────┤                      │
    // │   │ LYRSW  │ Macro1 │ Macro2 │ Macro6 │                      │
    // │   ├────────┼────────┼────────┼────────┤                      │
    // │   │ PRFSW  │ Macro3 │ Macro4 │ Macro5 │                      │
    // │   ├────────┼────────┼────────┼────────┤                      │
    // │   │  ---   │ Macro7 │ Macro8 │ Macro9 │                      │
    // │   └────────┴────────┴────────┴────────┘                      │
    // │                                                               │
    // │  Sends F13–F21 (safe, unbound on all OSes by default).       │
    // │    Macro1=F13  Macro3=F15  Macro5=F17  Macro7=F19  Macro9=F21│
    // │    Macro2=F14  Macro4=F16  Macro6=F18  Macro8=F20            │
    // │  Bind in your OS:                                             │
    // │    macOS   → Karabiner / BetterTouchTool                      │
    // │    Linux   → sxhkd / xbindkeys / KDE shortcuts               │
    // │    Windows → AutoHotKey  (F13::Run, "app.exe")               │
    // └──────────────────────────────────────────────────────────────┘
    [LAYER_MACRO] = LAYOUT(
        KC_LYRSW, KC_F13,   KC_F14,   KC_F18,
        KC_PRFSW, KC_F15,   KC_F16,   KC_F17,
        KC_TRNS,  KC_F19,   KC_F20,   KC_F21
    ),
};
// clang-format on

// ┌──────────────────────────────────────────────────────────┐
// │  ENCODER MAP  (rotation only — all layers)               │
// │                                                          │
// │   Rotate CCW ──► KC_VOLD  (Volume Down)                 │
// │   Rotate  CW ──► KC_VOLU  (Volume Up)                   │
// │                                                          │
// │   Push / SW  ──► KC_MUTE  (handled as DIRECT_PIN key,   │
// │                             GP9, defined in each layer)  │
// └──────────────────────────────────────────────────────────┘
#if defined(ENCODER_MAP_ENABLE)
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [LAYER_MEDIA] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [LAYER_APP]   = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [LAYER_MACRO] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
};
#endif

// ── Double-tap + special key logic ───────────────────────────────────────────

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // ── Layer switch (no double-tap needed) ──────────────────────────────────
    if (keycode == KC_LYRSW) {
        if (record->event.pressed) {
            uint8_t next = (get_highest_layer(layer_state) + 1) % LAYER_COUNT;
            layer_clear();
            layer_on(next);
            display_set_layer(next);
        }
        return false;
    }

    // ── OS profile switch (no double-tap needed) ─────────────────────────────
    if (keycode == KC_PRFSW) {
        if (record->event.pressed) {
            current_os = (os_profile_t)((current_os + 1) % OS_COUNT);
            save_os_profile();
            display_set_profile(current_os, os_profile_names[current_os]);
        }
        return false;
    }

    // ── App keys: resolve actual keycode from current OS profile ─────────────
    if (keycode >= KC_APP0 && keycode <= KC_APP9) {
        if (record->event.pressed) {
            uint8_t slot = keycode - KC_APP0;
            uint16_t real_kc = app_key(slot);

            // Show on display (single tap preview)
            if (tap_state.pending && keycode == tap_state.keycode &&
                timer_elapsed(tap_state.timer) < DOUBLE_TAP_TERM) {
                tap_state.pending = false;
                display_set_key_info(real_kc, true);
                register_code16(real_kc);
                return false;
            }
            tap_state.keycode = keycode;
            tap_state.timer   = timer_read();
            tap_state.pending = true;
            display_set_key_info(real_kc, false);
        } else {
            // Release: if we sent it, release it
            uint16_t real_kc = app_key(keycode - KC_APP0);
            unregister_code16(real_kc);
        }
        return false;
    }

    // ── All other keys: double-tap to send ───────────────────────────────────
    if (record->event.pressed) {
        if (tap_state.pending && keycode == tap_state.keycode &&
            timer_elapsed(tap_state.timer) < DOUBLE_TAP_TERM) {
            tap_state.pending = false;
            display_set_key_info(keycode, true);
            return true;
        }

        // First tap: show info, arm double-tap, swallow
        tap_state.keycode    = keycode;
        tap_state.timer      = timer_read();
        tap_state.pending    = true;
        last_pressed_keycode = keycode;
        display_set_key_info(keycode, false);
        return false;
    }

    return true;
}

void matrix_scan_user(void) {
    if (tap_state.pending && timer_elapsed(tap_state.timer) > DOUBLE_TAP_TERM) {
        tap_state.pending = false;
    }
}
