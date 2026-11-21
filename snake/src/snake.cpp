#include "game.h"
#include "tft.h"
#include "ui.h"
#include "input.h"
#include "sound.h"
#include "scores.h"
#include "rng.h"
#include <string.h>

// =====================================================================
// SNAKE
//   Steer with the joystick. Each apple grows the snake and speeds it up.
//   Every 5th apple brings a golden apple for 8 s. Eating it starts
//   golden mode for 10 s: the snake turns orange, the walls wrap around
//   and apples score double.
//   Hitting a wall (outside golden mode) or yourself ends the game.
//   Reach length 100 to win.
// =====================================================================

#define EMPTY       0                // 1-4 = body, pointing to the next segment toward the head
#define FOOD        5
#define GOLD        6
#define START_LEN   3
#define WIN_LENGTH  100
#define START_STEP  16               // ticks (10 ms) between moves: 160 ms
#define MIN_STEP    7                // fastest: 70 ms
#define SPEEDUP     4                // one tick faster every 4 apples
#define GOLD_EVERY  5                // a golden apple after every 5th apple
#define GOLD_LIFE   800              // golden apple stays 8 s
#define OVER_GUARD  50               // stick presses ignored for 500 ms after game over
#define POWER_TIME  1000             // golden mode lasts 10 s

#define C_BODY    RGB565(  0, 170,   0)
#define C_HEAD    RGB565(120, 255, 120)
#define C_GBODY   RGB565(255, 150,   0)    // golden mode: orange snake
#define C_GHEAD   RGB565(255, 200,  80)
#define C_APPLE   RGB565(230,   0,   0)
#define C_GAPPLE  RGB565(255, 230,   0)

SnakeData S;
uint8_t snake_over = 0;
static uint8_t over_guard = 0;       // ticks left before a press can restart

static const int8_t DX[5]  = { 0,  0, 0, -1, 1 };          // indexed by DIR_*
static const int8_t DY[5]  = { 0, -1, 1,  0, 0 };
static const uint8_t OPP[5] = { DIR_NONE, DIR_DOWN, DIR_UP, DIR_RIGHT, DIR_LEFT };

static int16_t px(uint8_t cx) { return cx * CELL; }
static int16_t py(uint8_t cy) { return FIELD_Y + cy * CELL; }
static uint8_t golden() { return S.power_timer > 0; }

// Snake pieces are 14 x 14 squares, leaving a 1 px black gap around each
// so the segments show. Apples are 10 x 10.
static void draw_empty(uint8_t x, uint8_t y) { tft_fill_rect(px(x), py(y), CELL, CELL, C_BLACK); }
static void draw_body(uint8_t x, uint8_t y)  { tft_fill_rect(px(x) + 1, py(y) + 1, CELL - 2, CELL - 2, golden() ? C_GBODY : C_BODY); }
static void draw_head(uint8_t x, uint8_t y)  { tft_fill_rect(px(x) + 1, py(y) + 1, CELL - 2, CELL - 2, golden() ? C_GHEAD : C_HEAD); }
static void draw_apple(uint8_t x, uint8_t y, uint8_t gold) {
    tft_fill_rect(px(x) + 3, py(y) + 3, CELL - 6, CELL - 6, gold ? C_GAPPLE : C_APPLE);
}

// Repaint the whole snake (golden mode starting or ending)
static void repaint_snake() {
    uint8_t x = S.tx, y = S.ty;
    for (uint16_t i = 1; i < S.length; i++) {
        draw_body(x, y);
        const uint8_t d = S.grid[y][x];
        x = (x + DX[d] + SN_COLS) % SN_COLS;
        y = (y + DY[d] + SN_ROWS) % SN_ROWS;
    }
    draw_head(S.hx, S.hy);
}

