#pragma once
#include <stdint.h>

// XPT2046 resistive touch controller. It shares the TFT's SPI bus and has
// its own chip select (TOUCH_CS in config.h).
void touch_init();                    // also loads the saved calibration from EEPROM

// Read the panel: 1 while it is pressed, with the screen position in x, y.
uint8_t touch_read(int16_t *x, int16_t *y);

// The chip's own X and Y channels at the last press. Which one runs along
// the screen's width, and which way, depends on how the panel was glued on:
// calibration works that out.
extern uint16_t touch_raw_x, touch_raw_y;

// ---- Calibration ----
// The setup screen's three crosses, 20 px in from the corners:
// top-left, top-right, bottom-right
#define TOUCH_CAL_LEFT   20
#define TOUCH_CAL_RIGHT  459          // TFT_W - 21
#define TOUCH_CAL_TOP    20
#define TOUCH_CAL_BOTTOM 299          // TFT_H - 21

uint8_t touch_calibrated();           // 1 once a calibration has been saved

// Work out the calibration from the readings at the three crosses
// (raw[i][0] = touch_raw_x, raw[i][1] = touch_raw_y at cross i), use it and
// save it to EEPROM. Returns 0 and changes nothing if the readings don't
// add up (a cross missed, or touched twice).
uint8_t touch_calibrate(const uint16_t raw[3][2]);
