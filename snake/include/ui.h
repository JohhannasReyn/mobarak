#pragma once
#include <stdint.h>
#include "tft.h"

// Status bar (y 0-31): SCORE, LIVES, the seconds of star power or golden mode left, HI
#define HUD_TEXT_Y  8
#define HUD_SCORE_X 84          // digits after "SCORE:"
#define HUD_LIVES_X 232         // digit after "LIVES:"
#define HUD_GOLD_X  262         // "GOLD 10" or "STAR 08"
#define HUD_HI_X    396         // digits after "HI:"

void ui_hud(uint16_t score, uint16_t hi, uint8_t lives);   // draw the whole status bar (new game)
void ui_hud_score(uint16_t score);
void ui_hud_lives(uint8_t lives);
void ui_hud_gold(uint8_t seconds);                  // golden-mode seconds left; 0 clears it
void ui_set_demo(uint8_t on);                       // 1: the status bar says DEMO, and nothing else
void ui_hud_star(uint8_t seconds);                  // star-power seconds left (shown first); 0 clears it
void ui_pause_banner(uint8_t on);                   // on: PAUSED; off: the status bar again
void ui_game_over(uint8_t won, uint16_t score, int8_t rank);

// Game-over box
#define GO_X        90
#define GO_Y        100
#define GO_W        300
#define GO_H        140
#define GO_SCORE_X  254         // digits after "SCORE:"
