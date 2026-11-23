#include "game.h"
#include "tft.h"
#include "ui.h"
#include "input.h"
#include "sound.h"
#include "scores.h"
#include "touch.h"
#include "tapdebug.h"
#include "config.h"
#include <avr/pgmspace.h>

uint8_t snake_active = 0;
uint8_t snake_restart = 0;
uint8_t sys_paused = 0;
uint8_t snake_demo = 0;

// ---- Title menu ----
// Stick up/down moves the marker, a click chooses; or tap an item.
// The two volume rows: left/right (or [-]/[+]) set the level, a click (or
// [MUTE]) mutes; the marker stays on the row while you adjust. The ON/OFF
// item flips its setting and hands the marker back to PLAY, so the next
// click starts a game. All of them are saved in EEPROM.
#define MENU_X    108
#define MENU_Y0   100                 // the first item; the rest follow MENU_STEP apart
#define MENU_STEP 28
enum Menu_Items {
    MENU_PLAY, MENU_SCORES, MENU_MUSIC, MENU_SFX,
#if TOUCH_ENABLED
    MENU_TOUCH,
#endif
#if TAPDBG
    MENU_DRAW,
#endif
    MENU_ITEMS
};
static uint8_t menu_sel = MENU_PLAY;

static int16_t menu_y(uint8_t item) { return MENU_Y0 + item * MENU_STEP; }

// The volume rows: MUSIC VOL: [-] 10 [+] [MUTE]
#define VOL_MINUS_X 240
#define VOL_NUM_X   274
#define VOL_PLUS_X  304
#define VOL_MUTE_X  340
#define BTN_W       26                // [-], [+]
#define MUTE_W      62
static uint8_t vol_level[2] = { 10, 10 };   // [0] music, [1] effects: 1-10
static uint8_t vol_on[2] = { 1, 1 };        // 0: muted

static void draw_button(int16_t x, int16_t y, uint8_t w, const char *label, uint16_t c) {
    tft_frame(x, y - 4, w, 22, 1, c);
    tft_text_P(x + 8, y, label, c, C_BLACK, 2);
}

static void draw_volume_row(uint8_t k, int16_t y, uint16_t fg) {
    tft_text_P(MENU_X, y, k ? PSTR("FX VOL:") : PSTR("MUSIC VOL:"), fg, C_BLACK, 2);
    draw_button(VOL_MINUS_X, y, BTN_W, PSTR("-"), C_GRAY);
    tft_number(VOL_NUM_X, y, vol_level[k], 2, vol_on[k] ? fg : C_GRAY, C_BLACK, 2);
    draw_button(VOL_PLUS_X, y, BTN_W, PSTR("+"), C_GRAY);
    draw_button(VOL_MUTE_X, y, MUTE_W, PSTR("MUTE"), vol_on[k] ? C_GRAY : C_RED);
}

// Hand the volume settings to the sound task
static void apply_volumes() {
    sound_music_enable(vol_on[0]);
    sound_music_volume(vol_level[0]);
    sound_sfx_enable(vol_on[1]);
    sound_sfx_volume(vol_level[1]);
}

#if TAPDBG
static void draw_on_off(int16_t x, int16_t y, uint8_t on, uint16_t fg) {
    tft_text_P(x, y, on ? PSTR("ON ") : PSTR("OFF"), fg, C_BLACK, 2);
}
#endif

static void draw_menu_item(uint8_t item) {
    const int16_t y = menu_y(item);
    const uint16_t fg = (item == menu_sel) ? C_YELLOW : C_WHITE;
    tft_char(MENU_X - 24, y, item == menu_sel ? '>' : ' ', C_YELLOW, C_BLACK, 2);
    switch (item) {
        case MENU_PLAY:   tft_text_P(MENU_X, y, PSTR("PLAY"), fg, C_BLACK, 2); break;
        case MENU_SCORES: tft_text_P(MENU_X, y, PSTR("HIGH SCORES"), fg, C_BLACK, 2); break;
        case MENU_MUSIC:  draw_volume_row(0, y, fg); break;
        case MENU_SFX:    draw_volume_row(1, y, fg); break;
#if TOUCH_ENABLED
        case MENU_TOUCH:  tft_text_P(MENU_X, y, PSTR("TOUCH SETUP"), fg, C_BLACK, 2); break;
#endif
#if TAPDBG
        case MENU_DRAW:
            tft_text_P(MENU_X, y, PSTR("DRAW VISUAL TOUCH:"), fg, C_BLACK, 2);
            draw_on_off(MENU_X + 228, y, tapdbg_enabled(), fg);
            break;
#endif
    }
}

