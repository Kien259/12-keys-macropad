import time
import board
import busio
import digitalio
import usb_hid
import displayio
import fourwire
import terminalio
import rotaryio

from adafruit_hid.keyboard import Keyboard
from adafruit_hid.keycode import Keycode
from adafruit_hid.consumer_control import ConsumerControl
from adafruit_hid.consumer_control_code import ConsumerControlCode
from adafruit_display_text import label
import adafruit_st7789

# =============================================================================
# PIN CONFIG
# =============================================================================

COL_PINS = (board.GP0, board.GP1, board.GP2, board.GP3)
ROW_PINS = (board.GP4, board.GP5, board.GP6)

ENC_CLK = board.GP27
ENC_DT  = board.GP28
ENC_SW  = board.GP29

LCD_SCK = board.GP10
LCD_MOSI = board.GP11
LCD_CS = board.GP13
LCD_DC = board.GP14
LCD_RST = board.GP15
LCD_BL = board.GP26

# =============================================================================
# CONSTANTS
# =============================================================================

NUM_ROWS = len(ROW_PINS)
NUM_COLS = len(COL_PINS)

ARM_TIMEOUT_MS = 2000
KEY_INFO_SHOW_MS = 3000

SCREEN_W = 320
SCREEN_H = 172

# Colors
BLACK = 0x000000
WHITE = 0xFFFFFF
GRAY = 0x555555
CYAN = 0x00FFFF
YELLOW = 0xFFFF00
GREEN = 0x00FF00
DIVIDER_COLOR = 0x333333

PROFILES = ("macOS", "Windows", "Linux")
LAYOUT_NAMES = ("Media & Sys", "Hyper Apps", "F-Keys")

# =============================================================================
# HID
# =============================================================================

kbd = Keyboard(usb_hid.devices)
cc = ConsumerControl(usb_hid.devices)

# =============================================================================
# MATRIX SETUP
# =============================================================================

cols = []
for pin in COL_PINS:
    d = digitalio.DigitalInOut(pin)
    d.direction = digitalio.Direction.OUTPUT
    d.value = True
    cols.append(d)

rows = []
for pin in ROW_PINS:
    d = digitalio.DigitalInOut(pin)
    d.direction = digitalio.Direction.INPUT
    d.pull = digitalio.Pull.UP
    rows.append(d)

key_prev = [[True]*NUM_COLS for _ in range(NUM_ROWS)]

# =============================================================================
# ENCODER
# =============================================================================

# Rotary encoder
encoder = rotaryio.IncrementalEncoder(board.GP27, board.GP28)
enc_last_position = encoder.position

# Encoder button
enc_sw = digitalio.DigitalInOut(board.GP29)
enc_sw.direction = digitalio.Direction.INPUT
enc_sw.pull = digitalio.Pull.UP

enc_sw_last = True

# =============================================================================
# DISPLAY
# =============================================================================

displayio.release_displays()

spi = busio.SPI(clock=LCD_SCK, MOSI=LCD_MOSI)

display_bus = fourwire.FourWire(
    spi,
    command=LCD_DC,
    chip_select=LCD_CS,
    reset=LCD_RST
)

display = adafruit_st7789.ST7789(
    display_bus,
    width=SCREEN_W,
    height=SCREEN_H,
    rotation=270,
    rowstart=0,
    colstart=34
)

bl = digitalio.DigitalInOut(LCD_BL)
bl.direction = digitalio.Direction.OUTPUT
bl.value = True

# =============================================================================
# GLOBAL STATE
# =============================================================================

current_layer = 0
current_profile = 1

armed_row = -1
armed_col = -1
armed_time = 0

last_key_label = ""
key_info_time = 0
last_keypress_time = 0

# =============================================================================
# KEY DEFINITIONS
# =============================================================================

HYPER_MODS = (Keycode.CONTROL, Keycode.SHIFT, Keycode.ALT, Keycode.GUI)

