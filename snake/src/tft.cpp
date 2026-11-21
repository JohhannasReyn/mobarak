#include "tft.h"
#include "config.h"
#include "spi.h"
#include "font5x7.h"
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#define CS_LOW()   (PORTB &= ~(1 << TFT_CS))
#define CS_HIGH()  (PORTB |=  (1 << TFT_CS))
#define DC_CMD()   (PORTB &= ~(1 << TFT_DC))
#define DC_DATA()  (PORTB |=  (1 << TFT_DC))

// MIPI DCS commands all three chips share
#define C_SWRESET 0x01
#define C_SLPOUT  0x11
#define C_INVOFF  0x20
#define C_INVON   0x21
#define C_DISPON  0x29
#define C_CASET   0x2A
#define C_RASET   0x2B
#define C_RAMWR   0x2C
#define C_MADCTL  0x36
#define C_COLMOD  0x3A

// MADCTL: MV swaps rows and columns for landscape; MX|MY turn it 180 degrees.
#define MADCTL_VALUE ((TFT_FLIP ? 0xE0 : 0x20) | (TFT_BGR ? 0x08 : 0x00))
// ILI9486 SPI panels are built inverted: they need INVON for normal colors.
#if TFT_DRIVER == TFT_ILI9486
#define PANEL_INVERTED 1
#else
#define PANEL_INVERTED 0
#endif
#define INVERT_CMD   ((PANEL_INVERTED ^ TFT_INVERT) ? C_INVON : C_INVOFF)

// Init sequences: command, argument count, arguments. If bit 7 of the
// count is set, a delay in ms follows the arguments. 0xFF ends the table.
#define DELAY 0x80

#if TFT_DRIVER == TFT_ST7796S
static const uint8_t init_cmds[] PROGMEM = {
    C_SWRESET, DELAY, 120,
    C_SLPOUT,  DELAY, 120,
    0xF0, 1, 0xC3,                                   // unlock command set, part 1
    0xF0, 1, 0x96,                                   // unlock command set, part 2
    C_MADCTL, 1, MADCTL_VALUE,
    C_COLMOD, 1, 0x55,                               // 16 bits per pixel
    0xB4, 1, 0x01,                                   // 1-dot inversion
    0xB6, 3, 0x80, 0x02, 0x3B,                       // display function control
    0xE8, 8, 0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33,   // output timing
    0xC1, 1, 0x06,                                   // power control 2
    0xC2, 1, 0xA7,                                   // power control 3
    0xC5, 1 | DELAY, 0x18, 120,                      // VCOM
    0xE0, 14, 0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F,
              0x54, 0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B,   // gamma +
    0xE1, 14 | DELAY, 0xE0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2B,
              0x43, 0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B, 120,   // gamma -
    0xF0, 1, 0x3C,                                   // lock command set again
    0xF0, 1 | DELAY, 0x69, 120,
    INVERT_CMD, 0,
    C_DISPON, DELAY, 50,
    0xFF
};
#elif TFT_DRIVER == TFT_ILI9486
static const uint8_t init_cmds[] PROGMEM = {
    C_SWRESET, DELAY, 120,
    C_SLPOUT,  DELAY, 120,
    C_COLMOD, 1, 0x66,                               // 18 bits per pixel (SPI)
    0xC0, 2, 0x0E, 0x0E,                             // power control 1
    0xC1, 2, 0x41, 0x00,                             // power control 2
    0xC2, 1, 0x55,                                   // power control 3
    0xC5, 4, 0x00, 0x00, 0x00, 0x00,                 // VCOM
    0xE0, 15, 0x0F, 0x1F, 0x1C, 0x0C, 0x0F, 0x08, 0x48, 0x98,
              0x37, 0x0A, 0x13, 0x04, 0x11, 0x0D, 0x00,   // gamma +
    0xE1, 15, 0x0F, 0x32, 0x2E, 0x0B, 0x0D, 0x05, 0x47, 0x75,
              0x37, 0x06, 0x10, 0x03, 0x24, 0x20, 0x00,   // gamma -
    INVERT_CMD, 0,
    C_MADCTL, 1, MADCTL_VALUE,
    C_DISPON, DELAY, 150,
    0xFF
};
#elif TFT_DRIVER == TFT_ILI9488
static const uint8_t init_cmds[] PROGMEM = {
    C_SWRESET, DELAY, 120,
    0xE0, 15, 0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78,
              0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F,   // gamma +
    0xE1, 15, 0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45,
              0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F,   // gamma -
    0xC0, 2, 0x17, 0x15,                             // power control 1
    0xC1, 1, 0x41,                                   // power control 2
    0xC5, 3, 0x00, 0x12, 0x80,                       // VCOM
    C_MADCTL, 1, MADCTL_VALUE,
    C_COLMOD, 1, 0x66,                               // 18 bits per pixel (SPI)
    0xB0, 1, 0x00,                                   // interface mode
    0xB1, 1, 0xA0,                                   // frame rate
    0xB4, 1, 0x02,                                   // 2-dot inversion
    0xB6, 3, 0x02, 0x02, 0x3B,                       // display function control
    0xB7, 1, 0xC6,                                   // entry mode
    0xF7, 4, 0xA9, 0x51, 0x2C, 0x82,                 // adjust control 3
    INVERT_CMD, 0,
    C_SLPOUT, DELAY, 120,
    C_DISPON, DELAY, 25,
    0xFF
};
#else
#error "TFT_DRIVER must be TFT_ST7796S, TFT_ILI9486 or TFT_ILI9488"
#endif

