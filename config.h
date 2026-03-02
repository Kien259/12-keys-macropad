// Copyright 2024 Kyran Hoang
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// ── Encoder switch (KY-040 SW pin → GP9) ─────────────────────────────────────
// The KY-040 module has a 10kΩ pull-up on SW already; no diode or matrix wiring
// needed. GP9 is read directly in housekeeping_task_kb() (bongo_macro.c).
// A falling edge (HIGH→LOW) triggers KC_MUTE via tap_code().

// ── Quantum Painter / ST7789 SPI TFT ─────────────────────────────────────────
// 1.47" 172x320 IPS TFT (ST7789 controller)
// SPI0 bus: SCK=GP18, MOSI(SDA)=GP19
// CS=GP17, DC=GP20, RST=GP21, BL=GP22
#define DISPLAY_CS_PIN   GP17
#define DISPLAY_DC_PIN   GP20
#define DISPLAY_RST_PIN  GP21
#define DISPLAY_BL_PIN   GP22

// Panel is 172 wide x 320 tall (portrait).
// We rotate 90° so it is 320 wide x 172 tall (landscape).
#define DISPLAY_WIDTH    320
#define DISPLAY_HEIGHT   172

// SPI divisor: 4 → 31.25 MHz on RP2040 (ST7789 max ~80 MHz)
#define DISPLAY_SPI_DIVISOR 4
#define DISPLAY_SPI_MODE    3

// Quantum Painter
#define QUANTUM_PAINTER_DISPLAY_TIMEOUT 0   // never sleep (we control it)
#define QUANTUM_PAINTER_TASK_THROTTLE   16  // ms between QP task calls

// Font / image assets compiled with qmk painter-convert-*
// Defined here so display code can reference them
#define DISPLAY_FONT_SMALL  font_noto_sans_8
#define DISPLAY_FONT_MEDIUM font_noto_sans_12

// ── Onboard WS2812 RGB LED (RP2040-Zero GP16) ────────────────────────────────
#define WS2812_DI_PIN GP16

// ── WPM (used by bongo cat animation) ────────────────────────────────────────
#define WPM_LAUNCH_CONTROL

// ── Encoder ───────────────────────────────────────────────────────────────────
#define ENCODER_RESOLUTION 4

// ── Bootmagic ─────────────────────────────────────────────────────────────────
// Hold top-left key (K00) while plugging in to enter bootloader
#define BOOTMAGIC_LITE_ROW 0
#define BOOTMAGIC_LITE_COLUMN 0
