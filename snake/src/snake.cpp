#include "game.h"
#include "tft.h"
#include "ui.h"
#include "input.h"
#include "sound.h"
#include "scores.h"
#include "tapdebug.h"
#include "rng.h"
#include <avr/pgmspace.h>
#include <string.h>

// =====================================================================
// SNAKE
//   Steer with the joystick or by tapping. Each apple grows the snake and
//   speeds it up.
//   Every 5th apple brings a golden apple for 8 s. Eating it starts
//   golden mode for 10 s: the snake turns orange, the walls wrap around
//   and apples score double.
//   Every 7th apple brings a star for 8 s. Eating it gives 8 s of star
//   power: a rainbow snake at double speed that nothing can hurt. It goes
//   through the walls and through itself. When the star wears off, the
//   snake crashes only if its head is on its own body right then; a
//   crossing anywhere else untangles as the tail moves on.
//   Hitting a wall (outside golden mode and star power) or yourself costs
//   a life: the snake stops for a second, then starts over from the left
//   (the score stays). Losing the third life ends the game.
//   Reach length 100 to win.
// =====================================================================

#define START_LEN   3
#define WIN_LENGTH  100
#define START_STEP  16               // ticks (10 ms) between moves: 160 ms
#define MIN_STEP    7                // fastest: 70 ms
#define SPEEDUP     4                // one tick faster every 4 apples
#define GOLD_EVERY  5                // a golden apple after every 5th apple
#define GOLD_LIFE   800              // golden apple stays 8 s
#define STAR_EVERY  7                // a star after every 7th apple
#define STAR_LIFE   800              // the star stays 8 s
#define STAR_TIME   800              // star power lasts 8 s
#define START_LIVES 3
#define CRASH_TIME  100              // the snake stays crashed on screen for 1 s
#define POWER_TIME  1000             // golden mode lasts 10 s

#define C_BODY    RGB565(  0, 170,   0)
#define C_HEAD    RGB565(120, 255, 120)
#define C_GBODY   RGB565(255, 150,   0)    // golden mode: orange snake
#define C_GHEAD   RGB565(255, 200,  80)
#define C_SHEAD   RGB565(255, 255, 255)    // star power: a white head on a rainbow body
#define C_CRASH   RGB565(255,   0,   0)    // the head after a crash
#define C_APPLE   RGB565(230,   0,   0)
#define C_GAPPLE  RGB565(255, 230,   0)
#define C_LEAF    RGB565( 60, 200,  40)
#define C_STEM    RGB565(120,  70,  20)
#define C_SHINE   RGB565(255, 190, 190)
#define C_STARY   RGB565(255, 210,   0)    // the star
#define C_STARO   RGB565(255, 140,   0)    // its shaded side

// Star power: the body in neon rainbow colors, flowing along the snake
static const uint16_t RAINBOW[7] = {
    RGB565(255,   0, 110), RGB565(255, 120,   0), RGB565(255, 255,   0), RGB565( 60, 255,   0),
    RGB565(  0, 255, 220), RGB565( 40, 120, 255), RGB565(190,  40, 255),
};

// ---- Icons: 14 x 14 pixel art, drawn 1 px inside their cell ----
// Apple: '1' the apple, '2' leaf, '3' stem, '4' shine. Gold uses it too.
static const char APPLE_ART[] PROGMEM =
    "......3.22...."
    "......322....."
    "......3......."
    "..1111.1111..."
    ".11111111111.."
    "1141111111111."
    "1411111111111."
    "1111111111111."
    "1111111111111."
    "1111111111111."
    ".11111111111.."
    ".11111111111.."
    "..1111.1111..."
    "...11...11....";
// Star: '1' yellow, '2' the shaded side
static const char STAR_ART[] PROGMEM =
    "......1......."
    "......1......."
    ".....111......"
    ".....111......"
    "1111111111111."
    ".11111111122.."
    "..111111122..."
    "...1111122...."
    "...1111122...."
    "..1111.1122..."
    "..111...122..."
    ".111.....122.."
    ".11.......22.."
    "..............";
static_assert(sizeof APPLE_ART == 14 * 14 + 1 && sizeof STAR_ART == 14 * 14 + 1, "icons are 14 x 14");
static const uint16_t APPLE_COLORS[4] = { C_APPLE,  C_LEAF,  C_STEM, C_SHINE };
static const uint16_t GOLD_COLORS[4]  = { C_GAPPLE, C_LEAF,  C_STEM, C_WHITE };
static const uint16_t STAR_COLORS[4]  = { C_STARY,  C_STARO, C_STARY, C_STARY };

