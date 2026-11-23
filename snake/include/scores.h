#pragma once
#include <stdint.h>

// Top-3 high-score table per game in the ATmega328P's EEPROM, kept
// across power cycles, with the players' initials, plus the menu's
// settings. Layout: magic byte at 0, music on (not muted) at 1, then
// 3 x uint16 per game from 2; music and effects volume at 12 and 13,
// effects on at 14, DRAW VISUAL TOUCH at 15; the touch calibration at
// 16-25 (touch.cpp); 3 letters per score from 32.
#define SCORE_SLOTS 3

void     scores_init();                               // formats the table on first boot
uint16_t score_get(uint8_t game, uint8_t rank);       // rank 0 = best
int8_t   score_submit(uint8_t game, uint16_t score);  // rank it landed at, or -1
// Initials: 3 letters 'A'-'Z'; '-' for a slot without a name yet
void     score_name_get(uint8_t game, uint8_t rank, char name[3]);
void     score_name_set(uint8_t game, uint8_t rank, const char name[3]);

uint8_t  setting_music();                             // 1 = music on (not muted)
uint8_t  setting_music_vol();                         // 1-10
void     setting_music_vol_set(uint8_t v);
uint8_t  setting_sfx_vol();                           // 1-10
void     setting_sfx_vol_set(uint8_t v);
void     setting_music_set(uint8_t on);
uint8_t  setting_sfx();                               // 1 = sound effects on (not muted)
void     setting_sfx_set(uint8_t on);
uint8_t  setting_draw_touch();                        // 1 = the touch debug overlay on
void     setting_draw_touch_set(uint8_t on);
