#pragma once
#include <stdint.h>

// 4.0" 480 x 320 SPI TFT in landscape: ST7796S, ILI9486 or ILI9488 (see
// TFT_DRIVER in config.h). The code uses RGB565 colors everywhere; on the
// 18-bit chips the driver expands each pixel to 3 bytes on the way out.
#define TFT_W 480
#define TFT_H 320

#define RGB565(r, g, b) ((uint16_t)((((uint16_t)(r) & 0xF8) << 8) | (((uint16_t)(g) & 0xFC) << 3) | ((uint16_t)(b) >> 3)))

#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_RED     RGB565(230,  40,  40)
#define C_ORANGE  RGB565(255, 140,   0)
#define C_YELLOW  RGB565(255, 220,   0)
#define C_GREEN   RGB565( 40, 200,  60)
#define C_CYAN    RGB565(  0, 210, 230)
#define C_BLUE    RGB565( 40,  90, 255)
#define C_GRAY    RGB565(140, 140, 150)
#define C_DGRAY   RGB565( 60,  60,  70)
#define C_NAVY    RGB565( 10,  16,  40)

void tft_init();

// Everything below clips to the screen.
void tft_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void tft_frame(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t t, uint16_t color);   // t px thick outline
void tft_fill_screen(uint16_t color);

// 5x7 font in a 6x8 cell, scaled, drawn opaque (fg on bg). Uppercase only.
void tft_char(int16_t x, int16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale);
void tft_text(int16_t x, int16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale);
void tft_text_P(int16_t x, int16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale);   // PROGMEM string
void tft_text_center_P(int16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale);
void tft_number(int16_t x, int16_t y, uint16_t value, uint8_t digits, uint16_t fg, uint16_t bg, uint8_t scale);   // zero-padded
uint16_t tft_text_width_P(const char *s, uint8_t scale);