SnakeData S;
uint8_t snake_over = 0;

static const int8_t DX[5]  = { 0,  0, 0, -1, 1 };          // indexed by DIR_*
static const int8_t DY[5]  = { 0, -1, 1,  0, 0 };
static const uint8_t OPP[5] = { DIR_NONE, DIR_SOUTH, DIR_NORTH, DIR_EAST, DIR_WEST };

static int16_t px(uint8_t cx) { return cx * CELL; }
static int16_t py(uint8_t cy) { return FIELD_Y + cy * CELL; }
static uint8_t golden()  { return S.power_timer > 0; }
static uint8_t starred() { return S.star_timer > 0; }
static uint8_t is_snake(uint8_t v) { return v != EMPTY && v <= SEG_MAX; }

// ---- The body: a ring of cells, tail first ----
// ring(j): where segment j (0 = the tail) sits in bx[] / by[]
static uint8_t ring(uint8_t j) {
    const uint8_t i = S.tail_i + j;
    return i >= SN_RING ? i - SN_RING : i;
}

static void push_head(uint8_t x, uint8_t y) {
    const uint8_t i = ring(S.length);
    S.bx[i] = x;
    S.by[i] = y;
    S.length++;
    S.grid[y][x]++;                // one more segment on this cell
    S.hx = x;
    S.hy = y;
}

static void pop_tail() {
    S.grid[S.ty][S.tx]--;
    S.tail_i = ring(1);
    S.length--;
    S.tx = S.bx[S.tail_i];
    S.ty = S.by[S.tail_i];
}

// ---- Drawing ----
// Snake pieces are 14 x 14 squares, leaving a 1 px black gap around each
// so the segments show. Apples and the star are 14 x 14 icons.
static uint16_t seg_color(uint8_t i) {         // i: the segment's place in the ring
    if (starred()) {
        uint8_t k = i % 7 + S.hue;
        if (k >= 7) k -= 7;
        return RAINBOW[k];
    }
    return golden() ? C_GBODY : C_BODY;
}
static void draw_empty(uint8_t x, uint8_t y) { tft_fill_rect(px(x), py(y), CELL, CELL, C_BLACK); }
static void draw_seg(uint8_t i) { tft_fill_rect(px(S.bx[i]) + 1, py(S.by[i]) + 1, CELL - 2, CELL - 2, seg_color(i)); }
static void draw_head() {
    tft_fill_rect(px(S.hx) + 1, py(S.hy) + 1, CELL - 2, CELL - 2, starred() ? C_SHEAD : golden() ? C_GHEAD : C_HEAD);
}
static void draw_item(uint8_t x, uint8_t y, uint8_t what) {
    tft_sprite_P(px(x) + 1, py(y) + 1, 14, 14, what == STAR ? STAR_ART : APPLE_ART,
                 what == STAR ? STAR_COLORS : what == GOLD ? GOLD_COLORS : APPLE_COLORS, C_BLACK);
}

// Draw one cell again from what the grid says is there (the touch debug
// overlay cleans up after itself with this)
void snake_redraw_cell(uint8_t x, uint8_t y) {
    const uint8_t v = S.grid[y][x];
    draw_empty(x, y);
    if (v >= FOOD) { draw_item(x, y, v); return; }
    if (v == EMPTY) return;
    if (x == S.hx && y == S.hy) { draw_head(); return; }
    for (uint8_t j = S.length; j-- > 0; ) {       // the newest segment there is the one on top
        const uint8_t i = ring(j);
        if (S.bx[i] == x && S.by[i] == y) { draw_seg(i); return; }
    }
}

// Repaint the whole snake, tail to head (golden mode or star power
// starting or ending)
static void repaint_snake() {
    for (uint8_t j = 0; j + 1 < S.length; j++) draw_seg(ring(j));
    draw_head();
}

// Star power, on ticks without a move: repaint two segments, tail to head,
// in the next rainbow phase, so the colors flow along the snake. A new
// phase starts at most every 80 ms. A cell two segments share is skipped,
// so the newer one stays on top.
static void rainbow_tick() {
    if (S.hue_wait < 8) S.hue_wait++;
    for (uint8_t k = 0; k < 2; k++) {
        if (S.paint + 1 >= S.length) {             // swept up to the head
            if (S.hue_wait < 8) return;
            S.hue_wait = 0;
            S.paint = 0;
            S.hue = S.hue == 6 ? 0 : S.hue + 1;
        }
        const uint8_t i = ring(S.paint++);
        if (S.grid[S.by[i]][S.bx[i]] == 1) draw_seg(i);
    }
}

