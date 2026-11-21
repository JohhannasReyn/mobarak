#include "rng.h"

static uint32_t state = 0x2545F491UL;

uint16_t rng_next() {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return (uint16_t)(state >> 8);
}

uint8_t rng_below(uint8_t n) {
    return (uint8_t)(((uint32_t)rng_next() * n) >> 16);
}

void rng_stir(uint16_t noise) {
    state ^= ((uint32_t)noise << 16) | noise;
    if (state == 0) state = 0x2545F491UL;
    rng_next();
}