HYPER_LETTERS = (
    Keycode.A, Keycode.B, Keycode.C,
    Keycode.D, Keycode.E, Keycode.F,
    Keycode.G, Keycode.H, Keycode.I,
)

KEY_LABELS = [
[
["ESC","Lock Scrn","Restart","Shutdown"],
["Layer","Prev","Play/Pause","Next"],
["Profile","TBD","TBD","TBD"],
],
[
["ESC","Hyper+A","Hyper+B","Hyper+C"],
["Layer","Hyper+D","Hyper+E","Hyper+F"],
["Profile","Hyper+G","Hyper+H","Hyper+I"],
],
[
["ESC","F13","F14","F15"],
["Layer","F16","F17","F18"],
["Profile","F19","F20","F21"],
]
]

# =============================================================================
# TIME
# =============================================================================

def now_ms():
    return time.monotonic_ns() // 1_000_000

# =============================================================================
# KEY EXECUTION
# =============================================================================

def execute_key(row,col):

    if col == 0:
        return

    if current_layer == 0:
        exec_media(row,col)

    elif current_layer == 1:
        exec_hyper(row,col)

    elif current_layer == 2:
        exec_fkeys(row,col)


def exec_media(row,col):

    prof = current_profile

    if row == 0:

        if col == 1:

            if prof == 0:
                kbd.send(Keycode.CONTROL,Keycode.GUI,Keycode.Q)
            else:
                kbd.send(Keycode.GUI,Keycode.L)

        elif col == 2:

            if prof == 1:
                kbd.send(Keycode.GUI,Keycode.D)
                time.sleep(0.3)
                kbd.send(Keycode.ALT,Keycode.F4)
            else:
                kbd.send(Keycode.CONTROL,Keycode.ALT,Keycode.DELETE)

        elif col == 3:

            if prof == 1:
                kbd.send(Keycode.GUI,Keycode.D)
                time.sleep(0.3)
                kbd.send(Keycode.ALT,Keycode.F4)
            else:
                kbd.send(Keycode.CONTROL,Keycode.ALT,Keycode.DELETE)

    elif row == 1:

        if col == 1:
            cc.send(ConsumerControlCode.SCAN_PREVIOUS_TRACK)

        elif col == 2:
            cc.send(ConsumerControlCode.PLAY_PAUSE)

        elif col == 3:
            cc.send(ConsumerControlCode.SCAN_NEXT_TRACK)


def exec_hyper(row,col):

    idx = row*3 + (col-1)
    kbd.send(*HYPER_MODS, HYPER_LETTERS[idx])


def exec_fkeys(row,col):

    idx = row*3 + (col-1)
    kbd.send(Keycode.F13 + idx)

# =============================================================================
# MATRIX SCAN
# =============================================================================

def scan_matrix():

    global current_layer,current_profile
    global last_key_label,key_info_time
    global last_keypress_time
    global armed_row,armed_col,armed_time

    now = now_ms()

    for c in range(NUM_COLS):

        cols[c].value = False
        time.sleep(0.001)

        for r in range(NUM_ROWS):

            pressed = not rows[r].value
            was_pressed = not key_prev[r][c]

            if pressed and not was_pressed:

                last_keypress_time = now

                if r == 0 and c == 0:

                    armed_row = -1
                    armed_col = -1
                    last_key_label = "ESC"
                    key_info_time = now

                elif r == 1 and c == 0:

                    current_layer = (current_layer+1)%len(LAYOUT_NAMES)

                    armed_row = -1
                    armed_col = -1

                    last_key_label = KEY_LABELS[current_layer][r][c] + " ✓"
                    key_info_time = now

                elif r == 2 and c == 0:

                    current_profile = (current_profile+1)%len(PROFILES)

                    armed_row = -1
                    armed_col = -1

                    last_key_label = KEY_LABELS[current_layer][r][c] + " ✓"
                    key_info_time = now

                elif armed_row < 0:

                    armed_row = r
                    armed_col = c
                    armed_time = now

                    last_key_label = KEY_LABELS[current_layer][r][c]
                    key_info_time = now

                elif armed_row == r and armed_col == c and (now-armed_time)<=ARM_TIMEOUT_MS:

                    execute_key(r,c)

                    last_key_label = KEY_LABELS[current_layer][r][c] + " ✓"
                    key_info_time = now

                    armed_row = -1
                    armed_col = -1

                else:

                    armed_row = r
                    armed_col = c
                    armed_time = now

                    last_key_label = KEY_LABELS[current_layer][r][c]
                    key_info_time = now

            key_prev[r][c] = not pressed

        cols[c].value = True