// Put an apple (or a star) on a random empty cell. Returns 0 if the board is full.
static uint8_t place(uint8_t what, uint8_t *ox, uint8_t *oy) {
    for (uint8_t tries = 0; tries < 40; tries++) {
        const uint8_t x = rng_below(SN_COLS), y = rng_below(SN_ROWS);
        if (S.grid[y][x] == EMPTY) {
            S.grid[y][x] = what; *ox = x; *oy = y;
            draw_item(x, y, what);
            return 1;
        }
    }
    const uint16_t start = rng_next() % (SN_COLS * SN_ROWS);      // crowded: scan for a gap
    for (uint16_t k = 0; k < SN_COLS * SN_ROWS; k++) {
        const uint16_t i = (start + k) % (SN_COLS * SN_ROWS);
        const uint8_t x = i % SN_COLS, y = i / SN_COLS;
        if (S.grid[y][x] == EMPTY) {
            S.grid[y][x] = what; *ox = x; *oy = y;
            draw_item(x, y, what);
            return 1;
        }
    }
    return 0;
}

// Music speeds up with the snake; a star doubles it, like the snake
static void set_tempo() { sound_tempo(starred() ? S.step_ticks * 7 / 2 : S.step_ticks * 7); }

// A new snake: three segments left of center, heading right. Used at the
// start of a game and after each lost life. An apple sitting where the
// snake appears is moved somewhere else.
static void spawn_snake() {
    const uint8_t y = SN_ROWS / 2;
    uint8_t apple_lost = 0;
    S.tail_i = 0;
    S.length = 0;
    for (uint8_t x = 5; x < 5 + START_LEN; x++) {
        if (S.grid[y][x] == FOOD) apple_lost = 1;
        S.grid[y][x] = EMPTY;                      // a golden apple or a star there is simply gone
        push_head(x, y);
    }
    S.tx = S.bx[0];
    S.ty = S.by[0];
    S.grow = 0;
    S.dir = S.next_dir = DIR_EAST;
    S.step_count = 0;
    repaint_snake();
    uint8_t fx, fy;
    if (apple_lost) place(FOOD, &fx, &fy);
}

static void start() {
    memset(&S, 0, sizeof S);
    input_clear();                 // stick wiggles on the game-over screen don't steer the new game
    snake_over = 0;
    S.lives = START_LIVES;
    ui_hud(0, score_get(GAME_SNAKE, 0), S.lives);
    tft_fill_rect(0, FIELD_Y, TFT_W, TFT_H - FIELD_Y, C_BLACK);
    tapdbg_forget();               // the field is blank: no debug drawing left on it

    S.step_ticks = START_STEP;
    spawn_snake();
    uint8_t fx, fy;
    place(FOOD, &fx, &fy);
    sound_music(SONG_SNAKE);
    set_tempo();
}

static int finish(uint8_t won) {
    tapdbg_erase();                // before the box goes up, or the wipe would draw over it
    sound_music(0);
    const int8_t rank = score_submit(GAME_SNAKE, S.score);
    ui_game_over(won, S.score, rank);
    sound_sfx(won ? SFX_WIN : SFX_OVER);           // after the panel, so the jingle isn't stretched by the draw
    input_clear();                 // a press from before the crash doesn't count on the box
    snake_over = 1;                // System takes it from here
    return SN_OVER;
}

// A crash costs a life. The last life ends the game; otherwise the snake
// freezes for a second with a red head, then respawn() starts it over.
static int crash() {
    tapdbg_erase();                // before the head turns red
    S.lives--;
    ui_hud_lives(S.lives);         // 0 too: the status bar shouldn't claim a life is left
    if (S.lives == 0) return finish(0);
    tft_fill_rect(px(S.hx) + 1, py(S.hy) + 1, CELL - 2, CELL - 2, C_CRASH);
    sound_music(0);
    sound_sfx(SFX_CRASH);
    S.crash_timer = CRASH_TIME;
    return SN_CRASH;
}

// After the crash pause: erase the old snake, end golden mode, new snake.
// Score, speed and the apples on the board stay as they were.
static void respawn() {
    for (uint8_t j = 0; j < S.length; j++) {
        const uint8_t i = ring(j);
        S.grid[S.by[i]][S.bx[i]] = EMPTY;
        draw_empty(S.bx[i], S.by[i]);
    }
    if (golden()) { S.power_timer = 0; ui_hud_gold(0); }
    spawn_snake();
    input_clear();                 // a flick made during the crash doesn't steer the new snake
    sound_music(SONG_SNAKE);
    set_tempo();
}

