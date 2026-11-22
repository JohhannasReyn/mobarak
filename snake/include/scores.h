#pragma once
#include <stdint.h>

// Top-3 high-score table per game in the ATmega328P's EEPROM, kept
// across power cycles, plus the music setting. Layout: magic byte at 0,
// music setting at 1, then 3 x uint16 per game from 2.
#define SCORE_SLOTS 3

void     scores_init();                               // formats the table on first boot
uint16_t score_get(uint8_t game, uint8_t rank);       // rank 0 = best
int8_t   score_submit(uint8_t game, uint16_t score);  // rank it landed at, or -1

uint8_t  setting_music();                             // 1 = music on
void     setting_music_set(uint8_t on);