static void menu_move(uint8_t to) {
    if (to == menu_sel) return;
    const uint8_t from = menu_sel;
    menu_sel = to;
    draw_menu_item(from);
    draw_menu_item(to);
}

// A volume row: by -1 or +1 changes the level (and unmutes), 0 flips mute.
// Saved, and a blip at the new level.
static void volume_change(uint8_t item, int8_t by) {
    const uint8_t k = item == MENU_MUSIC ? 0 : 1;
    if (by == 0) {
        vol_on[k] = !vol_on[k];
    } else {
        const int8_t v = vol_level[k] + by;
        if (v < 1 || v > 10) return;
        vol_level[k] = v;
        vol_on[k] = 1;
    }
    apply_volumes();
    if (k == 0) { setting_music_set(vol_on[0]); setting_music_vol_set(vol_level[0]); }
    else        { setting_sfx_set(vol_on[1]);   setting_sfx_vol_set(vol_level[1]); }
    if (vol_on[k]) sound_preview(vol_level[k]);
    draw_menu_item(item);
}

static uint8_t is_volume(uint8_t item) { return item == MENU_MUSIC || item == MENU_SFX; }

// Where the chosen item leads: SYS_PLAY, SYS_SCORES, SYS_TOUCH_SETUP, or
// SYS_TITLE (an ON/OFF item: flipped, and stay)
static uint8_t menu_choose(uint8_t item) {
    switch (item) {
        case MENU_PLAY:   return SYS_PLAY;
        case MENU_SCORES: return SYS_SCORES;
#if TOUCH_ENABLED
        case MENU_TOUCH:  return SYS_TOUCH_SETUP;
#endif
        case MENU_MUSIC:
        case MENU_SFX:
            volume_change(item, 0);            // a click mutes or unmutes; the marker stays
            return SYS_TITLE;
#if TAPDBG
        case MENU_DRAW: {
            const uint8_t on = !tapdbg_enabled();
            tapdbg_enable(on);
            setting_draw_touch_set(on);
            break;
        }
#endif
    }
    sound_sfx(SFX_EAT);                            // a blip, so you know it took
    const uint8_t was = menu_sel;
    menu_sel = MENU_PLAY;                          // the marker goes back to PLAY:
    draw_menu_item(was);                           // the next click starts the game
    draw_menu_item(item);
    draw_menu_item(MENU_PLAY);
    return SYS_TITLE;
}

// Title screen: the name and the menu
static void draw_title() {
    tft_fill_screen(C_BLACK);
    tft_text_center_P(24, PSTR("SNAKE"), C_GREEN, C_BLACK, 6);
    for (uint8_t i = 0; i < MENU_ITEMS; i++) draw_menu_item(i);
    tft_text_center_P(272, PSTR("STICK UP/DOWN + CLICK, OR TAP"), C_GRAY, C_BLACK, 2);
    tft_text_center_P(294, PSTR("PAUSE: CLICK, OR TAP THE TOP BAR"), C_GRAY, C_BLACK, 2);
}

// ---- High scores page: the top-3 table from EEPROM, with initials ----
#define SCORES_X 123                  // "1. ABC  00000" at scale 3 (13 x 18 px), centered

static void enter_scores(int8_t mark) {   // mark: a row to show in yellow (a new score), or -1
    tft_fill_screen(C_BLACK);
    tft_text_center_P(40, PSTR("HIGH SCORES"), C_YELLOW, C_BLACK, 4);
    for (uint8_t r = 0; r < SCORE_SLOTS; r++) {
        const int16_t y = 110 + r * 44;
        const uint16_t c = (int8_t)r == mark ? C_YELLOW : C_WHITE;
        char nm[3];
        score_name_get(GAME_SNAKE, r, nm);
        tft_char(SCORES_X, y, '1' + r, c, C_BLACK, 3);
        tft_char(SCORES_X + 18, y, '.', c, C_BLACK, 3);
        for (uint8_t k = 0; k < 3; k++) tft_char(SCORES_X + 54 + k * 18, y, nm[k], c, C_BLACK, 3);
        tft_number(SCORES_X + 144, y, score_get(GAME_SNAKE, r), 5, c, C_BLACK, 3);
    }
    tft_text_center_P(282, PSTR("CLICK OR TAP: BACK"), C_GRAY, C_BLACK, 2);
    input_clear();
}