# =============================================================================
# ENCODER (KY-040)
# =============================================================================

def read_encoder():

    global enc_last_position, enc_sw_last, last_keypress_time

    pos = encoder.position

    if pos > enc_last_position:
        cc.send(ConsumerControlCode.VOLUME_INCREMENT)
        last_keypress_time = now_ms()

    elif pos < enc_last_position:
        cc.send(ConsumerControlCode.VOLUME_DECREMENT)
        last_keypress_time = now_ms()

    enc_last_position = pos

    # Button (mute)
    sw = enc_sw.value

    if not sw and enc_sw_last:
        cc.send(ConsumerControlCode.MUTE)
        last_keypress_time = now_ms()

    enc_sw_last = sw

# =============================================================================
# DISPLAY UI
# =============================================================================

root = displayio.Group()

bg = displayio.Bitmap(SCREEN_W,SCREEN_H,1)
pal = displayio.Palette(1)
pal[0] = BLACK
root.append(displayio.TileGrid(bg,pixel_shader=pal))

DIV_Y = 86
VDIV_X = 130

div = displayio.Bitmap(SCREEN_W,2,1)
pal2 = displayio.Palette(1)
pal2[0] = DIVIDER_COLOR
root.append(displayio.TileGrid(div,pixel_shader=pal2,x=0,y=DIV_Y))

vdiv = displayio.Bitmap(2,DIV_Y,1)
root.append(displayio.TileGrid(vdiv,pixel_shader=pal2,x=VDIV_X,y=0))

FONT = terminalio.FONT

layout_labels=[]

for i in range(3):
    lbl=label.Label(FONT,text="",color=WHITE,x=4,y=14+i*24)
    root.append(lbl)
    layout_labels.append(lbl)

key_info_label=label.Label(FONT,text="",color=CYAN,x=VDIV_X+8,y=14)
root.append(key_info_label)

profile_label=label.Label(FONT,text="",color=YELLOW,x=VDIV_X+8,y=38)
root.append(profile_label)

status_label=label.Label(FONT,text="",color=GREEN,x=VDIV_X+8,y=62)
root.append(status_label)

bongo_label=label.Label(FONT,text="BONGO CAT",color=WHITE,x=100,y=DIV_Y+32)
root.append(bongo_label)

display.root_group=root

# =============================================================================
# DISPLAY UPDATE
# =============================================================================

def update_bongo():

    since = now_ms() - last_keypress_time

    if since < 200:
        bongo_label.color = 0xFFAAAA
    elif since < 400:
        bongo_label.color = WHITE
    else:
        bongo_label.color = 0x888888


def update_display():

    now = now_ms()

    for i,lbl in enumerate(layout_labels):

        arrow="> " if i==current_layer else "  "

        lbl.text=arrow+LAYOUT_NAMES[i]
        lbl.color=CYAN if i==current_layer else GRAY

    if last_key_label and (now-key_info_time)<KEY_INFO_SHOW_MS:
        key_info_label.text=last_key_label
    else:
        key_info_label.text=""

    profile_label.text=PROFILES[current_profile]
    status_label.text="Layer "+str(current_layer+1)

# =============================================================================
# MAIN LOOP
# =============================================================================

update_display()

while True:

    scan_matrix()
    read_encoder()
    update_bongo()
    update_display()

    time.sleep(0.005)