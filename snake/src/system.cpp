#include "game.h"
#include "tft.h"
#include "ui.h"
#include "input.h"
#include "sound.h"
#include "scores.h"
#include "touch.h"
#include "config.h"
#include <avr/pgmspace.h>

uint8_t snake_active = 0;
uint8_t snake_restart = 0;
uint8_t sys_paused = 0;

// ---- Title menu: PLAY and MUSIC: ON/OFF ----
// Stick up/down moves the marker, a click chooses; or tap an item.
#define MENU_X  186
#define MENU_Y0 222                   // PLAY
#define MENU_Y1 246                   // MUSIC
enum Menu_Items { MENU_PLAY, MENU_MUSIC, MENU_ITEMS };
static uint8_t menu_sel = MENU_PLAY;

static int16_t menu_y(uint8_t item) { return item == MENU_PLAY ? MENU_Y0 : MENU_Y1; }

static void draw_menu_item(uint8_t item) {
    const int16_t y = menu_y(item);
    const uint16_t fg = (item == menu_sel) ? C_YELLOW : C_WHITE;
    tft_char(MENU_X - 24, y, item == menu_sel ? '>' : ' ', C_YELLOW, C_BLACK, 2);
    if (item == MENU_PLAY) {
        tft_text_P(MENU_X, y, PSTR("PLAY"), fg, C_BLACK, 2);
    } else {
        tft_text_P(MENU_X, y, PSTR("MUSIC:"), fg, C_BLACK, 2);
        tft_text_P(MENU_X + 84, y, sound_music_enabled() ? PSTR("ON ") : PSTR("OFF"), fg, C_BLACK, 2);
    }
}

static void menu_move(uint8_t to) {
    if (to == menu_sel) return;
    const uint8_t from = menu_sel;
    menu_sel = to;
    draw_menu_item(from);
    draw_menu_item(to);
}

// Returns 1 when the chosen item starts the game
static uint8_t menu_choose(uint8_t item) {
    if (item == MENU_PLAY) return 1;
    const uint8_t on = !sound_music_enabled();     // MUSIC: flip it and remember it
    sound_music_enable(on);
    setting_music_set(on);
    sound_sfx(SFX_EAT);                            // a blip, so you know it took
    draw_menu_item(MENU_MUSIC);
    return 0;
}

// Title screen: the name, the top-3 table, the menu
static void draw_title() {
    tft_fill_screen(C_BLACK);
    tft_text_center_P(30, PSTR("SNAKE"), C_GREEN, C_BLACK, 6);
    tft_text_center_P(110, PSTR("HIGH SCORES"), C_WHITE, C_BLACK, 2);
    for (uint8_t r = 0; r < SCORE_SLOTS; r++) {
        const int16_t y = 140 + r * 24;
        tft_char(192, y, '1' + r, C_WHITE, C_BLACK, 2);
        tft_char(204, y, '.', C_WHITE, C_BLACK, 2);
        tft_number(228, y, score_get(GAME_SNAKE, r), 5, C_WHITE, C_BLACK, 2);
    }
    draw_menu_item(MENU_PLAY);
    draw_menu_item(MENU_MUSIC);
    tft_text_center_P(282, PSTR("STICK UP/DOWN + CLICK, OR TAP"), C_GRAY, C_BLACK, 2);
    tft_text_center_P(302, PSTR("PAUSE: CLICK, OR TAP THE TOP BAR"), C_GRAY, C_BLACK, 2);
}

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
// ---- Touch test screen: hold the stick pressed while powering up ----
// Shows the raw touch readings for calibration and leaves a dot wherever
// the code thinks you touched.
#define TEST_LABEL_X 140
#define TEST_NUM_X   272
static uint8_t test_ticks = 0;

static void draw_cross(int16_t x, int16_t y) {
    tft_fill_rect(x - 10, y, 21, 1, C_WHITE);
    tft_fill_rect(x, y - 10, 1, 21, C_WHITE);
}

