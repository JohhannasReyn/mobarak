#pragma once
#include <stdint.h>

enum Games { GAME_SNAKE = 0, GAME_COUNT };   // scores.cpp sizes its table from this

// ---- System task -> Snake task ----
extern uint8_t snake_active;      // 1 while Snake owns the screen (0 on the title screen)
extern uint8_t snake_restart;     // System sets 1 to (re)start a game; Snake clears it
extern uint8_t sys_paused;        // Snake freezes while this is set

// ---- Snake task -> System task ----
extern uint8_t snake_over;        // 1 while the game-over box is up: a stick press there
                                  // means "play again", so System must not pause on it

// ---- Screen layout: a 32 px status bar, then the field ----
#define HUD_H   32
#define CELL    16
#define FIELD_Y HUD_H
#define SN_COLS 30                // 30 x 18 cells of 16 px: the 480 x 288 field
#define SN_ROWS 18

struct SnakeData {
    uint8_t  grid[SN_ROWS][SN_COLS];   // 0 empty, 1-4 body (direction to the next segment
                                       // toward the head), 5 apple, 6 golden apple
    uint8_t  hx, hy, tx, ty;           // head and tail cells
    uint8_t  dir, next_dir;
    uint16_t length;
    uint8_t  grow;
    uint8_t  step_ticks, step_count;   // the snake moves every step_ticks ticks
    uint16_t score;
    uint8_t  eaten;
    uint8_t  gx, gy;                   // golden apple cell
    uint8_t  lives;          // the game ends when the last one is lost
    uint8_t  crash_timer;    // ticks left of the pause after a crash
    uint16_t gold_timer;               // ticks the golden apple stays (0 = none on the board)
    uint16_t power_timer;              // ticks of golden mode left
};
extern SnakeData S;

// ---- Task tick functions and their states ----
enum System_States { SYS_INIT, SYS_TITLE, SYS_PLAY, SYS_PAUSE };
enum Snake_States  { SN_OFF, SN_PLAY, SN_CRASH, SN_OVER };

int TickFct_System(int state);
int TickFct_Snake(int state);