// ---- Initials for a new top-3 score ----
// Stick up/down changes a letter, left/right moves between them, a click
// moves on (on the last letter it saves). Or tap the arrows above and
// below a letter, then SAVE. After 30 s untouched, what's there is saved.
#define NAME_X0    162                // three letters at scale 6 (36 px), 60 px apart
#define NAME_STEP  60
#define NAME_Y     118
#define SAVE_Y     214
#define NAME_IDLE  1500               // 30 s
static char name[3] = { 'A', 'A', 'A' };   // the last initials entered: the next ones start there
static uint8_t  name_pos;
static uint16_t name_idle;

static void draw_letter(uint8_t i) {
    tft_char(NAME_X0 + i * NAME_STEP, NAME_Y, name[i], i == name_pos ? C_YELLOW : C_WHITE, C_BLACK, 6);
}

static void draw_arrows(uint8_t i) {          // small triangles above and below letter i
    const int16_t cx = NAME_X0 + i * NAME_STEP + 15;
    for (uint8_t k = 0; k < 9; k++) {
        tft_fill_rect(cx - k - 1, NAME_Y - 18 + k, 2 * k + 2, 1, C_GRAY);   // pointing up
        tft_fill_rect(cx - k - 1, NAME_Y + 64 - k, 2 * k + 2, 1, C_GRAY);   // pointing down
    }
}

static void name_select(uint8_t i) {
    const uint8_t was = name_pos;
    name_pos = i;
    draw_letter(was);
    draw_letter(i);
}

static void name_bump(uint8_t i, int8_t by) {
    name[i] = 'A' + (name[i] - 'A' + 26 + by) % 26;
    draw_letter(i);
    sound_sfx(SFX_EAT);
}

static void enter_name() {
    snake_active = 0;
    tft_fill_screen(C_BLACK);
    tft_text_center_P(20, snake_rank == 0 ? PSTR("NEW HIGH SCORE!") : PSTR("TOP 3 SCORE!"), C_YELLOW, C_BLACK, 3);
    tft_text_center_P(62, PSTR("ENTER YOUR INITIALS"), C_WHITE, C_BLACK, 2);
    name_pos = 0;
    name_idle = 0;
    for (uint8_t i = 0; i < 3; i++) { draw_arrows(i); draw_letter(i); }
    tft_frame(186, SAVE_Y - 8, 108, 30, 2, C_WHITE);
    tft_text_center_P(SAVE_Y, PSTR("SAVE"), C_WHITE, C_BLACK, 2);
    tft_text_center_P(258, PSTR("STICK: UP/DOWN LETTER, CLICK NEXT"), C_GRAY, C_BLACK, 2);
    tft_text_center_P(280, PSTR("OR TAP THE ARROWS, THEN SAVE"), C_GRAY, C_BLACK, 2);
    input_clear();
}

// One tick of the initials screen. Returns 1 when they're done.
static uint8_t name_tick() {
    uint8_t act = 0;
    const uint8_t ev = input_take(EV_DIRS);
    if (ev) act = 1;
    if (ev & EV_UP)                     name_bump(name_pos, +1);
    if (ev & EV_DOWN)                   name_bump(name_pos, -1);
    if ((ev & EV_LEFT) && name_pos > 0)  name_select(name_pos - 1);
    if ((ev & EV_RIGHT) && name_pos < 2) name_select(name_pos + 1);
    if (input_take(EV_SW)) {
        if (name_pos == 2) return 1;
        act = 1;
        name_select(name_pos + 1);
    }
    for (uint8_t i = 0; i < 3; i++) {             // taps: the arrows, or the letter itself
        const int16_t x0 = NAME_X0 + i * NAME_STEP - 12, x1 = x0 + NAME_STEP;
        if (input_take_tap(x0, NAME_Y - 38, x1, NAME_Y))      { act = 1; name_select(i); name_bump(i, +1); }
        if (input_take_tap(x0, NAME_Y, x1, NAME_Y + 48))      { act = 1; name_select(i); }
        if (input_take_tap(x0, NAME_Y + 48, x1, NAME_Y + 82)) { act = 1; name_select(i); name_bump(i, -1); }
    }
    if (input_take_tap(170, SAVE_Y - 10, 310, SAVE_Y + 26)) return 1;
    if (input_take(EV_TAP)) act = 1;
    if (act) name_idle = 0;
    else if (++name_idle >= NAME_IDLE) return 1;
    return 0;
}

