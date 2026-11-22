#include "touch.h"
#include "config.h"
#include "spi.h"
#include "tft.h"
#include <avr/io.h>

#define CS_LOW()  (PORTD &= ~(1 << TOUCH_CS))
#define CS_HIGH() (PORTD |=  (1 << TOUCH_CS))

// Control byte: start bit, channel, 12-bit mode, differential, power down
// between conversions.
#define CMD_X  0xD0
#define CMD_Y  0x90
#define CMD_Z1 0xB0
#define CMD_Z2 0xC0

// The calibration crosses on the touch test screen
#define CROSS_X0 20
#define CROSS_Y0 20
#define CROSS_X1 (TFT_W - 21)
#define CROSS_Y1 (TFT_H - 21)

uint16_t touch_raw_x = 0, touch_raw_y = 0, touch_pressure = 0;

void touch_init() {
    DDRD |= (1 << TOUCH_CS);
    CS_HIGH();
}

// One conversion: send the control byte; the 12-bit result comes back,
// left-aligned, in the next two bytes.
static uint16_t conversion(uint8_t cmd) {
    spi_transfer(cmd);
    const uint8_t hi = spi_transfer(0);
    const uint8_t lo = spi_transfer(0);
    return (((uint16_t)hi << 8) | lo) >> 3 & 0x0FFF;
}

static uint16_t median3(uint16_t a, uint16_t b, uint16_t c) {
    if (a > b) { uint16_t t = a; a = b; b = t; }
    if (b > c) b = c;
    return a > b ? a : b;
}

// Map a raw reading to the screen, using the raw values at the two crosses.
// A range that runs backwards (raw0 > raw1) flips the axis by itself.
static int16_t to_screen(uint16_t raw, int16_t raw0, int16_t raw1, int16_t s0, int16_t s1, int16_t limit) {
    int32_t s = s0 + (int32_t)((int16_t)raw - raw0) * (s1 - s0) / (raw1 - raw0);
    if (s < 0) s = 0;
    if (s > limit - 1) s = limit - 1;
    return (int16_t)s;
}

uint8_t touch_read(int16_t *x, int16_t *y) {
    // The XPT2046 takes at most 2.5 MHz: drop to 1 MHz while talking to it
    const uint8_t spcr = SPCR, spsr = SPSR;
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0);   // clk/16
    SPSR = 0;
    CS_LOW();
    const uint16_t z1 = conversion(CMD_Z1);
    const uint16_t z2 = conversion(CMD_Z2);
    uint16_t a[3], b[3];
    for (uint8_t i = 0; i < 3; i++) {                 // three samples each, keep the middle one
        a[i] = conversion(CMD_X);
        b[i] = conversion(CMD_Y);
    }
    CS_HIGH();
    SPCR = spcr;                                      // back to full speed for the screen
    SPSR = spsr;

    uint16_t rx = median3(a[0], a[1], a[2]);
    uint16_t ry = median3(b[0], b[1], b[2]);
    if (TOUCH_SWAP_XY) { uint16_t t = rx; rx = ry; ry = t; }
    touch_raw_x = rx;
    touch_raw_y = ry;
    touch_pressure = z1 + 4095 - z2;

    // Nothing pressing (or nothing wired, which reads all 0s or all 1s)
    if (z1 < 50 || z2 > 4050 || touch_pressure < TOUCH_PRESSURE) return 0;
    if (rx < 50 || rx > 4050 || ry < 50 || ry > 4050) return 0;

    *x = to_screen(rx, TOUCH_X_LEFT, TOUCH_X_RIGHT, CROSS_X0, CROSS_X1, TFT_W);
    *y = to_screen(ry, TOUCH_Y_TOP, TOUCH_Y_BOTTOM, CROSS_Y0, CROSS_Y1, TFT_H);
    return 1;
}
