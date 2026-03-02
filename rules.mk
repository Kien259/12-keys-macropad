# Quantum Painter (ST7789 over SPI)
QUANTUM_PAINTER_ENABLE = yes
QUANTUM_PAINTER_DRIVERS += st7789_spi

# WS2812 onboard LED (RP2040 requires vendor driver)
WS2812_DRIVER = vendor

# WPM counter (drives bongo cat animation speed)
WPM_ENABLE = yes

# Encoder map (layer-aware encoder actions)
ENCODER_MAP_ENABLE = yes

# SRC files
SRC += bongo_display.c
SRC += thintel15.qff.c
