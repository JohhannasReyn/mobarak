#include "scores.h"
#include "game.h"
#include <avr/eeprom.h>

#define EE_MAGIC_ADDR ((uint8_t *)0)
#define EE_MAGIC      0x5B              // change to wipe the table on the next boot
#define EE_MUSIC_ADDR ((uint8_t *)1)   // 0 = off; anything else (an erased EEPROM reads 0xFF) = on
#define EE_TABLE      2

static uint16_t *slot(uint8_t game, uint8_t rank) {
    return (uint16_t *)(uintptr_t)(EE_TABLE + (game * SCORE_SLOTS + rank) * 2);
}

void scores_init() {
    if (eeprom_read_byte(EE_MAGIC_ADDR) != EE_MAGIC) {
        for (uint8_t g = 0; g < GAME_COUNT; g++)
            for (uint8_t r = 0; r < SCORE_SLOTS; r++)
                eeprom_update_word(slot(g, r), 0);
        eeprom_update_byte(EE_MUSIC_ADDR, 1);
        eeprom_update_byte(EE_MAGIC_ADDR, EE_MAGIC);
    }
}

uint16_t score_get(uint8_t game, uint8_t rank) {
    return eeprom_read_word(slot(game, rank));
}

uint8_t setting_music() {
    return eeprom_read_byte(EE_MUSIC_ADDR) != 0;
}

void setting_music_set(uint8_t on) {
    eeprom_update_byte(EE_MUSIC_ADDR, on ? 1 : 0);
}

int8_t score_submit(uint8_t game, uint16_t score) {
    if (score == 0) return -1;
    for (uint8_t r = 0; r < SCORE_SLOTS; r++) {
        if (score > score_get(game, r)) {
            for (uint8_t k = SCORE_SLOTS - 1; k > r; k--)          // shift lower scores down
                eeprom_update_word(slot(game, k), score_get(game, k - 1));
            eeprom_update_word(slot(game, r), score);
            return r;
        }
    }
    return -1;
}
