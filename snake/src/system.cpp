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

// ---- Title menu ----
// Stick up/down moves the marker, a click chooses; or tap an item.
// The ON/OFF items flip their setting, save it, and hand the marker back
// to PLAY, so the next click starts a game.
#define MENU_X    108
#define MENU_Y0   100                 // the first item; the rest follow MENU_STEP apart
#define MENU_STEP 28
enum Menu_Items {
    MENU_PLAY, MENU_SCORES, MENU_MUSIC,
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

static void draw_on_off(int16_t x, int16_t y, uint8_t on, uint16_t fg) {
    tft_text_P(x, y, on ? PSTR("ON ") : PSTR("OFF"), fg, C_BLACK, 2);
}

static void draw_menu_item(uint8_t item) {
    const int16_t y = menu_y(item);
    const uint16_t fg = (item == menu_sel) ? C_YELLOW : C_WHITE;
    tft_char(MENU_X - 24, y, item == menu_sel ? '>' : ' ', C_YELLOW, C_BLACK, 2);
    switch (item) {
        case MENU_PLAY:   tft_text_P(MENU_X, y, PSTR("PLAY"), fg, C_BLACK, 2); break;
        case MENU_SCORES: tft_text_P(MENU_X, y, PSTR("HIGH SCORES"), fg, C_BLACK, 2); break;
        case MENU_MUSIC:
            tft_text_P(MENU_X, y, PSTR("MUSIC:"), fg, C_BLACK, 2);
            draw_on_off(MENU_X + 84, y, sound_music_enabled(), fg);
            break;
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

// Where the chosen item leads: SYS_PLAY, SYS_SCORES, SYS_TOUCH_SETUP, or
// SYS_TITLE (an ON/OFF item: flipped, and stay)
static uint8_t menu_choose(uint8_t item) {
    switch (item) {
        case MENU_PLAY:   return SYS_PLAY;
        case MENU_SCORES: return SYS_SCORES;
#if TOUCH_ENABLED
        case MENU_TOUCH:  return SYS_TOUCH_SETUP;
#endif
        case MENU_MUSIC: {
            const uint8_t on = !sound_music_enabled();
            sound_music_enable(on);
            setting_music_set(on);
            break;
        }
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

// ---- High scores page: the top-3 table from EEPROM ----
#define SCORES_X 159                  // "1.  00000" at scale 3 (9 x 18 px), centered

static void enter_scores() {
    tft_fill_screen(C_BLACK);
    tft_text_center_P(40, PSTR("HIGH SCORES"), C_YELLOW, C_BLACK, 4);
    for (uint8_t r = 0; r < SCORE_SLOTS; r++) {
        const int16_t y = 110 + r * 44;
        tft_char(SCORES_X, y, '1' + r, C_WHITE, C_BLACK, 3);
        tft_char(SCORES_X + 18, y, '.', C_WHITE, C_BLACK, 3);
        tft_number(SCORES_X + 72, y, score_get(GAME_SNAKE, r), 5, C_WHITE, C_BLACK, 3);
    }
    tft_text_center_P(282, PSTR("CLICK OR TAP: BACK"), C_GRAY, C_BLACK, 2);
    input_clear();
}

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
    sys_paused = 0;
    sound_set_pause(0);
    sound_music(0);
    input_clear();
    menu_sel = MENU_PLAY;
    draw_title();
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
// touch setup, pause, game over, and the RESET button.
//   SYS_INIT  --> SYS_TITLE
//             --(no touch calibration saved yet, or stick held at power-up)--> SYS_TOUCH_SETUP
//   SYS_TITLE --(PLAY chosen, or RESET)--> SYS_PLAY
//             --(HIGH SCORES chosen)--> SYS_SCORES
//             --(TOUCH SETUP chosen)--> SYS_TOUCH_SETUP
//             (stick up/down moves the menu marker; MUSIC and DRAW VISUAL
//              TOUCH flip their setting and hand the marker back to PLAY)
//   SYS_SCORES --(stick press or tap)--> SYS_TITLE
//              --(RESET button)--> SYS_PLAY
//   SYS_PLAY  --(RESET button)--> SYS_PLAY (fresh game)
//             --(stick press or tap on the status bar)--> SYS_PAUSE
//             --(Snake put up the game-over box)--> SYS_OVER
//   SYS_PAUSE --(stick press or tap anywhere)--> SYS_PLAY
//             --(RESET button)--> SYS_PLAY (fresh game)
//   SYS_OVER  --(stick press or tap, once the box has been up 0.5 s; or 5 s)--> SYS_TITLE
//             --(RESET button)--> SYS_PLAY (fresh game)
//   SYS_TOUCH_SETUP --(stick press)--> SYS_TITLE
//                   --(RESET button)--> SYS_TOUCH_SETUP (start over)
// =====================================================================
int TickFct_System(int state) {
    switch (state) {                       // transitions, with their one-time actions
        case SYS_INIT:
            sound_music_enable(setting_music());   // the saved settings
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
            if (input_take(EV_SW)) {
                const uint8_t n = menu_choose(menu_sel);
                if (n != SYS_TITLE) next = n;
            }
            for (uint8_t i = 0; i < MENU_ITEMS; i++) {     // a tap on an item picks it
                if (input_take_tap(0, menu_y(i) - 7, TFT_W, menu_y(i) + 21)) {
                    const uint8_t n = menu_choose(i);
                    if (n != SYS_TITLE) next = n;
                }
            }
            input_take(EV_TAP);                            // taps anywhere else do nothing
            if (next == SYS_PLAY)   start_game();
            if (next == SYS_SCORES) enter_scores();
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
                enter_title();
                state = SYS_TITLE;
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