// ---- Demo: after 20 s untouched on the title screen, the snake plays itself ----
#define DEMO_IDLE 1000                // 20 s of 20 ms ticks
#define DEMO_TIME 10000                // 200 s of 20 ms ticks, then back to the title screen
static uint16_t idle_ticks, demo_ticks;

// ---- Game over: the box Snake drew stays up until a press, or 5 s ----
#define OVER_GUARD 25                 // 0.5 s: presses ignored while the box comes up
#define OVER_SHOW  250                // 5 s: then back to the menu by itself
static uint8_t over_ticks;

static void start_game() {
    snake_active = 1;
    snake_restart = 1;            // Snake draws a fresh game on its next tick
    sys_paused = 0;
    sound_set_pause(0);
    input_clear();
}

static void enter_title() {
    snake_active = 0;
    idle_ticks = 0;
    sys_paused = 0;
    sound_set_pause(0);
    sound_music(0);
    input_clear();
    menu_sel = MENU_PLAY;
    draw_title();
}

static void start_demo() {
    snake_demo = 1;
    ui_set_demo(1);
    sound_set_mute(1);                // a quiet demo
    demo_ticks = 0;
    start_game();
}

static void end_demo() {
    snake_demo = 0;
    ui_set_demo(0);
    sound_set_mute(0);
}

static void set_pause(uint8_t on) {
    sys_paused = on;
    sound_set_pause(on);
    ui_pause_banner(on);
    input_clear();
}

#if TOUCH_ENABLED
// ---- Touch setup ----
// Comes up by itself until the touch screen has been calibrated, from the
// title menu's TOUCH SETUP, and if you hold the stick pressed while
// powering up. Touch three crosses in
// turn; touch_calibrate() works out the rest and saves it. Then a dot
// follows your finger, so you can see it worked.
static const int16_t CAL_X[3] = { TOUCH_CAL_LEFT, TOUCH_CAL_RIGHT, TOUCH_CAL_RIGHT };
static const int16_t CAL_Y[3] = { TOUCH_CAL_TOP,  TOUCH_CAL_TOP,   TOUCH_CAL_BOTTOM };
#define CAL_DONE 3
static uint8_t  cal_step;             // the cross to touch, or CAL_DONE: checking
static uint16_t cal_raw[3][2];        // the readings at each cross
static uint32_t cal_sum[2];           // the readings added up while the finger is down
static uint8_t  cal_n;
static uint8_t  cal_armed;            // the finger has been lifted since the screen came up

static void draw_cross(uint8_t i, uint16_t color) {
    tft_fill_rect(CAL_X[i] - 10, CAL_Y[i], 21, 1, color);
    tft_fill_rect(CAL_X[i], CAL_Y[i] - 10, 1, 21, color);
}

static void cal_message(const char *line1, const char *line2) {
    tft_fill_rect(0, 150, TFT_W, 40, C_BLACK);
    tft_text_center_P(150, line1, C_WHITE, C_BLACK, 2);
    tft_text_center_P(174, line2, C_WHITE, C_BLACK, 2);
}

static void cal_restart() {
    cal_step = 0;
    cal_n = 0;
    cal_sum[0] = cal_sum[1] = 0;
    draw_cross(0, C_WHITE);
}

static void enter_touch_setup() {
    snake_active = 0;
    sound_music(0);
    tft_fill_screen(C_BLACK);
    tft_text_center_P(100, PSTR("TOUCH SETUP"), C_YELLOW, C_BLACK, 3);
    cal_message(PSTR("TOUCH EACH CROSS AS IT APPEARS"), PSTR(""));
    tft_text_center_P(240, PSTR("STICK CLICK: EXIT   RESET: REDO"), C_GRAY, C_BLACK, 2);
    cal_restart();
    cal_armed = 0;
    input_clear();
}

