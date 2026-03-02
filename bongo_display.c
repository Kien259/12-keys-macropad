// Copyright 2024 Kyran Hoang
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Display layout (320×172, landscape):
//
//  x=0          x=160        x=320
//  ┌────────────┬────────────┐  y=0
//  │ Layer list │  Key info  │
//  │  (left)    │  (right)   │
//  ├────────────┴────────────┤  y=112
//  │       Bongo Cat         │
//  └─────────────────────────┘  y=172
//
// The horizontal divider is drawn at y=112.
// The vertical divider in the top half is at x=160.

#include "bongo_display.h"
#include QMK_KEYBOARD_H
#include "qp.h"
#include "qp_st7789.h"
#include "wpm.h"
#include "thintel15.qff.h"
#include <string.h>
#include <stdio.h>

// Both "small" and "medium" text use the same thintel15 font.
// font_noto_sans_8 / font_noto_sans_12 are loaded at runtime in display_init().
static painter_font_handle_t font_noto_sans_8;
static painter_font_handle_t font_noto_sans_12;

// ── Pin definitions (from config.h) ──────────────────────────────────────────
#ifndef DISPLAY_CS_PIN
#    define DISPLAY_CS_PIN   GP17
#endif
#ifndef DISPLAY_DC_PIN
#    define DISPLAY_DC_PIN   GP20
#endif
#ifndef DISPLAY_RST_PIN
#    define DISPLAY_RST_PIN  GP21
#endif
#ifndef DISPLAY_BL_PIN
#    define DISPLAY_BL_PIN   GP22
#endif

// ── Layout constants ──────────────────────────────────────────────────────────
#define SCREEN_W        320
#define SCREEN_H        172
#define DIVIDER_Y       112   // horizontal line separating top / bongo
#define DIVIDER_X       160   // vertical line separating layer list / key info
#define TOP_H           (DIVIDER_Y)
#define BONGO_H         (SCREEN_H - DIVIDER_Y)  // 60px tall bongo area
#define BONGO_W         SCREEN_W

// Colours
#define COL_BG          HSV_BLACK
#define COL_DIVIDER     0x38, 0x38, 0x38   // dark grey (H,S,V)
#define COL_TEXT        HSV_WHITE
#define COL_ACCENT      0x85, 0xFF, 0xFF   // cyan-ish
#define COL_ACTIVE_LYR  0x43, 0xFF, 0xFF   // green-ish
#define COL_INACTIVE    0x00, 0x00, 0x80   // dim white
#define COL_CONFIRMED   0x43, 0xFF, 0xFF   // green for confirmed key

// ── Layer names ───────────────────────────────────────────────────────────────
#define LAYER_COUNT 3
static const char *layer_names[LAYER_COUNT] = {
    "Media",
    "App",
    "Macro",
};

// ── Bongo cat animation data ──────────────────────────────────────────────────
// Original data is for a 128×32 OLED (512 bytes per frame, stored column-major
// in 8-pixel vertical bands).  We scale it to fit the 320×60 bongo area by
// rendering each source pixel as a 2×2 block and centering the 256×64 result.
//
// Source frame: 128 cols × 32 rows  (128 bytes/row-band × 4 bands = 512 bytes)
#define ANIM_COLS   128
#define ANIM_ROWS   32
#define ANIM_SIZE   (ANIM_COLS * (ANIM_ROWS / 8))  // 512

#define IDLE_FRAMES  5
#define TAP_FRAMES   2
#define IDLE_SPEED   10   // wpm threshold idle→prep
#define TAP_SPEED    40   // wpm threshold prep→tap
#define ANIM_FRAME_DURATION 100  // ms per frame

