#pragma once
#include <stdint.h>

// Scheduler time base: Timer1 interrupts every 1 ms and queues one
// scheduler tick every GCD_PERIOD ms. The tasks run in main(), not in the
// interrupt, so a long screen draw never stops the clock.
#define GCD_PERIOD 10

// A full-screen redraw blocks the loop for ~0.5 s. Ticks missed meanwhile
// are dropped, not replayed: replaying 50 of them back to back would make
// the snake lurch several cells forward right after the field appears.
#define MAX_BACKLOG 2

void     timer_init();
uint8_t  timer_take_tick();   // 1 if a scheduler tick is waiting (and consumes it)
uint32_t timer_ms();          // ms since boot
