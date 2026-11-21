/*
 * Snake on a 4.0" 480x320 SPI TFT for UNO R3 (ATmega328P)
 *
 * Tasks (GCD period 10 ms)
 *   1  Input    20 ms  joystick direction, debounced RESET and stick-press buttons
 *   2  System   20 ms  title screen, soft reset, pause
 *   3  Snake    10 ms  the game: movement, apples, golden mode, scoring; draws on the TFT
 *   4  Sound    10 ms  background music and sound effects on the buzzer
 *
 * Shared variables
 *   in_dir, input events        Input  -> System, Snake
 *   snake_active, snake_restart System -> Snake
 *   snake_over                  Snake  -> System (a stick press there means "play again")
 *   sys_paused                  System -> Snake, Sound (via sound_set_pause)
 *   song, effect, tempo         Snake  -> Sound (sound_music / sound_sfx / sound_tempo)
 *
 * The screen has no buffer (480x320 pixels would need 300 KB): tasks draw
 * straight to it in turns. System draws the title screen and the pause
 * banner, Snake draws the game; snake_active and sys_paused decide whose
 * turn it is.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include "timer.h"
#include "tft.h"
#include "adc.h"
#include "input.h"
#include "sound.h"
#include "scores.h"
#include "game.h"

typedef struct _task {
    signed char state;          // task's current state
    unsigned long period;       // task period (ms)
    unsigned long elapsedTime;  // time since the task last ticked
    int (*TickFct)(int);        // task tick function
} task;

#define NUM_TASKS 4
task tasks[NUM_TASKS];

#define INPUT_PERIOD  20
#define SYSTEM_PERIOD 20
#define SNAKE_PERIOD  10
#define SOUND_PERIOD  10

// One scheduler tick: run every task whose period has elapsed.
// Called from main(), not from the timer interrupt: a full-screen redraw
// takes ~0.5 s and a game-over EEPROM write ~20 ms. Inside the interrupt,
// the 1 ms clock itself would stop counting while they ran.
void scheduler_tick() {
    for (unsigned char i = 0; i < NUM_TASKS; i++) {
        if (tasks[i].elapsedTime >= tasks[i].period) {
            tasks[i].state = tasks[i].TickFct(tasks[i].state);
            tasks[i].elapsedTime = 0;
        }
        tasks[i].elapsedTime += GCD_PERIOD;
    }
}

static void add_task(unsigned char i, signed char state, unsigned long period, int (*fct)(int)) {
    tasks[i].state = state;
    tasks[i].period = period;
    tasks[i].elapsedTime = period;      // tick on the first pass
    tasks[i].TickFct = fct;
}

void system_setup() {
    tft_init();                  // also sets up SPI
    adc_init();
    input_init();
    sound_init();
    scores_init();
    timer_init();

    add_task(0, IN_INIT,   INPUT_PERIOD,  &TickFct_Input);
    add_task(1, SYS_INIT,  SYSTEM_PERIOD, &TickFct_System);
    add_task(2, SN_OFF,    SNAKE_PERIOD,  &TickFct_Snake);
    add_task(3, SND_INIT,  SOUND_PERIOD,  &TickFct_Sound);
}

int main(void) {
    system_setup();
    sei();
    while (1) {
        if (timer_take_tick()) scheduler_tick();
    }
    return 0;
}