static const uint8_t PROGMEM idle[IDLE_FRAMES][ANIM_SIZE] = {
    {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,64,64,32,32,32,32,16,16,16,16,16,8,8,4,4,4,8,48,64,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,24,100,130,2,2,2,2,2,1,0,0,0,0,128,128,0,0,0,0,0,0,0,0,0,128,0,48,48,0,192,193,193,194,4,8,16,32,64,128,0,0,0,128,128,128,128,64,64,
        64,64,32,32,32,32,16,16,16,16,8,8,8,8,8,4,4,4,4,4,2,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,192,56,4,3,0,0,0,0,0,0,0,12,12,12,13,1,0,64,160,33,34,18,17,17,17,9,8,8,8,8,4,4,8,8,16,16,16,16,16,17,15,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,128,128,128,64,64,64,64,64,32,32,32,32,32,16,16,16,16,16,8,8,8,8,8,4,4,4,4,4,2,3,2,2,1,1,1,1,1,1,2,2,4,4,8,8,8,8,8,7,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    },
    {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,64,64,32,32,32,32,16,16,16,16,16,8,8,4,4,4,8,48,64,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,24,100,130,2,2,2,2,2,1,0,0,0,0,128,128,0,0,0,0,0,0,0,0,0,128,0,48,48,0,192,193,193,194,4,8,16,32,64,128,0,0,0,128,128,128,128,64,64,
        64,64,32,32,32,32,16,16,16,16,8,8,8,8,8,4,4,4,4,4,2,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,192,56,4,3,0,0,0,0,0,0,0,12,12,12,13,1,0,64,160,33,34,18,17,17,17,9,8,8,8,8,4,4,8,8,16,16,16,16,16,17,15,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,128,128,128,64,64,64,64,64,32,32,32,32,32,16,16,16,16,16,8,8,8,8,8,4,4,4,4,4,2,3,2,2,1,1,1,1,1,1,2,2,4,4,8,8,8,8,8,
        7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    },
    {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,64,64,64,64,32,32,32,32,16,8,4,2,2,4,24,96,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,194,1,1,2,2,4,4,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,96,96,0,129,130,130,132,8,16,32,64,128,0,0,0,0,128,128,128,128,64,64,64,64,32,
        32,32,32,16,16,16,16,8,8,8,8,8,4,4,4,4,4,2,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,112,25,6,0,0,0,0,0,0,0,24,24,24,27,3,0,64,160,34,36,20,18,18,18,11,8,8,8,8,5,5,9,9,16,16,16,16,16,17,15,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,128,128,128,64,64,64,64,64,32,32,32,32,32,16,16,16,16,16,8,8,8,8,8,4,4,4,4,4,2,3,2,2,1,1,1,1,1,1,2,2,4,4,8,8,8,8,8,7,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    },
    {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,0,0,0,0,0,128,64,64,32,32,32,32,16,16,16,16,8,4,2,1,1,2,12,48,64,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,30,225,0,0,1,1,2,2,1,0,0,0,0,128,128,0,0,0,0,0,0,0,0,0,128,0,48,48,0,192,193,193,194,4,8,16,32,64,128,0,0,0,128,128,128,128,64,64,
        64,64,32,32,32,32,16,16,16,16,8,8,8,8,8,4,4,4,4,4,2,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,112,12,3,0,0,0,0,0,0,0,12,12,12,13,1,0,64,160,33,34,18,17,17,17,9,8,8,8,8,4,4,8,8,16,16,16,16,16,17,15,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,128,128,128,64,64,64,64,64,32,32,32,32,32,16,16,16,16,16,8,8,8,8,8,4,4,4,4,4,2,3,2,2,1,1,1,1,1,1,2,2,4,4,8,8,8,8,8,
        7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    },
    {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,64,64,32,32,32,32,16,16,16,16,8,8,4,2,2,2,4,56,64,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,28,226,1,1,2,2,2,2,1,0,0,0,0,128,128,0,0,0,0,0,0,0,0,0,128,0,48,48,0,192,193,193,194,4,8,16,32,64,128,0,0,0,128,128,128,128,64,64,64,64,
        32,32,32,32,16,16,16,16,8,8,8,8,8,4,4,4,4,4,2,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,112,12,3,0,0,0,0,0,0,0,12,12,12,13,1,0,64,160,33,34,18,17,17,17,9,8,8,8,8,4,4,8,8,16,16,16,16,16,17,15,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,128,128,128,64,64,64,64,64,32,32,32,32,32,16,16,16,16,16,8,8,8,8,8,4,4,4,4,4,2,3,2,2,1,1,1,1,1,1,2,2,4,4,8,8,8,8,8,7,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    }
};

static const uint8_t PROGMEM prep[1][ANIM_SIZE] = {
    {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,0,0,0,0,0,128,64,64,32,32,32,32,16,16,16,16,8,4,2,1,1,2,12,48,64,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,30,225,0,0,1,1,2,2,129,128,128,0,0,128,128,0,0,0,0,0,0,0,0,0,128,0,48,48,0,0,1,225,26,6,9,49,53,1,138,124,0,0,128,128,128,128,64,64,
        64,64,32,32,32,32,16,16,16,16,8,8,8,8,8,4,4,4,4,4,2,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,112,12,3,0,0,24,6,5,152,153,132,195,124,65,65,64,64,32,33,34,18,17,17,17,9,8,8,8,8,4,4,4,4,4,4,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,128,128,128,64,64,64,64,64,32,32,32,32,32,16,16,16,16,16,8,8,8,8,8,4,4,4,4,4,2,3,2,2,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    }
};

