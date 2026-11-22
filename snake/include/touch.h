#pragma once
#include <stdint.h>

// XPT2046 resistive touch controller. It shares the TFT's SPI bus and has
// its own chip select (TOUCH_CS in config.h).
void touch_init();

// Read the panel: 1 while it is pressed, with the screen position in x, y.
uint8_t touch_read(int16_t *x, int16_t *y);

// The raw readings behind the last touch_read(), for the touch test screen.
// RAW X / RAW Y are after TOUCH_SWAP_XY; PRESSURE grows as you press harder.
extern uint16_t touch_raw_x, touch_raw_y, touch_pressure;
