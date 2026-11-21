#include "game.h"
#include "tft.h"
#include "ui.h"
#include "input.h"
#include "sound.h"
#include "scores.h"
#include <avr/pgmspace.h>

uint8_t snake_active = 0;
uint8_t snake_restart = 0;
uint8_t sys_paused = 0;

// Title screen: the name, the top-3 table, how to play
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
    tft_text_center_P(236, PSTR("PRESS STICK TO PLAY"), C_YELLOW, C_BLACK, 2);
    tft_text_center_P(270, PSTR("RESET BUTTON: NEW GAME"), C_GRAY, C_BLACK, 2);
    tft_text_center_P(290, PSTR("CLICK THE STICK TO PAUSE"), C_GRAY, C_BLACK, 2);
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
    draw_title();
}

static void set_pause(uint8_t on) {
    sys_paused = on;
    sound_set_pause(on);
    ui_pause_banner(on);
    input_clear();
}

// =====================================================================
// System task, period 20 ms. Owns the title screen, soft reset and pause.
//   SYS_INIT  --> SYS_TITLE
//   SYS_TITLE --(stick press or RESET)--> SYS_PLAY
//   SYS_PLAY  --(RESET button)--> SYS_PLAY (fresh game)
//             --(stick press, while a game is running)--> SYS_PAUSE
//   SYS_PAUSE --(stick press)--> SYS_PLAY
//             --(RESET button)--> SYS_PLAY (fresh game)
// =====================================================================
int TickFct_System(int state) {
    switch (state) {                       // transitions, with their one-time actions
        case SYS_INIT:
            enter_title();
            state = SYS_TITLE;
            break;

        case SYS_TITLE:
            if (input_take(EV_SW | EV_RESET)) {
                start_game();
                state = SYS_PLAY;
            }
            break;

        case SYS_PLAY:
            if (input_take(EV_RESET)) {
                start_game();
                state = SYS_PLAY;
            } else if (!snake_over && input_take(EV_SW)) {
                set_pause(1);              // on the game-over box the press belongs to Snake
                state = SYS_PAUSE;
            }
            break;

        case SYS_PAUSE:
            if (input_take(EV_RESET))     { start_game(); state = SYS_PLAY; }
            else if (input_take(EV_SW))   { set_pause(0); state = SYS_PLAY; }
            break;

        default:
            state = SYS_INIT;
            break;
    }
    return state;
}
