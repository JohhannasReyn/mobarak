#pragma once
#include <stdint.h>

// xorshift32. The input task stirs in ADC noise every tick, so each game
// plays out differently.
uint16_t rng_next();
uint8_t  rng_below(uint8_t n);      // 0 .. n-1
void     rng_stir(uint16_t noise);