// ---- Pixel format ----
// ST7796S: RGB565, 2 bytes. ILI9486/ILI9488 over SPI only take RGB666,
// 3 bytes, each color in the top 6 bits.
struct Px { uint8_t a, b, c; };
static inline Px px_bytes(uint16_t color) {
#if TFT_DRIVER == TFT_ST7796S
    return Px{ (uint8_t)(color >> 8), (uint8_t)color, 0 };
#else
    return Px{ (uint8_t)((color >> 8) & 0xF8), (uint8_t)((color >> 3) & 0xFC), (uint8_t)(color << 3) };
#endif
}
static inline void push(const Px &p) {
    spi_write(p.a);
    spi_write(p.b);
#if TFT_DRIVER != TFT_ST7796S
    spi_write(p.c);
#endif
}

static void write_cmd(uint8_t c) {
    DC_CMD();
    spi_write(c);
    DC_DATA();
}

static void delay_ms(uint16_t ms) {
    while (ms--) _delay_ms(1);
}

void tft_init() {
    spi_init();
    DDRB |= (1 << TFT_RST) | (1 << TFT_DC) | (1 << TFT_CS);
    CS_HIGH();
    DC_DATA();

    // Hardware reset pulse
    PORTB |= (1 << TFT_RST);   delay_ms(10);
    PORTB &= ~(1 << TFT_RST);  delay_ms(20);
    PORTB |= (1 << TFT_RST);   delay_ms(150);

    CS_LOW();
    const uint8_t *p = init_cmds;
    uint8_t cmd;
    while ((cmd = pgm_read_byte(p++)) != 0xFF) {
        write_cmd(cmd);
        uint8_t nargs = pgm_read_byte(p++);
        const uint8_t has_delay = nargs & DELAY;
        nargs &= 0x7F;
        while (nargs--) spi_write(pgm_read_byte(p++));
        if (has_delay) delay_ms(pgm_read_byte(p++));
    }
    CS_HIGH();
}

// Select the rectangle x0..x1, y0..y1 and start a pixel write.
static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    write_cmd(C_CASET);
    spi_write(x0 >> 8); spi_write(x0);
    spi_write(x1 >> 8); spi_write(x1);
    write_cmd(C_RASET);
    spi_write(y0 >> 8); spi_write(y0);
    spi_write(y1 >> 8); spi_write(y1);
    write_cmd(C_RAMWR);
}

void tft_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > TFT_W) w = TFT_W - x;
    if (y + h > TFT_H) h = TFT_H - y;
    if (w <= 0 || h <= 0) return;

    const Px p = px_bytes(color);
    CS_LOW();
    set_window(x, y, x + w - 1, y + h - 1);
    for (int16_t row = 0; row < h; row++)          // a full screen is 153,600 pixels:
        for (int16_t col = 0; col < w; col++)      // too many for one 16-bit counter
            push(p);
    CS_HIGH();
}

void tft_frame(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t t, uint16_t color) {
    tft_fill_rect(x, y, w, t, color);
    tft_fill_rect(x, y + h - t, w, t, color);
    tft_fill_rect(x, y + t, t, h - 2 * t, color);
    tft_fill_rect(x + w - t, y + t, t, h - 2 * t, color);
}

void tft_fill_screen(uint16_t color) {
    tft_fill_rect(0, 0, TFT_W, TFT_H, color);
}

static uint8_t glyph_index(char c) {
    if (c >= 'a' && c <= 'z') c -= 32;               // uppercase only
    if (c < FONT_FIRST || c > FONT_LAST) c = '?';
    return (uint8_t)(c - FONT_FIRST);
}

void tft_char(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale) {
    const int16_t w = 6 * scale, h = 8 * scale;
    if (x < 0 || y < 0 || x + w > TFT_W || y + h > TFT_H) return;   // whole glyphs only

    const uint8_t g = glyph_index(c);
    uint8_t cols[5];
    for (uint8_t i = 0; i < 5; i++) cols[i] = pgm_read_byte(&font5x7[g][i]);

    const Px f = px_bytes(fg), b = px_bytes(bg);
    CS_LOW();
    set_window(x, y, x + w - 1, y + h - 1);
    for (uint8_t row = 0; row < 8; row++) {
        for (uint8_t sy = 0; sy < scale; sy++) {
            for (uint8_t col = 0; col < 6; col++) {
                const Px &p = (col < 5 && (cols[col] >> row & 1)) ? f : b;
                for (uint8_t sx = 0; sx < scale; sx++) push(p);
            }
        }
    }
    CS_HIGH();
}

void tft_text(int16_t x, int16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale) {
    while (*s) {
        tft_char(x, y, *s++, fg, bg, scale);
        x += 6 * scale;
    }
}

void tft_text_P(int16_t x, int16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale) {
    char c;
    while ((c = pgm_read_byte(s++))) {
        tft_char(x, y, c, fg, bg, scale);
        x += 6 * scale;
    }
}

uint16_t tft_text_width_P(const char *s, uint8_t scale) {
    uint16_t n = 0;
    while (pgm_read_byte(s + n)) n++;
    return n ? n * 6 * scale - scale : 0;     // no trailing gap
}

void tft_text_center_P(int16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale) {
    tft_text_P((TFT_W - (int16_t)tft_text_width_P(s, scale)) / 2, y, s, fg, bg, scale);
}

void tft_number(int16_t x, int16_t y, uint16_t value, uint8_t digits, uint16_t fg, uint16_t bg, uint8_t scale) {
    char buf[6];
    if (digits > 5) digits = 5;
    buf[digits] = 0;
    for (int8_t i = digits - 1; i >= 0; i--) {
        buf[i] = '0' + value % 10;
        value /= 10;
    }
    tft_text(x, y, buf, fg, bg, scale);
}
