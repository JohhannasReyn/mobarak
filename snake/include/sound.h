#pragma once
#include <stdint.h>
#include <avr/pgmspace.h>

// Passive buzzer on D3 driven by Timer2 in fast PWM: the hardware makes the
// tone on OC2B, so it costs no CPU time at all. The period sets the pitch,
// the pulse width the volume.
//
// Songs and sound effects are PROGMEM byte pairs {MIDI note, length in
// steps}, ending with 0xFF. Note 0 is a rest. Music loops; an effect plays
// once over the music, then the music picks up where it left off.

extern const uint8_t SONG_SNAKE[] PROGMEM;
extern const uint8_t SONG_POWER[] PROGMEM;
extern const uint8_t SONG_STAR[] PROGMEM;    // star power: swap in your own notes here

extern const uint8_t SFX_EAT[] PROGMEM;
extern const uint8_t SFX_GOLD[] PROGMEM;
extern const uint8_t SFX_POWERUP[] PROGMEM;
extern const uint8_t SFX_OVER[] PROGMEM;
extern const uint8_t SFX_CRASH[] PROGMEM;   // a life lost
extern const uint8_t SFX_WIN[] PROGMEM;
extern const uint8_t SFX_COUNT[] PROGMEM;   // the countdown's 3, 2, 1
extern const uint8_t SFX_GO[] PROGMEM;      // and go

void sound_init();
void sound_music(const uint8_t *song);   // 0 stops the music
void sound_music_enable(uint8_t on);     // 0: songs stay silent (muted), effects still play
void sound_music_volume(uint8_t v);      // 1-10
void sound_sfx_volume(uint8_t v);        // 1-10
void sound_preview(uint8_t v);           // a blip at volume v (the menu, after a change)
uint8_t sound_music_enabled();
void sound_sfx(const uint8_t *effect);   // nothing while effects are muted
void sound_sfx_enable(uint8_t on);       // 0: effects muted
uint8_t sound_sfx_enabled();
void sound_tempo(uint8_t ms_per_step);   // music speed; games raise it as play speeds up
void sound_set_mute(uint8_t on);
void sound_set_pause(uint8_t on);        // silence without losing the music position
uint8_t sound_muted();
const uint8_t *sound_current_music();
uint16_t sound_frequency();              // Hz now on the buzzer, 0 = silent

enum Sound_States { SND_INIT, SND_SILENT, SND_MUSIC, SND_SFX };
int TickFct_Sound(int state);