static void enter_touch_test() {
    snake_active = 0;
    sound_music(0);
    tft_fill_screen(C_BLACK);
    draw_cross(20, 20);                          // the calibration points
    draw_cross(TFT_W - 21, TFT_H - 21);
    tft_text_center_P(56, PSTR("TOUCH TEST"), C_YELLOW, C_BLACK, 3);
    tft_text_center_P(100, PSTR("TOUCH EACH CROSS, COPY THE RAW"), C_WHITE, C_BLACK, 2);
    tft_text_center_P(120, PSTR("NUMBERS INTO CONFIG.H"), C_WHITE, C_BLACK, 2);
    tft_text_P(TEST_LABEL_X, 160, PSTR("RAW X:"), C_WHITE, C_BLACK, 2);
    tft_text_P(TEST_LABEL_X, 184, PSTR("RAW Y:"), C_WHITE, C_BLACK, 2);
    tft_text_P(TEST_LABEL_X, 208, PSTR("PRESSURE:"), C_WHITE, C_BLACK, 2);
    tft_text_center_P(256, PSTR("RESET BUTTON: PLAY"), C_GRAY, C_BLACK, 2);
    test_ticks = 0;
    input_clear();
}

static void update_touch_test() {
    if (in_touch) tft_fill_rect(in_touch_x - 2, in_touch_y - 2, 5, 5, C_CYAN);
    if (++test_ticks < 5) return;                // numbers every 100 ms
    test_ticks = 0;
    tft_number(TEST_NUM_X, 160, touch_raw_x, 4, C_CYAN, C_BLACK, 2);
    tft_number(TEST_NUM_X, 184, touch_raw_y, 4, C_CYAN, C_BLACK, 2);
    tft_number(TEST_NUM_X, 208, touch_pressure, 4, C_CYAN, C_BLACK, 2);
}
#endif

// =====================================================================
// System task, period 20 ms. Owns the title screen, soft reset and pause.
//   SYS_INIT  --> SYS_TITLE
//             --(stick held at power-up)--> SYS_TOUCH_TEST
//   SYS_TITLE --(PLAY chosen, or RESET)--> SYS_PLAY
//             (stick up/down moves the menu marker; MUSIC flips the setting)
//   SYS_PLAY  --(RESET button)--> SYS_PLAY (fresh game)
//             --(stick press or tap on the status bar, while a game is running)--> SYS_PAUSE
//   SYS_PAUSE --(stick press or tap anywhere)--> SYS_PLAY
//             --(RESET button)--> SYS_PLAY (fresh game)
//   SYS_TOUCH_TEST --(RESET button)--> SYS_TITLE
// =====================================================================
int TickFct_System(int state) {
    switch (state) {                       // transitions, with their one-time actions
        case SYS_INIT:
            sound_music_enable(setting_music());   // the saved MUSIC setting
#if TOUCH_ENABLED
            if (input_sw_down()) {
                enter_touch_test();
                state = SYS_TOUCH_TEST;
                break;
            }
#endif
            enter_title();
            state = SYS_TITLE;
            break;

        case SYS_TITLE: {
            uint8_t go = input_take(EV_RESET) != 0;        // RESET always starts a game
            const uint8_t ev = input_take(EV_DIRS);
            if (ev & EV_UP)   menu_move(MENU_PLAY);
            if (ev & EV_DOWN) menu_move(MENU_MUSIC);
            if (input_take(EV_SW)) go |= menu_choose(menu_sel);
            for (uint8_t i = 0; i < MENU_ITEMS; i++) {     // a tap on an item picks it
                if (input_take_tap(0, menu_y(i) - 6, TFT_W, menu_y(i) + 22)) {
                    menu_move(i);
                    go |= menu_choose(i);
                }
            }
            input_take(EV_TAP);                            // taps anywhere else do nothing
            if (go) {
                start_game();
                state = SYS_PLAY;
            }
            break;
        }

        case SYS_PLAY:
            if (input_take(EV_RESET)) {
                start_game();
                state = SYS_PLAY;
            } else if (!snake_over && (input_take(EV_SW) || input_take_tap(0, 0, TFT_W, HUD_H))) {
                set_pause(1);              // on the game-over box the press belongs to Snake
                state = SYS_PAUSE;
            }
            break;

        case SYS_PAUSE:
            if (input_take(EV_RESET))     { start_game(); state = SYS_PLAY; }
            else if (input_take(EV_SW | EV_TAP)) { set_pause(0); state = SYS_PLAY; }
            break;

#if TOUCH_ENABLED
        case SYS_TOUCH_TEST:
            if (input_take(EV_RESET)) {
                enter_title();
                state = SYS_TITLE;
            } else {
                input_take(EV_SW | EV_TAP);        // not used here
                update_touch_test();
            }
            break;
#endif

        default:
            state = SYS_INIT;
            break;
    }
    return state;
}
