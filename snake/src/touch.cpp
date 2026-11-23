#include "touch.h"
#include "config.h"
#include "spi.h"
#include "tft.h"
#include <avr/io.h>
#include <avr/eeprom.h>

#define CS_LOW()  (PORTD &= ~(1 << TOUCH_CS))
#define CS_HIGH() (PORTD |=  (1 << TOUCH_CS))

// Control byte: start bit, channel, 12-bit mode, differential, power down
// between conversions.
#define CMD_X  0xD0
#define CMD_Y  0x90
#define CMD_Z1 0xB0
#define CMD_Z2 0xC0

// The calibration in EEPROM, after the score table (layout in scores.h):
// a marker byte, the swap flag, then the four raw readings
#define EE_CAL        ((uint8_t *)16)
#define EE_CAL_MAGIC  0xA7
#define EE_CAL_SWAP   ((uint8_t *)17)
#define EE_CAL_VALUES 18

// The raw reading at each edge of the crosses. config.h's numbers until
// the screen has been calibrated.
enum { X_LEFT, X_RIGHT, Y_TOP, Y_BOTTOM };
static int16_t cal[4] = { TOUCH_X_LEFT, TOUCH_X_RIGHT, TOUCH_Y_TOP, TOUCH_Y_BOTTOM };
static uint8_t cal_swap = TOUCH_SWAP_XY;      // 1: the chip's Y channel runs along the screen's width
static uint8_t cal_saved = 0;

uint16_t touch_raw_x = 0, touch_raw_y = 0;

static uint16_t *ee_value(uint8_t i) { return (uint16_t *)(uintptr_t)(EE_CAL_VALUES + 2 * i); }

static int16_t span(int16_t a, int16_t b) { return a > b ? a - b : b - a; }

// Usable: every reading in the chip's 12-bit range, and each axis spanning
// a good part of it
static uint8_t sane(const int16_t c[4]) {
    for (uint8_t i = 0; i < 4; i++)
        if (c[i] < 0 || c[i] > 4095) return 0;
    return span(c[X_LEFT], c[X_RIGHT]) >= 1000 && span(c[Y_TOP], c[Y_BOTTOM]) >= 1000;
}

void touch_init() {
    DDRD |= (1 << TOUCH_CS);
    CS_HIGH();
    if (eeprom_read_byte(EE_CAL) != EE_CAL_MAGIC) return;     // never calibrated
    int16_t c[4];
    for (uint8_t i = 0; i < 4; i++) c[i] = (int16_t)eeprom_read_word(ee_value(i));
    if (!sane(c)) return;
    for (uint8_t i = 0; i < 4; i++) cal[i] = c[i];
    cal_swap = eeprom_read_byte(EE_CAL_SWAP) != 0;
    cal_saved = 1;
}

uint8_t touch_calibrated() { return cal_saved; }

uint8_t touch_calibrate(const uint16_t raw[3][2]) {
    // From the top-left cross to the top-right one the finger only moves
    // sideways: the channel that changed most runs along the screen's width.
    const uint8_t swap = span(raw[1][1], raw[0][1]) > span(raw[1][0], raw[0][0]);
    const uint8_t cx = swap ? 1 : 0;              // the channel for screen x
    const uint8_t cy = swap ? 0 : 1;              // and for screen y
    int16_t c[4];
    c[X_LEFT]   = raw[0][cx];
    c[X_RIGHT]  = (raw[1][cx] + raw[2][cx]) / 2;
    c[Y_TOP]    = (raw[0][cy] + raw[1][cy]) / 2;
    c[Y_BOTTOM] = raw[2][cy];
    if (!sane(c)) return 0;
    // On each leg the other channel should hardly move. If it did, the
    // finger didn't go where the crosses were.
    if (span(raw[1][cy], raw[0][cy]) * 4 > span(c[X_LEFT], c[X_RIGHT])) return 0;
    if (span(raw[2][cx], raw[1][cx]) * 4 > span(c[Y_TOP], c[Y_BOTTOM])) return 0;

    for (uint8_t i = 0; i < 4; i++) {
        cal[i] = c[i];
        eeprom_update_word(ee_value(i), (uint16_t)c[i]);
    }
    cal_swap = swap;
    eeprom_update_byte(EE_CAL_SWAP, swap);
    eeprom_update_byte(EE_CAL, EE_CAL_MAGIC);    // last: a power cut halfway leaves no half calibration
    cal_saved = 1;
    return 1;
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

// Map a raw reading to the screen, using the raw values at the crosses.
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

    const uint16_t ra = median3(a[0], a[1], a[2]);
    const uint16_t rb = median3(b[0], b[1], b[2]);
    const uint16_t pressure = z1 + 4095 - z2;

    // Nothing pressing (or nothing wired, which reads all 0s or all 1s)
    if (z1 < 50 || z2 > 4050 || pressure < TOUCH_PRESSURE) return 0;
    if (ra < 50 || ra > 4050 || rb < 50 || rb > 4050) return 0;
    touch_raw_x = ra;
    touch_raw_y = rb;

    const uint16_t rx = cal_swap ? rb : ra;
    const uint16_t ry = cal_swap ? ra : rb;
    *x = to_screen(rx, cal[X_LEFT], cal[X_RIGHT], TOUCH_CAL_LEFT, TOUCH_CAL_RIGHT, TFT_W);
    *y = to_screen(ry, cal[Y_TOP], cal[Y_BOTTOM], TOUCH_CAL_TOP, TOUCH_CAL_BOTTOM, TFT_H);
    return 1;
}
