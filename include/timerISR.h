#ifndef TIMERISR_H
#define TIMERISR_H

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

void TimerISR();   // Provided by the application: the task scheduler.

unsigned long _avr_timer_M       = 1;   // TimerISR() period in 1 ms hardware ticks
unsigned long _avr_timer_cntcurr = 0;   // ticks left until the next TimerISR()

void TimerSet(unsigned long M) {
    _avr_timer_M       = M;
    _avr_timer_cntcurr = M;
}

void TimerOn() {
    TCCR2A = (1 << WGM21);     // CTC mode, TOP = OCR2A
    TCCR2B = (1 << CS22);      // clk/64 (on Timer2 that's CS22 alone) -> 250 kHz
    OCR2A  = 249;              // 250 counts at 250 kHz = 1 ms
    TCNT2  = 0;
    TIMSK2 = (1 << OCIE2A);    // compare-match A interrupt
    _avr_timer_cntcurr = _avr_timer_M;
    sei();
}

void TimerOff() {
    TCCR2B = 0;
    TIMSK2 = 0;
}

ISR(TIMER2_COMPA_vect) {
    if (--_avr_timer_cntcurr == 0) {
        TimerISR();
        _avr_timer_cntcurr = _avr_timer_M;
    }
}

////////// SONAR (HC-SR04) //////////
// Echo MUST be on PB0 (D8): that's the ATmega328P's ICP1 input-capture pin.
// Trigger can be any output; change these three lines if you wire it elsewhere.
#define SONAR_TRIG_PORT  PORTB
#define SONAR_TRIG_DDR   DDRB
#define SONAR_TRIG_BIT   PB1          // Arduino D9

// Timer1 at clk/64 = 250 kHz -> 4 us per count, 262 ms before a 16-bit wrap.
// That's far longer than any echo, so no overflow counting is needed -- which matters,
// because tasks tick inside an ISR where an overflow interrupt could never run anyway.
#define SONAR_US_PER_COUNT  4
#define SONAR_RISE_TIMEOUT  (5000U  / SONAR_US_PER_COUNT)   // echo normally rises ~0.5 ms after trigger
#define SONAR_PULSE_TIMEOUT (30000U / SONAR_US_PER_COUNT)    // 30 ms of echo ~= 5 m: nothing in range

// Returns distance in cm.
//   -1.0  -> no echo at all (sensor glitch): caller should keep its previous reading.
//   ~517  -> echo still high at the 30 ms timeout: nothing within range.
// Blocks for roughly (0.5 ms + 58 us/cm), i.e. ~4 ms at 60 cm, 35 ms worst case.
double read_sonar() {
    SONAR_TRIG_DDR |= (1 << SONAR_TRIG_BIT);
    DDRB &= ~(1 << PB0);                     // echo / ICP1 is an input

    TIMSK1 = 0;                              // polled: no Timer1 interrupts
    TCCR1A = 0;                              // normal mode, OC1A/OC1B pins disconnected
    TCCR1B = 0;                              // stopped while we set up
    TCNT1  = 0;
    TIFR1  = (1 << ICF1) | (1 << TOV1);      // flags are write-1-to-clear

    SONAR_TRIG_PORT |= (1 << SONAR_TRIG_BIT);    // >= 10 us trigger pulse
    _delay_us(10);
    SONAR_TRIG_PORT &= ~(1 << SONAR_TRIG_BIT);

    // Start Timer1: noise canceler on, capture on RISING edge, clk/64.
    TCCR1B = (1 << ICNC1) | (1 << ICES1) | (1 << CS11) | (1 << CS10);

    while (!(TIFR1 & (1 << ICF1))) {
        if (TCNT1 > SONAR_RISE_TIMEOUT) { TCCR1B = 0; return -1.0; }
    }
    uint16_t start = ICR1;

    TCCR1B &= ~(1 << ICES1);                 // now capture the FALLING edge...
    TIFR1 = (1 << ICF1);                     // ...and clear ICF1 after an edge change (datasheet,
                                             //    "Using the Input Capture Unit")

    while (!(TIFR1 & (1 << ICF1))) {
        if ((uint16_t)(TCNT1 - start) > SONAR_PULSE_TIMEOUT) {
            TCCR1B = 0;
            return (double)SONAR_PULSE_TIMEOUT * SONAR_US_PER_COUNT / 58.0;
        }
    }
    uint16_t width = ICR1 - start;           // unsigned math survives a wrap

    TCCR1B = 0;                              // stop Timer1 and leave it clean
    TIFR1  = (1 << ICF1) | (1 << TOV1);

    return (double)width * SONAR_US_PER_COUNT / 58.0;   // 58 us of round trip per cm
}

#endif // TIMERISR_H
