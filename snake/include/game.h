#pragma once
#include <stdint.h>

enum Games { GAME_SNAKE = 0, GAME_COUNT };   // scores.cpp sizes its table from this

// ---- System task -> Snake task ----
extern uint8_t snake_active;      // 1 while Snake owns the screen (0 on the title screen)
extern uint8_t snake_restart;     // System sets 1 to (re)start a game; Snake clears it
extern uint8_t sys_paused;        // Snake freezes while this is set
extern uint8_t snake_demo;        // 1: the title screen's demo; Snake steers itself, saves no score

// ---- Snake task -> System task ----
extern uint8_t snake_over;        // 1 while the game-over box is up: System takes over,
                                  // and goes back to the menu on a press or after 5 s
extern int8_t  snake_rank;        // where the last score landed in the top 3 (0 = best), or -1

// ---- Screen layout: a 32 px status bar, then the field ----
#define HUD_H   32
#define CELL    16
#define FIELD_Y HUD_H
#define SN_COLS 30                // 30 x 18 cells of 16 px: the 480 x 288 field
#define SN_ROWS 18

// What a grid cell holds: EMPTY, an item, or 1-SEG_MAX: how many snake
// segments are on it. Normally that's 1; a star lets the snake cross
// itself, so a cell can hold 2 or more until the tail untangles it.
#define EMPTY   0
#define SEG_MAX 0xEF
#define FOOD    0xF0                // apple
#define GOLD    0xF1                // golden apple
#define STAR    0xF2                // star

// The body, tail to head, as a ring of cells: bx[i], by[i]. SN_RING is a
// multiple of 7 (the rainbow's colors) and more than the 100 of a win.
#define SN_RING 126

struct SnakeData {
    uint8_t  grid[SN_ROWS][SN_COLS];   // see above
    uint8_t  bx[SN_RING], by[SN_RING]; // the snake's cells; the tail is at tail_i
    uint8_t  tail_i;
    uint8_t  hx, hy, tx, ty;           // head and tail cells
    uint8_t  dir, next_dir;
    uint8_t  length;                   // segments, head included
    uint8_t  grow;
    uint8_t  step_ticks, step_count;   // the snake moves every step_ticks ticks (half that with a star)
    uint16_t score;
    uint8_t  eaten;
    uint8_t  gx, gy;                   // golden apple cell
    uint16_t gold_timer;               // ticks the golden apple stays (0 = none on the board)
    uint16_t power_timer;              // ticks of golden mode left
    uint8_t  starx, stary;             // star cell
    uint16_t star_life;                // ticks the star stays on the board (0 = none)
    uint16_t star_timer;               // ticks of star power left
    uint8_t  hue, hue_wait, paint;     // star: rainbow phase, ticks in it, next segment to repaint
    uint8_t  ax, ay;                   // apple cell
    uint8_t  lives;                    // the game ends when the last one is lost
    uint8_t  crash_timer;              // ticks left of the pause after a crash
    uint8_t  flash_on, flash_i, flash_wait;   // crash: body white or not, next segment, ticks waited
    uint8_t  ready_timer;              // ticks left of the 3-2-1 countdown
};
extern SnakeData S;

// ---- Task tick functions and their states ----
enum System_States { SYS_INIT, SYS_TITLE, SYS_PLAY, SYS_PAUSE, SYS_TOUCH_SETUP, SYS_SCORES, SYS_OVER,
                     SYS_DEMO, SYS_NAME };
enum Snake_States  { SN_OFF, SN_PLAY, SN_CRASH, SN_OVER, SN_READY };

int TickFct_System(int state);
int TickFct_Snake(int state);
