#pragma once
#include <stdint.h>

enum Directions { DIR_NONE = 0, DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };

// Press events. They stay set until a task takes them with input_take().
#define EV_RESET 0x01
#define EV_SW    0x02
#define EV_UP    0x04
#define EV_DOWN  0x08
#define EV_LEFT  0x10
#define EV_RIGHT 0x20
#define EV_DIRS  (EV_UP | EV_DOWN | EV_LEFT | EV_RIGHT)

// Published by the Input task every 20 ms
extern uint16_t in_joy_x, in_joy_y;   // 0..1023 after orientation fixes
extern uint8_t  in_dir;               // direction held right now (DIR_*)

uint8_t input_take(uint8_t mask);     // returns the requested events and clears them
void    input_clear();                // drop every pending event
void    input_init();

enum Input_States { IN_INIT, IN_SAMPLE };
int TickFct_Input(int state);
