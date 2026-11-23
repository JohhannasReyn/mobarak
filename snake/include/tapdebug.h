#pragma once
#include <stdint.h>
#include "config.h"

// Touch debugging overlay: built in while TOUCH_DEBUG is 1 in config.h,
// switched on and off by the title menu's DRAW VISUAL TOUCH (off until
// you switch it on). While it's on, each tap on the field draws:
//   - a cyan line from the middle of the field (white cross) to the spot
//     the tap registered at
//   - a cross on that spot
//   - the heading the tap asked for, N, E, S or W, in a color that says
//     what the snake did with it:
//       green: turned that way     white: was already going that way
//       red:   straight behind it, ignored     gray "-": right in the middle, nothing
// It's wiped at the next tap or after 2 s, by redrawing every cell it
// drew on from the grid.

enum Tap_Results { TAP_TURN, TAP_SAME, TAP_BEHIND, TAP_NONE };

// Draws one cell again from the grid (in snake.cpp)
void snake_redraw_cell(uint8_t x, uint8_t y);

#define TAPDBG (TOUCH_ENABLED && TOUCH_DEBUG)    // nothing to show without the touch panel
#if TAPDBG
void tapdbg_enable(uint8_t on);
uint8_t tapdbg_enabled();
void tapdbg_show(uint8_t dir, uint8_t result, int16_t from_x, int16_t from_y, int16_t tap_x, int16_t tap_y);
void tapdbg_tick();          // every Snake tick while playing: wipes it when its time is up
void tapdbg_erase();         // wipe it now (before a crash or the game-over box)
void tapdbg_forget();        // the whole field was cleared: nothing left to wipe
uint8_t tapdbg_covers(uint8_t x, uint8_t y);   // 1 if it's drawn on this cell (the simulator
                                               // leaves those cells out of its render check)
#else
static inline void tapdbg_enable(uint8_t) {}
static inline uint8_t tapdbg_enabled() { return 0; }
static inline void tapdbg_show(uint8_t, uint8_t, int16_t, int16_t, int16_t, int16_t) {}
static inline void tapdbg_tick() {}
static inline void tapdbg_erase() {}
static inline void tapdbg_forget() {}
#endif
