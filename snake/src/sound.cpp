#include "sound.h"
#include <avr/io.h>

// ---- Note names (MIDI numbers) ----
#define G4  67
#define A4  69
#define B4  71
#define C4  60
#define D4  62
#define E4  64
#define C5  72
#define D5  74
#define E5  76
#define F5  77
#define G5  79
#define A5  81
#define B5  83
#define C6  84
#define D6  86
#define E6  88
#define REST 0
#define END 0xFF

// ---- Songs (original tunes), 1 step = a 16th note at the current tempo ----
const uint8_t SONG_SNAKE[] PROGMEM = {
    A4,2, C5,2, E5,2, A5,2, G5,2, E5,2, D5,2, E5,2,
    A4,2, C5,2, E5,2, G5,2, A5,4, REST,4,
    D5,2, E5,2, G5,2, A5,2, G5,2, E5,2, D5,2, C5,2,
    D5,2, C5,2, A4,2, G4,2, A4,4, REST,4,
    END
};
const uint8_t SONG_POWER[] PROGMEM = {          // golden-apple mode
    C6,1, G5,1, E5,1, G5,1, C6,1, E6,1, D6,1, C6,1,
    B5,1, G5,1, D5,1, G5,1, B5,1, D6,1, C6,2,
    END
};
const uint8_t SONG_STAR_ALT[] PROGMEM = {       // Super Mario Starman theme
    C5,2, C5,2, C5,2, D4,1, C5,2, C5,1, D4,1, C5,1, D4,1, C5,2, REST,1,
    B4,2, B4,2, B4,2, C4,1, B4,2, B4,1, C4,1, B4,1, C4,1, B4,2, REST,1,
    END
};
const uint8_t SONG_STAR[] PROGMEM = {           // star power: quick rising arpeggios
    C5,1, E5,1, G5,1, C6,1, E6,1, C6,1, G5,1, E5,1,
    D5,1, F5,1, A5,1, D6,1, A5,1, F5,1, D5,1, F5,1,
    E5,1, G5,1, B5,1, E6,1, B5,1, G5,1, E5,1, G5,1,
    F5,1, A5,1, C6,1, A5,1, G5,1, B5,1, D6,1, B5,1,
    END
};
// ---- Sound effects, 1 step = 40 ms ----
const uint8_t SFX_EAT[]     PROGMEM = { E5,1, C6,1, END };
const uint8_t SFX_GOLD[]    PROGMEM = { G5,1, REST,1, G5,1, END };
const uint8_t SFX_POWERUP[] PROGMEM = { C5,1, E5,1, G5,1, C6,1, E6,2, END };
const uint8_t SFX_OVER[]    PROGMEM = { C5,3, G4,3, E4,3, C4,6, END };
const uint8_t SFX_CRASH[]   PROGMEM = { E5,2, C5,2, G4,4, END };
const uint8_t SFX_WIN[]     PROGMEM = { C5,2, E5,2, G5,2, C6,6, END };

#define SFX_STEP_MS 40
#define TICK_MS     10
#define GAP_MS      10        // short silence at the end of each note

// Octave 7 frequencies (C7..B7); lower octaves are right shifts.
static const uint16_t octave7[12] PROGMEM = {
    2093, 2217, 2349, 2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951
};
static const uint16_t prescalers[7] PROGMEM = { 1, 8, 32, 64, 128, 256, 1024 };

// ---- Shared with the games ----
static const uint8_t *music = 0;     // song start
static const uint8_t *mpos = 0;      // next music pair
static uint16_t m_left = 0;          // ms left in the current music note
static uint8_t  m_note = 0;
static uint16_t m_len = 0;
static const uint8_t *sfx = 0;       // next effect pair (0 = none)
static uint16_t s_left = 0;
static uint8_t  s_note = 0;
static uint16_t s_len = 0;
static uint8_t  tempo = 100;
static uint8_t  muted = 0, paused = 0;
static uint8_t  music_on = 1;        // the title menu's MUSIC setting
static uint8_t  playing_note = 0;    // note on the buzzer right now
static uint16_t playing_hz = 0;