static const uint8_t PROGMEM tap_anim[TAP_FRAMES][ANIM_SIZE] = {
    {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,0,0,0,0,0,128,64,64,32,32,32,32,16,16,16,16,8,4,2,1,1,2,12,48,64,128,0,0,0,0,0,0,0,248,248,248,248,0,0,0,0,0,128,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,30,225,0,0,1,1,2,2,129,128,128,0,0,128,128,0,0,0,0,0,0,0,0,0,128,0,48,48,0,0,1,1,2,4,8,16,32,67,135,7,1,0,184,188,190,159,
        95,95,79,76,32,32,32,32,16,16,16,16,8,8,8,8,8,4,4,4,4,4,2,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,112,12,3,0,0,24,6,5,152,153,132,67,124,65,65,64,64,32,33,34,18,17,17,17,9,8,8,8,8,4,4,8,8,16,16,16,16,16,17,15,1,61,124,252,252,252,252,252,60,12,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,128,128,128,64,64,64,64,64,32,32,32,32,32,16,16,16,16,16,8,8,8,8,8,4,4,4,4,4,2,3,2,2,1,1,1,
        1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,3,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    },
    {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,0,0,0,0,0,128,64,64,32,32,32,32,16,16,16,16,8,4,2,1,1,2,12,48,64,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,30,225,0,0,1,1,2,2,1,0,0,0,0,128,128,0,0,0,0,0,0,0,0,0,128,0,48,48,0,0,1,225,26,6,9,49,53,1,138,124,0,0,128,128,128,128,64,64,64,64,32,
        32,32,32,16,16,16,16,8,8,8,8,8,4,4,4,4,4,2,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,112,12,3,0,0,0,0,0,0,0,0,0,0,1,1,0,64,160,33,34,18,17,17,17,9,8,8,8,8,4,4,4,4,4,4,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,128,128,128,128,64,64,64,64,64,32,32,32,32,32,16,16,16,16,16,8,8,8,8,8,4,4,4,4,4,2,3,122,122,121,121,121,121,57,49,2,2,4,4,8,8,8,136,136,135,128,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    }
};

// ── State ─────────────────────────────────────────────────────────────────────
static painter_device_t display;
static bool             display_ready = false;

static uint8_t  current_layer      = 0;
static uint16_t last_keycode       = KC_NO;
static bool     key_confirmed      = false;
static bool     info_dirty         = true;
static bool     layer_dirty        = true;

// OS profile shown in key-info panel
static uint8_t current_profile     = 0;
static char    profile_name[12]    = "macOS";

// Bongo animation state
static uint8_t  current_idle_frame = 0;
static uint8_t  current_tap_frame  = 0;
static uint32_t anim_timer         = 0;
static bool     bongo_dirty        = true;

// ── Helpers ───────────────────────────────────────────────────────────────────

// Draw the 128×32 monochrome OLED-style frame into the bongo area.
// Source bytes are column-major, 8 rows per byte (LSB = top row).
// We scale 2× to fill ~256×64 and center in the 320×60 area.
static void draw_bongo_frame(const uint8_t *frame) {
    // Bongo area: x=[0..319], y=[DIVIDER_Y..SCREEN_H-1]
    // Scaled size: 256×64.  Offset to center: x_off=32, y_off=-2 (clip top 2px)
    const int16_t x_off = (BONGO_W - ANIM_COLS * 2) / 2;  // 32
    const int16_t y_off = DIVIDER_Y + (BONGO_H - ANIM_ROWS * 2) / 2;

    for (uint8_t col = 0; col < ANIM_COLS; col++) {
        for (uint8_t band = 0; band < (ANIM_ROWS / 8); band++) {
            uint8_t byte = pgm_read_byte(&frame[col + band * ANIM_COLS]);
            for (uint8_t bit = 0; bit < 8; bit++) {
                uint8_t row = band * 8 + bit;
                bool    on  = (byte >> bit) & 1;
                int16_t px  = x_off + col * 2;
                int16_t py  = y_off + row * 2;
                if (px < 0 || px + 1 >= SCREEN_W) continue;
                if (py < DIVIDER_Y || py + 1 >= SCREEN_H) continue;
                HSV colour = on ? (HSV){HSV_WHITE} : (HSV){HSV_BLACK};
                qp_rect(display, px, py, px + 1, py + 1, colour.h, colour.s, colour.v, true);
            }
        }
    }
}