// Star power over: back to green (or orange) and the usual song. The head
// on its own body right now is a crash; anywhere else the snake may still
// cross itself, and that untangles as the tail moves on.
static int end_star() {
    ui_hud_star(0);
    repaint_snake();
    sound_music(golden() ? SONG_POWER : SONG_SNAKE);
    set_tempo();
    if (S.grid[S.hy][S.hx] > 1) return crash();
    return SN_PLAY;
}

// Taps are measured from the middle of the field
#define MID_X  (TFT_W / 2)
#define MID_Y  (FIELD_Y + (TFT_H - FIELD_Y) / 2)
#define HALF_W (TFT_W / 2)
#define HALF_H ((TFT_H - FIELD_Y) / 2)

// The field works as a D-pad: its two diagonals cut it into four triangles,
// and the one a tap lands in is the heading it asks for (the top one north,
// the right one east...). Where the snake is, and which way it's going,
// play no part: the same spot on the glass always means the same heading.
// A tap right in the middle means nothing.
static uint8_t tap_direction() {
    const int16_t dx = in_tap_x - MID_X;
    const int16_t dy = in_tap_y - MID_Y;
    const int16_t ax = dx < 0 ? -dx : dx;
    const int16_t ay = dy < 0 ? -dy : dy;
    if (ax <= CELL / 2 && ay <= CELL / 2) return DIR_NONE;
    // Which side of the diagonal: ax / HALF_W against ay / HALF_H, multiplied out
    if ((int32_t)ax * HALF_H >= (int32_t)ay * HALF_W) return dx < 0 ? DIR_WEST : DIR_EAST;
    return dy < 0 ? DIR_NORTH : DIR_SOUTH;
}

// Steering: a tap on the field, a direction event (a fresh flick) or the
// held direction. Reversing straight into your own neck is ignored.
static void read_steering() {
    if (input_take_tap(0, FIELD_Y, TFT_W, TFT_H)) {
        const uint8_t t = tap_direction();
        const uint8_t ok = t != DIR_NONE && t != OPP[S.dir];  // straight behind: ignored
        tapdbg_show(t, t == DIR_NONE ? TAP_NONE : !ok ? TAP_BEHIND : t == S.dir ? TAP_SAME : TAP_TURN,
                    MID_X, MID_Y, in_tap_x, in_tap_y);
        if (ok) S.next_dir = t;
        return;
    }
    uint8_t want = DIR_NONE;
    const uint8_t ev = input_take(EV_DIRS);
    if      (ev & EV_UP)    want = DIR_NORTH;
    else if (ev & EV_DOWN)  want = DIR_SOUTH;
    else if (ev & EV_LEFT)  want = DIR_WEST;
    else if (ev & EV_RIGHT) want = DIR_EAST;
    else                    want = in_dir;
    if (want != DIR_NONE && want != OPP[S.dir]) S.next_dir = want;
}

