// Copyright 2024 Kyran Hoang
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "bongo_display.h"

// ── Encoder push-button (KY-040 SW → GP9) ────────────────────────────────────
// The KY-040 has a 10kΩ pull-up on SW; pin reads HIGH at rest, LOW when pressed.
// We cannot use this as a matrix key (it's an active output, not a passive switch),
// so we poll it here and inject KC_MUTE on each press edge.
#define ENC_SW_PIN GP9

static bool enc_sw_last = true;  // HIGH = not pressed

void keyboard_post_init_kb(void) {
    setPinInputHigh(ENC_SW_PIN);  // enable internal pull-up as backup
    display_init();
    keyboard_post_init_user();
}

void housekeeping_task_kb(void) {
    bool enc_sw_now = readPin(ENC_SW_PIN);
    if (!enc_sw_now && enc_sw_last) {
        // falling edge = press
        tap_code(KC_MUTE);
    }
    enc_sw_last = enc_sw_now;

    display_task();
    housekeeping_task_user();
}