void sound_music(const uint8_t *song) { music = music_on ? song : 0; mpos = music; m_left = 0; }
void sound_music_enable(uint8_t on) { music_on = on; if (!on) { music = 0; mpos = 0; } }
uint8_t sound_music_enabled() { return music_on; }
void sound_sfx(const uint8_t *effect) { sfx = effect; s_left = 0; }
void sound_tempo(uint8_t ms) { tempo = ms < 30 ? 30 : ms; }
void sound_set_mute(uint8_t on) { muted = on; }
void sound_set_pause(uint8_t on) { paused = on; }
uint8_t sound_muted() { return muted; }
const uint8_t *sound_current_music() { return music; }
uint16_t sound_frequency() { return playing_hz; }

static uint16_t note_hz(uint8_t note) {
    int8_t octave = note / 12 - 1;              // MIDI: C4 = 60
    if (octave > 7) octave = 7;
    if (octave < 2) octave = 2;
    return pgm_read_word(&octave7[note % 12]) >> (7 - octave);
}

static void buzzer_off() {
    TCCR2A = 0;                                 // release OC2B
    TCCR2B = 0;
    PORTD &= ~(1 << PD3);
    playing_note = 0;
    playing_hz = 0;
}

static void buzzer_note(uint8_t note) {
    if (note == 0) { buzzer_off(); return; }
    if (note == playing_note) return;           // already sounding: don't restart the timer
    const uint16_t hz = note_hz(note);
    for (uint8_t i = 0; i < 7; i++) {
        const uint32_t top = F_CPU / (2UL * pgm_read_word(&prescalers[i]) * hz) - 1;
        if (top <= 255) {
            TCCR2B = 0;
            TCNT2  = 0;
            OCR2A  = (uint8_t)top;              // CTC top: sets the frequency
            OCR2B  = 0;
            TCCR2A = (1 << COM2B0) | (1 << WGM21);   // toggle OC2B on match, CTC
            TCCR2B = i + 1;                     // CS22:0 = prescaler index + 1
            playing_note = note;
            playing_hz = hz;
            return;
        }
    }
    buzzer_off();
}

void sound_init() {
    DDRD |= (1 << PD3);
    buzzer_off();
}

// Advance one track by TICK_MS. Returns 0 when a one-shot track ends.
static uint8_t advance(const uint8_t **pos, uint16_t *left, uint8_t *note, uint16_t *len,
                       uint16_t step_ms, const uint8_t *loop_start) {
    if (*left == 0) {
        uint8_t n = pgm_read_byte(*pos);
        if (n == END) {
            if (!loop_start) return 0;
            *pos = loop_start;
            n = pgm_read_byte(*pos);
        }
        *note = n;
        *len = pgm_read_byte(*pos + 1) * step_ms;
        *left = *len;
        *pos += 2;
    }
    // Leave a short gap before the next note so repeated notes stay distinct
    if (*left <= GAP_MS && *len > 2 * GAP_MS) buzzer_off();
    else                                      buzzer_note(*note);
    *left = *left > TICK_MS ? *left - TICK_MS : 0;
    return 1;
}

// =====================================================================
// Sound task, period 10 ms
//   SND_INIT --> SND_SILENT
//   any state: muted or paused         --> SND_SILENT
//              an effect is queued     --> SND_SFX   (music position kept)
//              a song is set           --> SND_MUSIC
//              otherwise               --> SND_SILENT
// =====================================================================
int TickFct_Sound(int state) {
    switch (state) {
        case SND_INIT:
            state = SND_SILENT;
            break;
        default:
            if (muted || paused) { sfx = 0; state = SND_SILENT; }
            else if (sfx)        state = SND_SFX;
            else if (music)      state = SND_MUSIC;
            else                 state = SND_SILENT;
            break;
    }

    switch (state) {
        case SND_SILENT:
            if (playing_note) buzzer_off();
            break;
        case SND_SFX:
            if (!advance(&sfx, &s_left, &s_note, &s_len, SFX_STEP_MS, 0)) {
                sfx = 0;
                buzzer_off();
            }
            break;
        case SND_MUSIC:
            advance(&mpos, &m_left, &m_note, &m_len, tempo, music);
            break;
        default:
            break;
    }
    return state;
}
