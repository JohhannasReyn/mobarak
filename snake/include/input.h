#pragma once
#include <stdint.h>

// Compass headings on the screen: north is the top edge, east the right.
// The joystick's up/down/left/right map straight onto them.
enum Directions { DIR_NONE = 0, DIR_NORTH, DIR_SOUTH, DIR_WEST, DIR_EAST };

// Press events. They stay set until a task takes them with input_take().
#define EV_RESET 0x01
#define EV_SW    0x02
#define EV_UP    0x04
#define EV_DOWN  0x08
#define EV_LEFT  0x10
#define EV_RIGHT 0x20
#define EV_DIRS  (EV_UP | EV_DOWN | EV_LEFT | EV_RIGHT)
#define EV_TAP   0x40       // the touch screen was tapped, at in_tap_x, in_tap_y

// Published by the Input task every 20 ms
extern uint16_t in_joy_x, in_joy_y;   // 0..1023 after orientation fixes
extern uint8_t  in_dir;               // direction held right now (DIR_*)
extern uint8_t  in_touch;             // 1 while the screen is pressed
extern int16_t  in_touch_x, in_touch_y;   // where it is pressed right now
extern int16_t  in_tap_x, in_tap_y;   // where the last tap (EV_TAP) landed

uint8_t input_take(uint8_t mask);     // returns the requested events and clears them
// Takes a pending tap only if it landed inside x0 <= x < x1, y0 <= y < y1
uint8_t input_take_tap(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
uint8_t input_sw_down();              // the stick button is held right now (read at boot)
void    input_clear();                // drop every pending event
void    input_init();

enum Input_States { IN_INIT, IN_SAMPLE };
int TickFct_Input(int state);