static void update_touch_setup() {
    if (!cal_armed) {                                // a finger still down from the menu tap
        cal_armed = !in_touch;                       // isn't touching the first cross
        return;
    }
    if (cal_step == CAL_DONE) {                      // checking: a dot under the finger
        if (in_touch) tft_fill_rect(in_touch_x - 2, in_touch_y - 2, 5, 5, C_CYAN);
        return;
    }
    if (in_touch) {                                  // finger down: add up its readings
        if (cal_n < 200) {
            cal_sum[0] += touch_raw_x;
            cal_sum[1] += touch_raw_y;
            cal_n++;
        }
        return;
    }
    if (cal_n == 0) return;                          // waiting for a touch
    cal_raw[cal_step][0] = cal_sum[0] / cal_n;       // finger lifted: that cross is done
    cal_raw[cal_step][1] = cal_sum[1] / cal_n;
    cal_n = 0;
    cal_sum[0] = cal_sum[1] = 0;
    draw_cross(cal_step, C_BLACK);
    if (++cal_step < CAL_DONE) {
        sound_sfx(SFX_EAT);
        draw_cross(cal_step, C_WHITE);
    } else if (touch_calibrate(cal_raw)) {
        sound_sfx(SFX_GOLD);
        cal_message(PSTR("SAVED. TOUCH ANYWHERE: THE DOT"), PSTR("SHOULD LAND UNDER YOUR FINGER"));
    } else {
        sound_sfx(SFX_CRASH);
        cal_message(PSTR("MISSED ONE. TOUCH EACH CROSS"), PSTR("AGAIN, AS IT APPEARS"));
        cal_restart();
    }
}
#endif

