#include "tapdebug.h"
#if TAPDBG
#include "game.h"
#include "input.h"
#include "tft.h"
#include <string.h>

#define SHOW_TICKS 200                          // 2 s of 10 ms Snake ticks

// There's no screen buffer to put back, so every cell the overlay draws on
// is marked here, and wiping it means redrawing those cells from the grid.
static uint8_t dirty[SN_ROWS][(SN_COLS + 7) / 8];
static uint8_t showing = 0;
static uint8_t ticks = 0;
static uint8_t enabled = 0;                     // the menu's DRAW VISUAL TOUCH

static const char LETTER[5] = { '-', 'N', 'S', 'W', 'E' };     // indexed by DIR_*
static const uint16_t COLOR[4] = { C_GREEN, C_WHITE, C_RED, C_GRAY };   // by TAP_*

static void mark(int16_t x, int16_t y, int16_t w, int16_t h) {
    const int16_t cx0 = x / CELL, cx1 = (x + w - 1) / CELL;
    const int16_t cy0 = (y - FIELD_Y) / CELL, cy1 = (y + h - 1 - FIELD_Y) / CELL;
    for (int16_t cy = cy0; cy <= cy1; cy++)
        for (int16_t cx = cx0; cx <= cx1; cx++)
            dirty[cy][cx >> 3] |= 1 << (cx & 7);
    showing = 1;
}

// A filled rectangle, cut to fit the field
static void box(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < FIELD_Y) { h -= FIELD_Y - y; y = FIELD_Y; }
    if (x + w > TFT_W) w = TFT_W - x;
    if (y + h > TFT_H) h = TFT_H - y;
    if (w <= 0 || h <= 0) return;
    tft_fill_rect(x, y, w, h, color);
    mark(x, y, w, h);
}

// A 2 px line, drawn as straight runs: one fill per run rather than one per pixel
static void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    const int16_t dx = x1 > x0 ? x1 - x0 : x0 - x1;
    const int16_t dy = y1 > y0 ? y1 - y0 : y0 - y1;
    const int8_t  sx = x1 > x0 ? 1 : -1;
    const int8_t  sy = y1 > y0 ? 1 : -1;
    if (dx >= dy) {                              // mostly sideways: horizontal runs
        int16_t err = dx / 2, start = x0, y = y0;
        for (int16_t x = x0; x != x1; x += sx) {
            err -= dy;
            if (err < 0) {                       // the next pixel is a row further on
                box(start < x ? start : x, y, (x > start ? x - start : start - x) + 1, 2, color);
                y += sy;
                err += dx;
                start = x + sx;
            }
        }
        box(start < x1 ? start : x1, y, (x1 > start ? x1 - start : start - x1) + 1, 2, color);
    } else {                                     // mostly up or down: vertical runs
        int16_t err = dy / 2, start = y0, x = x0;
        for (int16_t y = y0; y != y1; y += sy) {
            err -= dx;
            if (err < 0) {
                box(x, start < y ? start : y, 2, (y > start ? y - start : start - y) + 1, color);
                x += sx;
                err += dy;
                start = y + sy;
            }
        }
        box(x, start < y1 ? start : y1, 2, (y1 > start ? y1 - start : start - y1) + 1, color);
    }
}

void tapdbg_enable(uint8_t on) {
    enabled = on;
    tapdbg_forget();                             // set from the menu: no field on screen
}

uint8_t tapdbg_enabled() { return enabled; }

void tapdbg_show(uint8_t dir, uint8_t result, int16_t from_x, int16_t from_y, int16_t tap_x, int16_t tap_y) {
    if (!enabled) return;
    tapdbg_erase();                              // one tap on screen at a time
    const uint16_t color = COLOR[result];
    line(from_x, from_y, tap_x, tap_y, C_CYAN);
    box(from_x - 6, from_y - 1, 13, 3, C_WHITE); // a white cross where it's measured from
    box(from_x - 1, from_y - 6, 3, 13, C_WHITE);
    box(tap_x - 7, tap_y - 1, 15, 3, color);     // a cross where the tap landed
    box(tap_x - 1, tap_y - 7, 3, 15, color);

    int16_t lx = tap_x + 10, ly = tap_y - 34;   // the letter: above and right of the cross,
    if (lx + 18 > TFT_W) lx = tap_x - 28;       // or wherever it fits on the field
    if (ly < FIELD_Y) ly = tap_y + 10;
    if (ly + 24 > TFT_H) ly = TFT_H - 24;
    if (lx < 0) lx = 0;
    tft_char(lx, ly, LETTER[dir], color, C_BLACK, 3);
    mark(lx, ly, 18, 24);
    ticks = SHOW_TICKS;
}

void tapdbg_tick() {
    if (ticks && --ticks == 0) tapdbg_erase();
}

void tapdbg_erase() {
    if (!showing) return;
    for (uint8_t cy = 0; cy < SN_ROWS; cy++)
        for (uint8_t cx = 0; cx < SN_COLS; cx++)
            if (dirty[cy][cx >> 3] & (1 << (cx & 7))) snake_redraw_cell(cx, cy);
    tapdbg_forget();
}

uint8_t tapdbg_covers(uint8_t x, uint8_t y) {
    return (dirty[y][x >> 3] >> (x & 7)) & 1;
}

void tapdbg_forget() {
    memset(dirty, 0, sizeof dirty);
    showing = 0;
    ticks = 0;
}
#endif
