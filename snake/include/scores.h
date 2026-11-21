#pragma once
#include <stdint.h>

// Top-3 high-score table per game in the ATmega328P's EEPROM, kept
// across power cycles. Layout: magic byte, then 3 x uint16 per game.
#define SCORE_SLOTS 3

void     scores_init();                               // formats the table on first boot
uint16_t score_get(uint8_t game, uint8_t rank);       // rank 0 = best
int8_t   score_submit(uint8_t game, uint16_t score);  // rank it landed at, or -1