// Put an apple on a random empty cell. Returns 0 if the board is full.
static uint8_t place(uint8_t what, uint8_t *ox, uint8_t *oy) {
    for (uint8_t tries = 0; tries < 40; tries++) {
        const uint8_t x = rng_below(SN_COLS), y = rng_below(SN_ROWS);
        if (S.grid[y][x] == EMPTY) {
            S.grid[y][x] = what; *ox = x; *oy = y;
            draw_apple(x, y, what == GOLD);
            return 1;
        }
    }
    const uint16_t start = rng_next() % (SN_COLS * SN_ROWS);      // crowded: scan for a gap
    for (uint16_t k = 0; k < SN_COLS * SN_ROWS; k++) {
        const uint16_t i = (start + k) % (SN_COLS * SN_ROWS);
        const uint8_t x = i % SN_COLS, y = i / SN_COLS;
        if (S.grid[y][x] == EMPTY) {
            S.grid[y][x] = what; *ox = x; *oy = y;
            draw_apple(x, y, what == GOLD);
            return 1;
        }
    }
    return 0;
}

static void set_tempo() { sound_tempo(S.step_ticks * 7); }    // music speeds up with the snake

static void start() {
    memset(&S, 0, sizeof S);
    input_clear();                 // stick wiggles on the game-over screen don't steer the new game
    snake_over = 0;
    ui_hud(0, score_get(GAME_SNAKE, 0));
    tft_fill_rect(0, FIELD_Y, TFT_W, TFT_H - FIELD_Y, C_BLACK);

    // Three segments left of center, heading right
    S.ty = S.hy = SN_ROWS / 2;
    S.tx = 5;
    S.hx = S.tx + START_LEN - 1;
    for (uint8_t x = S.tx; x <= S.hx; x++) S.grid[S.hy][x] = DIR_RIGHT;
    S.length = START_LEN;
    S.dir = S.next_dir = DIR_RIGHT;
    S.step_ticks = START_STEP;
    repaint_snake();

    uint8_t fx, fy;
    place(FOOD, &fx, &fy);
    sound_music(SONG_SNAKE);
    set_tempo();
}

static int finish(uint8_t won) {
    sound_music(0);
    const int8_t rank = score_submit(GAME_SNAKE, S.score);
    ui_game_over(won, S.score, rank);
    sound_sfx(won ? SFX_WIN : SFX_OVER);           // after the panel, so the jingle isn't stretched by the draw
    input_clear();                 // a press from before the crash must not restart the game
    over_guard = OVER_GUARD;       // and neither can one from the moment of the crash
    snake_over = 1;
    return SN_OVER;
}

// Steering: a direction event (a fresh flick) or the held direction.
// Reversing straight into your own neck is ignored.
static void read_steering() {
    uint8_t want = DIR_NONE;
    const uint8_t ev = input_take(EV_DIRS);
    if      (ev & EV_UP)    want = DIR_UP;
    else if (ev & EV_DOWN)  want = DIR_DOWN;
    else if (ev & EV_LEFT)  want = DIR_LEFT;
    else if (ev & EV_RIGHT) want = DIR_RIGHT;
    else                    want = in_dir;
    if (want != DIR_NONE && want != OPP[S.dir]) S.next_dir = want;
}

