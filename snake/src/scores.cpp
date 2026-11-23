#include "scores.h"
#include "game.h"
#include <avr/eeprom.h>

#define EE_MAGIC_ADDR ((uint8_t *)0)
#define EE_MAGIC      0x5B              // change to wipe the table on the next boot
#define EE_MUSIC_ADDR ((uint8_t *)1)   // 0 = off; anything else (an erased EEPROM reads 0xFF) = on
#define EE_MVOL_ADDR  ((uint8_t *)12)  // music volume 1-10 (anything else: 10)
#define EE_FVOL_ADDR  ((uint8_t *)13)  // effects volume 1-10
#define EE_SFX_ADDR   ((uint8_t *)14)  // 0 = effects muted; anything else = on
#define EE_DRAW_ADDR  ((uint8_t *)15)  // 1 = on; anything else (0xFF when erased) = off
#define EE_TABLE      2
#define EE_NAMES      32               // 3 letters per score, in the table's order

static uint16_t *slot(uint8_t game, uint8_t rank) {
    return (uint16_t *)(uintptr_t)(EE_TABLE + (game * SCORE_SLOTS + rank) * 2);
}
static uint8_t *name_slot(uint8_t game, uint8_t rank) {
    return (uint8_t *)(uintptr_t)(EE_NAMES + (game * SCORE_SLOTS + rank) * 3);
}
static_assert(EE_TABLE + GAME_COUNT * SCORE_SLOTS * 2 <= 12, "score table runs into the settings");
static_assert(EE_NAMES >= 26, "names run into the touch calibration");

void scores_init() {
    if (eeprom_read_byte(EE_MAGIC_ADDR) != EE_MAGIC) {
        for (uint8_t g = 0; g < GAME_COUNT; g++)
            for (uint8_t r = 0; r < SCORE_SLOTS; r++) {
                eeprom_update_word(slot(g, r), 0);
                for (uint8_t c = 0; c < 3; c++) eeprom_update_byte(name_slot(g, r) + c, '-');
            }
        eeprom_update_byte(EE_MUSIC_ADDR, 1);
        eeprom_update_byte(EE_SFX_ADDR, 1);
        eeprom_update_byte(EE_MVOL_ADDR, 10);
        eeprom_update_byte(EE_FVOL_ADDR, 10);
        eeprom_update_byte(EE_DRAW_ADDR, 0);
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

static uint8_t volume(uint8_t *addr) {
    const uint8_t v = eeprom_read_byte(addr);
    return v >= 1 && v <= 10 ? v : 10;
}
uint8_t setting_music_vol() { return volume(EE_MVOL_ADDR); }
uint8_t setting_sfx_vol()   { return volume(EE_FVOL_ADDR); }
void setting_music_vol_set(uint8_t v) { eeprom_update_byte(EE_MVOL_ADDR, v); }
void setting_sfx_vol_set(uint8_t v)   { eeprom_update_byte(EE_FVOL_ADDR, v); }

uint8_t setting_sfx() {
    return eeprom_read_byte(EE_SFX_ADDR) != 0;
}

void setting_sfx_set(uint8_t on) {
    eeprom_update_byte(EE_SFX_ADDR, on ? 1 : 0);
}

void score_name_get(uint8_t game, uint8_t rank, char name[3]) {
    for (uint8_t c = 0; c < 3; c++) {
        const uint8_t ch = eeprom_read_byte(name_slot(game, rank) + c);
        name[c] = (ch >= 'A' && ch <= 'Z') ? ch : '-';     // anything else (0xFF when erased): no name
    }
}

void score_name_set(uint8_t game, uint8_t rank, const char name[3]) {
    for (uint8_t c = 0; c < 3; c++) eeprom_update_byte(name_slot(game, rank) + c, name[c]);
}

uint8_t setting_draw_touch() {
    return eeprom_read_byte(EE_DRAW_ADDR) == 1;
}

void setting_draw_touch_set(uint8_t on) {
    eeprom_update_byte(EE_DRAW_ADDR, on ? 1 : 0);
}

int8_t score_submit(uint8_t game, uint16_t score) {
    if (score == 0) return -1;
    for (uint8_t r = 0; r < SCORE_SLOTS; r++) {
        if (score > score_get(game, r)) {
            for (uint8_t k = SCORE_SLOTS - 1; k > r; k--) {        // shift lower scores down,
                eeprom_update_word(slot(game, k), score_get(game, k - 1));
                char name[3];                                      // names with them
                score_name_get(game, k - 1, name);
                score_name_set(game, k, name);
            }
            eeprom_update_word(slot(game, r), score);
            score_name_set(game, r, "---");                       // System asks for the initials
            return r;
        }
    }
    return -1;
}
