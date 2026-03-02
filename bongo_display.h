// Copyright 2024 Kyran Hoang
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>
#include <stdbool.h>

// Called once from keyboard_post_init_user()
void display_init(void);

// Called every cycle from housekeeping_task_user()
void display_task(void);

// Update which layer is active (called from process_record_user on LYRSW)
void display_set_layer(uint8_t layer);

// Update the key-info panel.
// confirmed=true  → double-tap fired, show "→ <name>"
// confirmed=false → single tap preview, show "<name>"
void display_set_key_info(uint16_t keycode, bool confirmed);

// Update the active OS profile shown in the key-info panel.
// profile is the os_profile_t index; name is the display string.
void display_set_profile(uint8_t profile, const char *name);
