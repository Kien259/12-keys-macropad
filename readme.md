# bongo_macro

A hand-wired 3×4 macro pad built on an RP2040-Zero with an EC11 rotary encoder
and a 1.47″ 172×320 full-colour IPS TFT (ST7789 controller).

---

## Wiring guide

### RP2040-Zero pinout used

```
RP2040-Zero GPIO  │ Connected to
──────────────────┼──────────────────────────────────────────────
GP0               │ Matrix ROW 0  (top row)
GP1               │ Matrix ROW 1  (middle row)
GP2               │ Matrix ROW 2  (bottom row)
GP3               │ Matrix COL 0  (leftmost column)
GP4               │ Matrix COL 1
GP5               │ Matrix COL 2
GP6               │ Matrix COL 3  (rightmost column)
GP7               │ Encoder CLK  (EC11 pin A)
GP8               │ Encoder DT   (EC11 pin B)
GP9               │ Encoder SW   (EC11 push-button) → also to GND via switch
GP17              │ TFT CS
GP18              │ TFT SCL  (SPI0 SCK)
GP19              │ TFT SDA  (SPI0 MOSI)
GP20              │ TFT DC
GP21              │ TFT RES
GP22              │ TFT BL   (backlight, driven HIGH)
3V3               │ TFT VDD
GND               │ TFT GND, Encoder GND, Matrix diode cathode rail
```

### Switch matrix (COL2ROW diodes)

Wire each switch between a ROW and a COL pin.  Place a 1N4148 diode in series
with each switch, cathode toward the ROW pin (stripe toward ROW).

```
        COL0(GP3) COL1(GP4) COL2(GP5) COL3(GP6)
ROW0(GP0)  K00       K01       K02       K03
ROW1(GP1)  K10       K11       K12       K13
ROW2(GP2)  K20       K21       K22  [LYRSW K23]
```

K23 (bottom-right) is the **layout-switch key** (`KC_LYRSW`).

### EC11 Rotary Encoder

| EC11 pin | Wire to       |
|----------|---------------|
| CLK      | GP7           |
| DT       | GP8           |
| SW       | GP9           |
| +        | 3V3           |
| GND      | GND           |

The encoder SW pin also needs a pull-up; the RP2040 internal pull-up is enabled
automatically by QMK when `encoder: true` is set.

### 1.47″ SPI TFT (ST7789, 172×320)

| TFT pin | Wire to  |
|---------|----------|
| GND     | GND      |
| VDD     | 3V3      |
| SCL     | GP18     |
| SDA     | GP19     |
| RES     | GP21     |
| DC      | GP20     |
| CS      | GP17     |
| BL      | GP22     |

---

## Display layout

```
 ┌────────────────┬────────────────┐
 │ > Numpad       │                │
 │   Media        │   Key Info     │  ← top 112 px
 │   Nav          │                │
 ├────────────────┴────────────────┤
 │           Bongo Cat             │  ← bottom 60 px
 └─────────────────────────────────┘
```

- **Top-left**: layer list with `>` arrow on the active layer.
- **Top-right**: last pressed key name; turns green on double-tap (key fires).
- **Bottom**: animated bongo cat (idle / prep / typing based on WPM).

---

## Behaviour

| Action | Result |
|--------|--------|
| Rotate encoder | Volume down / up |
| Press encoder knob | Mute |
| Press any key (single) | Shows key name on display, does **not** send |
| Double-tap any key | Sends the key |
| Press `LYRSW` (K23) | Cycles Numpad → Media → Nav → Numpad |

---

## Building & flashing

### Prerequisites

Follow the [QMK setup guide](https://docs.qmk.fm/#/newbs_getting_started) to
install the QMK CLI and toolchain.

```bash
# From the qmk_firmware root
qmk setup
```

### Compile

```bash
qmk compile -kb bongo_macro -km default
```

This produces `bongo_macro_default.uf2`.

### Flash

1. Hold the **BOOT** button on the RP2040-Zero while plugging in USB.
2. A USB drive called `RPI-RP2` will appear.
3. Drag-and-drop `bongo_macro_default.uf2` onto that drive.
4. The board reboots automatically.

Alternatively, with the board already running QMK, hold the top-left key
(`K00`) while plugging in to trigger Bootmagic reset into the bootloader.

---

## Font assets

Quantum Painter requires compiled font files.  Generate them once:

```bash
# From qmk_firmware root
qmk painter-convert-font-image \
    --input /path/to/NotoSans-Regular.ttf \
    --size 8 \
    --output keyboards/bongo_macro/font_noto_sans_8.qff.h \
    --format pal2

qmk painter-convert-font-image \
    --input /path/to/NotoSans-Regular.ttf \
    --size 12 \
    --output keyboards/bongo_macro/font_noto_sans_12.qff.h \
    --format pal2
```

Then add to `rules.mk`:

```makefile
QUANTUM_PAINTER_ASSETS += keyboards/bongo_macro/font_noto_sans_8.qff.h
QUANTUM_PAINTER_ASSETS += keyboards/bongo_macro/font_noto_sans_12.qff.h
```

And declare them in a header:

```c
extern painter_font_handle_t font_noto_sans_8;
extern painter_font_handle_t font_noto_sans_12;
```