// =====================================================================
// System task, period 20 ms. Owns the screens: title menu, high scores,
// initials, the demo, touch setup, pause, game over, and the RESET button.
//   SYS_INIT  --> SYS_TITLE
//             --(no touch calibration saved yet, or stick held at power-up)--> SYS_TOUCH_SETUP
//   SYS_TITLE --(PLAY chosen, or RESET)--> SYS_PLAY
//             --(HIGH SCORES chosen)--> SYS_SCORES
//             --(TOUCH SETUP chosen)--> SYS_TOUCH_SETUP
//             --(20 s untouched)--> SYS_DEMO
//             (stick up/down moves the menu marker; on MUSIC VOL and FX VOL,
//              left/right set the level and a click mutes; DRAW VISUAL TOUCH
//              flips and hands the marker back to PLAY)
//   SYS_DEMO  --(any press, the demo's game over, or 40 s)--> SYS_TITLE
//             --(RESET button)--> SYS_PLAY
//   SYS_SCORES --(stick press or tap)--> SYS_TITLE
//              --(RESET button)--> SYS_PLAY
//   SYS_PLAY  --(RESET button)--> SYS_PLAY (fresh game)
//             --(stick press or tap on the status bar)--> SYS_PAUSE
//             --(Snake put up the game-over box)--> SYS_OVER
//   SYS_PAUSE --(stick press or tap anywhere)--> SYS_PLAY
//             --(RESET button)--> SYS_PLAY (fresh game)
//   SYS_OVER  --(stick press or tap, once the box has been up 0.5 s; or 5 s)--> SYS_NAME
//                  (a top-3 score), or SYS_TITLE
//             --(RESET button)--> SYS_PLAY (fresh game)
//   SYS_NAME  --(initials saved, or 30 s untouched)--> SYS_SCORES
//             --(RESET button)--> SYS_PLAY
//   SYS_TOUCH_SETUP --(stick press)--> SYS_TITLE
//                   --(RESET button)--> SYS_TOUCH_SETUP (start over)
// =====================================================================
int TickFct_System(int state) {
    switch (state) {                       // transitions, with their one-time actions
        case SYS_INIT:
            vol_on[0] = setting_music();           // the saved settings
            vol_level[0] = setting_music_vol();
            vol_on[1] = setting_sfx();
            vol_level[1] = setting_sfx_vol();
            apply_volumes();
            tapdbg_enable(setting_draw_touch());
#if TOUCH_ENABLED
            if (!touch_calibrated() || input_sw_down()) {
                enter_touch_setup();
                state = SYS_TOUCH_SETUP;
                break;
            }
#endif
            enter_title();
            state = SYS_TITLE;
            break;

        case SYS_TITLE: {
            uint8_t next = input_take(EV_RESET) ? SYS_PLAY : SYS_TITLE;   // RESET always starts a game
            const uint8_t ev = input_take(EV_DIRS);
            if ((ev & EV_UP) && menu_sel > 0)                menu_move(menu_sel - 1);
            if ((ev & EV_DOWN) && menu_sel < MENU_ITEMS - 1) menu_move(menu_sel + 1);
            if ((ev & EV_LEFT) && is_volume(menu_sel))       volume_change(menu_sel, -1);
            if ((ev & EV_RIGHT) && is_volume(menu_sel))      volume_change(menu_sel, +1);
            const uint8_t sw = input_take(EV_SW);
            if (sw) {
                const uint8_t n = menu_choose(menu_sel);
                if (n != SYS_TITLE) next = n;
            }
            for (uint8_t i = 0; i < MENU_ITEMS; i++) {     // a tap on an item picks it
                if (input_take_tap(0, menu_y(i) - 7, TFT_W, menu_y(i) + 21)) {
                    if (is_volume(i)) {                    // the buttons on a volume row
                        menu_move(i);
                        if (in_tap_x >= VOL_MINUS_X && in_tap_x < VOL_MINUS_X + BTN_W)     volume_change(i, -1);
                        else if (in_tap_x >= VOL_PLUS_X && in_tap_x < VOL_PLUS_X + BTN_W)  volume_change(i, +1);
                        else if (in_tap_x >= VOL_MUTE_X && in_tap_x < VOL_MUTE_X + MUTE_W) volume_change(i, 0);
                    } else {
                        const uint8_t n = menu_choose(i);
                        if (n != SYS_TITLE) next = n;
                    }
                }
            }
            input_take(EV_TAP);                            // taps anywhere else do nothing
            if (ev || sw || in_touch || in_dir != DIR_NONE) idle_ticks = 0;
            else if (++idle_ticks >= DEMO_IDLE && next == SYS_TITLE) { start_demo(); next = SYS_DEMO; }
            if (next == SYS_PLAY)   start_game();
            if (next == SYS_SCORES) enter_scores(-1);
#if TOUCH_ENABLED
            if (next == SYS_TOUCH_SETUP) enter_touch_setup();
#endif
            state = next;
            break;
        }

        case SYS_SCORES:
            if (input_take(EV_RESET))            { start_game(); state = SYS_PLAY; }
            else if (input_take(EV_SW | EV_TAP)) { enter_title(); state = SYS_TITLE; }
            break;

        case SYS_DEMO: {
            const uint8_t reset = input_take(EV_RESET);
            const uint8_t press = input_take(EV_SW | EV_DIRS | EV_TAP) || in_dir != DIR_NONE;
            if (reset || press || snake_over || ++demo_ticks >= DEMO_TIME) {
                end_demo();
                if (reset) { start_game(); state = SYS_PLAY; }
                else       { enter_title(); state = SYS_TITLE; }
            }
            break;
        }

        case SYS_NAME:
            if (input_take(EV_RESET)) {
                start_game();              // skipped: the score stays, as "---"
                state = SYS_PLAY;
            } else if (name_tick()) {
                score_name_set(GAME_SNAKE, snake_rank, name);
                sound_sfx(SFX_GOLD);
                enter_scores(snake_rank);
                state = SYS_SCORES;
            }
            break;

        case SYS_PLAY:
            if (input_take(EV_RESET)) {
                start_game();
                state = SYS_PLAY;
            } else if (snake_over) {
                over_ticks = 0;            // the game-over box is up
                state = SYS_OVER;
            } else if (input_take(EV_SW) || input_take_tap(0, 0, TFT_W, HUD_H)) {
                set_pause(1);
                state = SYS_PAUSE;
            }
            break;

        case SYS_PAUSE:
            if (input_take(EV_RESET))     { start_game(); state = SYS_PLAY; }
            else if (input_take(EV_SW | EV_TAP)) { set_pause(0); state = SYS_PLAY; }
            break;

        case SYS_OVER: {
            if (input_take(EV_RESET)) {
                start_game();
                state = SYS_PLAY;
                break;
            }
            const uint8_t press = input_take(EV_SW | EV_TAP) != 0;   // too soon: thrown away
            if (over_ticks < OVER_SHOW) over_ticks++;
            if ((press && over_ticks > OVER_GUARD) || over_ticks >= OVER_SHOW) {
                if (snake_rank >= 0) { enter_name(); state = SYS_NAME; }   // a top-3 score: initials
                else                 { enter_title(); state = SYS_TITLE; }
            }
            break;
        }

#if TOUCH_ENABLED
        case SYS_TOUCH_SETUP:
            if (input_take(EV_SW)) {
                enter_title();             // done (or skipped: config.h's numbers until next time)
                state = SYS_TITLE;
            } else if (input_take(EV_RESET)) {
                enter_touch_setup();
            } else {
                update_touch_setup();      // taps and stick moves don't matter here; the
            }                              // title screen clears them
            break;
#endif

        default:
            state = SYS_INIT;
            break;
    }
    return state;
}
