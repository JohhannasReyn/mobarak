#include "timer.h"
#include <avr/io.h>
#include <avr/interrupt.h>

static volatile uint32_t ms_count = 0;
static volatile uint8_t  ms_in_tick = 0;
static volatile uint8_t  ticks_pending = 0;

void timer_init() {
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);  // CTC, clk/64: 4 us per count
    OCR1A  = 249;                                       // 250 counts = 1 ms
    TCNT1  = 0;
    TIMSK1 = (1 << OCIE1A);
}

ISR(TIMER1_COMPA_vect) {
    ms_count++;
    if (++ms_in_tick >= GCD_PERIOD) {
        ms_in_tick = 0;
        if (ticks_pending < MAX_BACKLOG) ticks_pending++;
    }
}

uint8_t timer_take_tick() {
    uint8_t sreg = SREG;
    cli();
    uint8_t has = ticks_pending;
    if (has) ticks_pending--;
    SREG = sreg;
    return has != 0;
}

uint32_t timer_ms() {
    uint8_t sreg = SREG;
    cli();
    uint32_t m = ms_count;
    SREG = sreg;
    return m;
}