// One move. Returns the next state.
static int step() {
    S.dir = S.next_dir;
    int8_t nx = S.hx + DX[S.dir];
    int8_t ny = S.hy + DY[S.dir];
    if (nx < 0 || nx >= SN_COLS || ny < 0 || ny >= SN_ROWS) {
        if (!golden()) return finish(0);                  // wall
        nx = (nx + SN_COLS) % SN_COLS;                    // golden mode: wrap around
        ny = (ny + SN_ROWS) % SN_ROWS;
    }

    const uint8_t target = S.grid[ny][nx];
    if (target >= DIR_UP && target <= DIR_RIGHT) {
        // Only the tail cell is safe, and only if the tail is about to move
        if (!(nx == S.tx && ny == S.ty && S.grow == 0)) return finish(0);
    }

    if (target == FOOD) {
        S.score += golden() ? 20 : 10;
        S.eaten++;
        S.grow++;
        const int8_t faster = START_STEP - S.eaten / SPEEDUP;
        S.step_ticks = faster > MIN_STEP ? faster : MIN_STEP;
        set_tempo();
        sound_sfx(SFX_EAT);
        ui_hud_score(S.score);
    } else if (target == GOLD) {
        S.score += 50;
        S.grow++;
        S.gold_timer = 0;
        S.power_timer = POWER_TIME;
        sound_music(SONG_POWER);
        sound_sfx(SFX_POWERUP);
        ui_hud_score(S.score);
    }

    // Tail: stays put while growing, otherwise moves one cell
    if (S.grow > 0) {
        S.grow--;
        S.length++;
    } else {
        const uint8_t d = S.grid[S.ty][S.tx];
        S.grid[S.ty][S.tx] = EMPTY;
        if (!(S.tx == nx && S.ty == ny)) draw_empty(S.tx, S.ty);
        S.tx = (S.tx + DX[d] + SN_COLS) % SN_COLS;
        S.ty = (S.ty + DY[d] + SN_ROWS) % SN_ROWS;
    }

    // Head: the old head becomes body and points at the new head
    S.grid[S.hy][S.hx] = S.dir;
    draw_body(S.hx, S.hy);
    S.hx = nx;
    S.hy = ny;
    S.grid[S.hy][S.hx] = S.dir;
    draw_head(S.hx, S.hy);

    if (target == GOLD) { repaint_snake(); ui_hud_gold(POWER_TIME / 100); }
    if (S.length >= WIN_LENGTH) return finish(1);
    if (target == FOOD) {
        uint8_t fx, fy;
        if (!place(FOOD, &fx, &fy)) return finish(1);     // no room left: board cleared
        if (S.eaten % GOLD_EVERY == 0 && S.gold_timer == 0 && !golden()) {
            if (place(GOLD, &S.gx, &S.gy)) { S.gold_timer = GOLD_LIFE; sound_sfx(SFX_GOLD); }
        }
    }
    return SN_PLAY;
}

// Timers that run every tick: the golden apple's life, golden mode
static void tick_timers() {
    if (S.gold_timer > 0) {
        S.gold_timer--;
        if (S.grid[S.gy][S.gx] != GOLD) {
            S.gold_timer = 0;                              // eaten
        } else if (S.gold_timer == 0) {
            S.grid[S.gy][S.gx] = EMPTY;                    // expired
            draw_empty(S.gx, S.gy);
        }
    }
    if (S.power_timer > 0) {
        S.power_timer--;
        if (S.power_timer == 0) {
            repaint_snake();                               // back to green
            ui_hud_gold(0);
            sound_music(SONG_SNAKE);
            set_tempo();
        } else if (S.power_timer % 100 == 0) {
            ui_hud_gold(S.power_timer / 100);              // seconds left
        }
    }
}

// =====================================================================
// Snake task, period 10 ms
//   SN_OFF  --(System sets snake_restart)--> SN_PLAY (draw a fresh game)
//   SN_PLAY --(wall / self)--> SN_OVER      --(length 100)--> SN_OVER (win)
//   SN_OVER --(stick press, once the box has been up 0.5 s)--> SN_PLAY
//   any     --(System sets snake_restart: RESET button)--> SN_PLAY
//   any     --(System goes back to the title screen)--> SN_OFF
// =====================================================================
int TickFct_Snake(int state) {
    if (!snake_active) { snake_over = 0; return SN_OFF; }  // title screen is showing

    if (snake_restart) {
        snake_restart = 0;
        start();
        return SN_PLAY;
    }

    switch (state) {
        case SN_PLAY:
            if (sys_paused) break;
            read_steering();
            tick_timers();
            if (++S.step_count >= S.step_ticks) {
                S.step_count = 0;
                state = step();
            }
            break;
        case SN_OVER:
            if (over_guard > 0) {
                over_guard--;
                input_take(EV_SW);         // too soon: throw the press away
            } else if (input_take(EV_SW)) {
                start();
                state = SN_PLAY;
            }
            break;
        default:
            state = SN_OFF;
            break;
    }
    return state;
}