// One move. Returns the next state.
static int step() {
    S.dir = S.next_dir;
    int8_t nx = S.hx + DX[S.dir];
    int8_t ny = S.hy + DY[S.dir];
    if (nx < 0 || nx >= SN_COLS || ny < 0 || ny >= SN_ROWS) {
        if (!golden() && !starred()) return crash();      // wall
        nx = (nx + SN_COLS) % SN_COLS;                    // golden mode or a star: wrap around
        ny = (ny + SN_ROWS) % SN_ROWS;
    }

    const uint8_t target = S.grid[ny][nx];
    if (is_snake(target) && !starred()) {
        // Into the body: only the tail's cell is safe, and only if the tail
        // is about to move off it and leave it empty
        const uint8_t leaving = (nx == S.tx && ny == S.ty && S.grow == 0) ? 1 : 0;
        if (target > leaving) return crash();
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
        if (!starred()) sound_music(SONG_POWER);          // a star's song plays on while it lasts
        sound_sfx(SFX_POWERUP);
        ui_hud_score(S.score);
    } else if (target == STAR) {
        S.score += 50;
        S.grow++;
        S.star_life = 0;
        S.star_timer = STAR_TIME;
        S.hue = 0;
        S.hue_wait = 0;
        sound_music(SONG_STAR);
        set_tempo();
        sound_sfx(SFX_POWERUP);
        ui_hud_score(S.score);
    }

    // Tail: stays put while growing, otherwise moves one cell
    if (S.grow > 0) {
        S.grow--;
    } else {
        const uint8_t ox = S.tx, oy = S.ty;
        pop_tail();
        if (S.grid[oy][ox] == EMPTY && !(ox == nx && oy == ny)) draw_empty(ox, oy);
    }

    // Head: the old head becomes body, the new one goes on top
    draw_seg(ring(S.length - 1));
    if (target >= FOOD) S.grid[ny][nx] = EMPTY;           // eaten
    push_head(nx, ny);
    draw_head();

    if (target == GOLD) {
        if (!starred()) repaint_snake();                  // orange (a star keeps its rainbow)
        ui_hud_gold(POWER_TIME / 100);
    }
    if (target == STAR) {
        repaint_snake();                                  // rainbow
        S.paint = S.length;                               // the first new phase in 80 ms
        ui_hud_star(STAR_TIME / 100);
    }
    if (S.length >= WIN_LENGTH) return finish(1);
    if (target == FOOD) {
        uint8_t fx, fy;
        if (!place(FOOD, &fx, &fy)) return finish(1);     // no room left: board cleared
        if (S.eaten % GOLD_EVERY == 0 && S.gold_timer == 0 && !golden()) {
            if (place(GOLD, &S.gx, &S.gy)) { S.gold_timer = GOLD_LIFE; sound_sfx(SFX_GOLD); }
        }
        if (S.eaten % STAR_EVERY == 0 && S.star_life == 0 && !starred()) {
            if (place(STAR, &S.starx, &S.stary)) { S.star_life = STAR_LIFE; sound_sfx(SFX_GOLD); }
        }
    }
    return SN_PLAY;
}

// Timers that run every tick: the golden apple's and the star's time on
// the board, golden mode, star power. Returns the next state: the star
// wearing off can end in a crash.
static int tick_timers() {
    if (S.gold_timer > 0) {
        S.gold_timer--;
        if (S.grid[S.gy][S.gx] != GOLD) {
            S.gold_timer = 0;                              // eaten
        } else if (S.gold_timer == 0) {
            S.grid[S.gy][S.gx] = EMPTY;                    // expired
            draw_empty(S.gx, S.gy);
        }
    }
    if (S.star_life > 0) {
        S.star_life--;
        if (S.grid[S.stary][S.starx] != STAR) {
            S.star_life = 0;                               // eaten
        } else if (S.star_life == 0) {
            S.grid[S.stary][S.starx] = EMPTY;              // gone
            draw_empty(S.starx, S.stary);
        }
    }
    if (S.power_timer > 0) {
        S.power_timer--;
        if (S.power_timer == 0) {
            ui_hud_gold(0);
            if (!starred()) {                              // with a star on, its colors and song stay
                repaint_snake();                           // back to green
                sound_music(SONG_SNAKE);
                set_tempo();
            }
        } else if (S.power_timer % 100 == 0) {
            ui_hud_gold(S.power_timer / 100);              // seconds left
        }
    }
    if (S.star_timer > 0) {
        S.star_timer--;
        if (S.star_timer == 0) return end_star();
        if (S.star_timer % 100 == 0) ui_hud_star(S.star_timer / 100);
    }
    return SN_PLAY;
}

// =====================================================================
// Snake task, period 10 ms
//   SN_OFF  --(System sets snake_restart)--> SN_PLAY (draw a fresh game)
//   SN_PLAY --(wall / self, lives left)--> SN_CRASH
//           --(wall / self, last life)--> SN_OVER   --(length 100)--> SN_OVER (win)
//           --(star wears off with the head on the body)--> SN_CRASH (or SN_OVER)
//   SN_CRASH --(1 s later: new snake)--> SN_PLAY
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
        case SN_PLAY: {
            if (sys_paused) break;
            read_steering();
            tapdbg_tick();
            state = tick_timers();
            if (state != SN_PLAY) break;
            const uint8_t every = starred() ? (S.step_ticks + 1) / 2 : S.step_ticks;   // a star: double speed
            if (++S.step_count >= every) {
                S.step_count = 0;
                state = step();
            } else if (starred()) {
                rainbow_tick();            // only on ticks without a move: spreads out the drawing
            }
            break;
        }
        case SN_CRASH:
            if (sys_paused) break;
            if (--S.crash_timer == 0) {
                respawn();
                state = SN_PLAY;
            }
            break;
        case SN_OVER:                     // the box stays up until System leaves it
            break;
        default:
            state = SN_OFF;
            break;
    }
    return state;
}
