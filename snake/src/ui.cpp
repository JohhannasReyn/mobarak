#include "ui.h"
#include "game.h"
#include <avr/pgmspace.h>

// What the status bar shows, so it can be drawn again after a pause
static uint16_t hud_score = 0, hud_hi = 0;
static uint8_t hud_gold = 0, hud_lives = 0;

static void draw_hud() {
    tft_fill_rect(0, 0, TFT_W, HUD_H - 2, C_BLACK);
    tft_fill_rect(0, HUD_H - 2, TFT_W, 2, C_GRAY);
    tft_text_P(8, HUD_TEXT_Y, PSTR("SCORE:"), C_WHITE, C_BLACK, 2);
    tft_number(HUD_SCORE_X, HUD_TEXT_Y, hud_score, 5, C_WHITE, C_BLACK, 2);
    tft_text_P(158, HUD_TEXT_Y, PSTR("LIVES:"), C_WHITE, C_BLACK, 2);
    tft_number(HUD_LIVES_X, HUD_TEXT_Y, hud_lives, 1, C_WHITE, C_BLACK, 2);
    tft_text_P(356, HUD_TEXT_Y, PSTR("HI:"), C_WHITE, C_BLACK, 2);
    tft_number(HUD_HI_X, HUD_TEXT_Y, hud_hi, 5, C_WHITE, C_BLACK, 2);
    if (hud_gold) ui_hud_gold(hud_gold);
}

void ui_hud(uint16_t score, uint16_t hi, uint8_t lives) {
    hud_score = score;
    hud_hi = hi;
    hud_lives = lives;
    hud_gold = 0;
    draw_hud();
}

void ui_hud_score(uint16_t score) {
    hud_score = score;
    tft_number(HUD_SCORE_X, HUD_TEXT_Y, score, 5, C_WHITE, C_BLACK, 2);
}

void ui_hud_lives(uint8_t lives) {
    hud_lives = lives;
    tft_number(HUD_LIVES_X, HUD_TEXT_Y, lives, 1, C_WHITE, C_BLACK, 2);
}

void ui_hud_gold(uint8_t seconds) {
    hud_gold = seconds;
    if (seconds) {
        tft_text_P(HUD_GOLD_X, HUD_TEXT_Y, PSTR("GOLD"), C_YELLOW, C_BLACK, 2);
        tft_number(HUD_GOLD_X + 60, HUD_TEXT_Y, seconds, 2, C_YELLOW, C_BLACK, 2);
    } else {
        tft_fill_rect(HUD_GOLD_X, HUD_TEXT_Y, 7 * 12, 16, C_BLACK);
    }
}

void ui_pause_banner(uint8_t on) {
    if (!on) { draw_hud(); return; }
    tft_fill_rect(0, 0, TFT_W, HUD_H - 2, C_BLACK);
    tft_text_P(8, HUD_TEXT_Y, PSTR("PAUSED"), C_YELLOW, C_BLACK, 2);
    tft_text_P(290, HUD_TEXT_Y, PSTR("CLICK TO RESUME"), C_GRAY, C_BLACK, 2);
}

void ui_game_over(uint8_t won, uint16_t score, int8_t rank) {
    tft_fill_rect(GO_X, GO_Y, GO_W, GO_H, C_BLACK);
    tft_frame(GO_X, GO_Y, GO_W, GO_H, 2, C_WHITE);
    if (won) tft_text_center_P(GO_Y + 16, PSTR("YOU WIN!"), C_GREEN, C_BLACK, 4);
    else     tft_text_center_P(GO_Y + 16, PSTR("GAME OVER"), C_RED, C_BLACK, 4);

    tft_text_P(170, GO_Y + 64, PSTR("SCORE:"), C_WHITE, C_BLACK, 2);
    tft_number(GO_SCORE_X, GO_Y + 64, score, 5, C_WHITE, C_BLACK, 2);
    if (rank == 0)     tft_text_center_P(GO_Y + 88, PSTR("NEW HIGH SCORE!"), C_YELLOW, C_BLACK, 2);
    else if (rank > 0) tft_text_center_P(GO_Y + 88, PSTR("TOP 3 SCORE!"), C_YELLOW, C_BLACK, 2);
    tft_text_center_P(GO_Y + 112, PSTR("PRESS STICK"), C_GRAY, C_BLACK, 2);
}