static void draw_dividers(void) {
    // Horizontal divider
    qp_line(display, 0, DIVIDER_Y, SCREEN_W - 1, DIVIDER_Y, 0, 0, 80);
    // Vertical divider in top half
    qp_line(display, DIVIDER_X, 0, DIVIDER_X, DIVIDER_Y - 1, 0, 0, 80);
}

// Key name lookup for display panel.
// App keys are resolved to their Hyper chord before reaching here,
// so we match on the actual sent keycode (LCTL+LSFT+LALT+LGUI+letter).
#define HYPER(kc) LCTL(LSFT(LALT(LGUI(kc))))

static const char *keycode_name(uint16_t kc) {
    switch (kc) {
        // Media layer
        case KC_MPRV:  return "Prev";
        case KC_MNXT:  return "Next";
        case KC_MPLY:  return "Play/Pause";
        case KC_MSTP:  return "Stop";
        case KC_MUTE:  return "Mute";
        case KC_VOLU:  return "Vol +";
        case KC_VOLD:  return "Vol -";
        case KC_BRIU:  return "Bright +";
        case KC_BRID:  return "Bright -";
        // App layer (Hyper chords — Ctrl+Shift+Alt+Gui+letter)
        case HYPER(KC_A): return "App 1";
        case HYPER(KC_B): return "App 2";
        case HYPER(KC_C): return "App 3";
        case HYPER(KC_D): return "App 4";
        case HYPER(KC_E): return "App 5";
        case HYPER(KC_F): return "App 6";
        case HYPER(KC_G): return "App 7";
        case HYPER(KC_H): return "App 8";
        case HYPER(KC_I): return "App 9";
        // Macro layer (F13–F21)
        case KC_F13:   return "Macro 1";
        case KC_F14:   return "Macro 2";
        case KC_F15:   return "Macro 3";
        case KC_F16:   return "Macro 4";
        case KC_F17:   return "Macro 5";
        case KC_F18:   return "Macro 6";
        case KC_F19:   return "Macro 7";
        case KC_F20:   return "Macro 8";
        case KC_F21:   return "Macro 9";
        case KC_TRNS:  return "(pass)";
        case KC_NO:    return "---";
        default:       return "?";
    }
}

// qp_drawtext_recolor(device, x, y, font, str, hue_fg, sat_fg, val_fg, hue_bg, sat_bg, val_bg)
// bg is always black (0,0,0) throughout.
#define DRAW_TEXT(x, y, font, fh, fs, fv, str) \
    qp_drawtext_recolor(display, (x), (y), (font), (str), (fh), (fs), (fv), 0, 0, 0)

static void redraw_layer_panel(void) {
    qp_rect(display, 0, 0, DIVIDER_X - 1, DIVIDER_Y - 1, 0, 0, 0, true);

    DRAW_TEXT(4, 4, font_noto_sans_8, 0, 0, 200, "LAYERS");

    uint8_t active = get_highest_layer(layer_state);
    for (uint8_t i = 0; i < LAYER_COUNT; i++) {
        uint8_t y = 20 + i * 28;
        bool    is_active = (i == active);

        DRAW_TEXT(4, y, font_noto_sans_12,
                  is_active ? 43 : 0,
                  is_active ? 255 : 0,
                  is_active ? 255 : 60,
                  is_active ? ">" : " ");

        DRAW_TEXT(18, y, font_noto_sans_12,
                  is_active ? 43 : 0,
                  is_active ? 255 : 0,
                  is_active ? 255 : 120,
                  layer_names[i]);
    }
}

