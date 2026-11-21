#pragma once
#include <stdint.h>
#include <avr/pgmspace.h>

// Passive buzzer on D3 driven by Timer2 in CTC mode: the hardware toggles
// OC2B every compare match, so a tone costs no CPU time at all.
//
// Songs and sound effects are PROGMEM byte pairs {MIDI note, length in
// steps}, ending with 0xFF. Note 0 is a rest. Music loops; an effect plays
// once over the music, then the music picks up where it left off.

extern const uint8_t SONG_SNAKE[] PROGMEM;
extern const uint8_t SONG_POWER[] PROGMEM;

extern const uint8_t SFX_EAT[] PROGMEM;
extern const uint8_t SFX_GOLD[] PROGMEM;
extern const uint8_t SFX_POWERUP[] PROGMEM;
extern const uint8_t SFX_OVER[] PROGMEM;
extern const uint8_t SFX_WIN[] PROGMEM;

void sound_init();
void sound_music(const uint8_t *song);   // 0 stops the music
void sound_sfx(const uint8_t *effect);
void sound_tempo(uint8_t ms_per_step);   // music speed; games raise it as play speeds up
void sound_set_mute(uint8_t on);
void sound_set_pause(uint8_t on);        // silence without losing the music position
uint8_t sound_muted();
const uint8_t *sound_current_music();
uint16_t sound_frequency();              // Hz now on the buzzer, 0 = silent

enum Sound_States { SND_INIT, SND_SILENT, SND_MUSIC, SND_SFX };
int TickFct_Sound(int state);