static void redraw_key_info_panel(void) {
    qp_rect(display, DIVIDER_X + 1, 0, SCREEN_W - 1, DIVIDER_Y - 1, 0, 0, 0, true);

    DRAW_TEXT(DIVIDER_X + 4, 4, font_noto_sans_8, 0, 0, 200, "KEY INFO");

    // OS profile badge — colour per OS: macOS=cyan, Linux=orange, Windows=blue
    static const uint8_t profile_h[3] = { 128, 21, 170 };
    static const uint8_t profile_s[3] = { 255, 255, 255 };
    DRAW_TEXT(DIVIDER_X + 4, 92, font_noto_sans_8,
              profile_h[current_profile % 3],
              profile_s[current_profile % 3],
              200,
              profile_name);

    if (last_keycode == KC_NO) {
        DRAW_TEXT(DIVIDER_X + 4, 20, font_noto_sans_12, 0, 0, 80, "Press a key");
        return;
    }

    const char *name = keycode_name(last_keycode);

    // Lock / modifier status line
    led_t leds = host_keyboard_led_state();
    char  status[24] = {0};
    snprintf(status, sizeof(status), "%s%s",
             leds.caps_lock ? "CAP " : "",
             leds.num_lock  ? "NUM"  : "");
    DRAW_TEXT(DIVIDER_X + 4, 20, font_noto_sans_8, 0, 0, 160, status);

    // Key name — green when confirmed (double-tapped), white on first tap
    char label[32];
    snprintf(label, sizeof(label), key_confirmed ? "> %s" : "%s", name);
    DRAW_TEXT(DIVIDER_X + 4, 40, font_noto_sans_12,
              key_confirmed ? 43 : 0,
              key_confirmed ? 255 : 0,
              255,
              label);

    if (!key_confirmed) {
        DRAW_TEXT(DIVIDER_X + 4, 60, font_noto_sans_8, 0, 0, 120, "dbl-tap to send");
    }
}

static void redraw_bongo(void) {
    uint8_t wpm = get_current_wpm();
    const uint8_t *frame;

    if (wpm <= IDLE_SPEED) {
        current_idle_frame = (current_idle_frame + 1) % IDLE_FRAMES;
        frame = idle[current_idle_frame];
    } else if (wpm < TAP_SPEED) {
        frame = prep[0];
    } else {
        current_tap_frame = (current_tap_frame + 1) % TAP_FRAMES;
        frame = tap_anim[current_tap_frame];
    }

    draw_bongo_frame(frame);
}

// ── Public API ────────────────────────────────────────────────────────────────

void display_init(void) {
    // Load font (thintel15 used for both sizes)
    font_noto_sans_8  = qp_load_font_mem(font_thintel15);
    font_noto_sans_12 = qp_load_font_mem(font_thintel15);

    // Backlight on
    gpio_set_pin_output(DISPLAY_BL_PIN);
    gpio_write_pin_high(DISPLAY_BL_PIN);

    display = qp_st7789_make_spi_device(
        DISPLAY_WIDTH, DISPLAY_HEIGHT,
        DISPLAY_CS_PIN, DISPLAY_DC_PIN, DISPLAY_RST_PIN,
        DISPLAY_SPI_DIVISOR, DISPLAY_SPI_MODE
    );

    if (!qp_init(display, QP_ROTATION_90)) return;

    // The 172×320 panel has a 34-pixel column offset in portrait mode.
    // After 90° rotation (landscape 320×172) the offset shifts to rows.
    qp_set_viewport_offsets(display, 0, 34);

    display_ready = true;

    qp_rect(display, 0, 0, SCREEN_W - 1, SCREEN_H - 1, 0, 0, 0, true);
    draw_dividers();
    redraw_layer_panel();
    redraw_key_info_panel();
    anim_timer = timer_read32();
}

void display_task(void) {
    if (!display_ready) return;

    if (layer_dirty) {
        redraw_layer_panel();
        draw_dividers();
        layer_dirty = false;
    }

    if (info_dirty) {
        redraw_key_info_panel();
        draw_dividers();
        info_dirty = false;
    }

    if (timer_elapsed32(anim_timer) > ANIM_FRAME_DURATION) {
        anim_timer = timer_read32();
        redraw_bongo();
        bongo_dirty = false;
    }

    qp_flush(display);
}

void display_set_layer(uint8_t layer) {
    current_layer = layer;
    layer_dirty   = true;
}

void display_set_key_info(uint16_t keycode, bool confirmed) {
    last_keycode   = keycode;
    key_confirmed  = confirmed;
    info_dirty     = true;
}

void display_set_profile(uint8_t profile, const char *name) {
    current_profile = profile;
    strncpy(profile_name, name, sizeof(profile_name) - 1);
    profile_name[sizeof(profile_name) - 1] = '\0';
    info_dirty = true;
}
