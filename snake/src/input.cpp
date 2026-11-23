#include "input.h"
#include "config.h"
#include "adc.h"
#include "rng.h"
#include "timer.h"
#include "touch.h"
#include <avr/io.h>

uint16_t in_joy_x = 512, in_joy_y = 512;
uint8_t  in_dir = DIR_NONE;
uint8_t  in_touch = 0;
int16_t  in_touch_x = 0, in_touch_y = 0;
int16_t  in_tap_x = 0, in_tap_y = 0;
static uint8_t events = 0;

#define BTN_MASK ((1 << BTN_RESET) | (1 << BTN_SW))

// Joystick deadzone with hysteresis: leave center past ENTER, return inside EXIT
#define JOY_ENTER 300
#define JOY_EXIT  200

void input_init() {
    DDRC  &= ~BTN_MASK;
    PORTC |=  BTN_MASK;          // pull-ups: idle HIGH, pressed LOW
}

uint8_t input_take(uint8_t mask) {
    uint8_t got = events & mask;
    events &= ~mask;
    return got;
}

uint8_t input_take_tap(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    if (!(events & EV_TAP)) return 0;
    if (in_tap_x < x0 || in_tap_x >= x1 || in_tap_y < y0 || in_tap_y >= y1) return 0;
    events &= ~EV_TAP;
    return 1;
}

void input_clear() {
    events = 0;
}

uint8_t input_sw_down() {
    return !(PINC & (1 << BTN_SW));
}

// Dominant axis wins. Hysteresis keeps a held direction from flickering:
// the stick has to come back inside JOY_EXIT to release, and the other axis
// has to win by a clear margin to switch.
static uint8_t joystick_direction(int16_t dx, int16_t dy, uint8_t current) {
    const int16_t ax = dx < 0 ? -dx : dx;
    const int16_t ay = dy < 0 ? -dy : dy;
    const int16_t m  = ax > ay ? ax : ay;
    const uint8_t horizontal = dx < 0 ? DIR_WEST : DIR_EAST;
    const uint8_t vertical   = dy < 0 ? DIR_NORTH : DIR_SOUTH;

    if (m < JOY_EXIT) return DIR_NONE;
    if (m < JOY_ENTER && current == DIR_NONE) return DIR_NONE;
    if ((current == DIR_WEST || current == DIR_EAST) && ax + 100 >= ay) return horizontal;
    if ((current == DIR_NORTH || current == DIR_SOUTH) && ay + 100 >= ax) return vertical;
    return ax > ay ? horizontal : vertical;
}

// =====================================================================
// Input task, period 20 ms
//   IN_INIT --> IN_SAMPLE --> IN_SAMPLE ...
//   Reads the joystick and the touch screen, debounces the two buttons and
//   the touch (each must read the same on two samples in a row), and raises
//   press and tap events.
// =====================================================================
int TickFct_Input(int state) {
    static uint8_t stable = 0;     // debounced buttons, 1 = pressed
    static uint8_t last_raw = 0;
#if TOUCH_ENABLED
    static uint8_t touch_last = 0; // touch on the previous sample
#endif

    switch (state) {
        case IN_INIT:                  // a button already held at power-up isn't a press
            last_raw = stable = ~PINC & BTN_MASK;
            state = IN_SAMPLE;
            break;
        case IN_SAMPLE: break;
        default:        state = IN_INIT; break;
    }

    if (state == IN_SAMPLE) {
        uint16_t x = adc_read(ADC_JOY_X);
        uint16_t y = adc_read(ADC_JOY_Y);
        if (JOY_SWAP_XY)  { uint16_t t = x; x = y; y = t; }
        if (JOY_INVERT_X) x = 1023 - x;
        if (JOY_INVERT_Y) y = 1023 - y;
        in_joy_x = x;
        in_joy_y = y;
        rng_stir((x << 6) ^ (y << 3) ^ (uint16_t)timer_ms());

        const uint8_t prev_dir = in_dir;
        in_dir = joystick_direction((int16_t)x - 512, (int16_t)y - 512, in_dir);
        if (in_dir != prev_dir) {
            switch (in_dir) {
                case DIR_NORTH:    events |= EV_UP;    break;
                case DIR_SOUTH:  events |= EV_DOWN;  break;
                case DIR_WEST:  events |= EV_LEFT;  break;
                case DIR_EAST: events |= EV_RIGHT; break;
                default: break;
            }
        }

        const uint8_t raw = ~PINC & BTN_MASK;
        const uint8_t settled = ~(raw ^ last_raw) & BTN_MASK;      // same as last sample
        const uint8_t now = (stable & ~settled) | (raw & settled);
        const uint8_t pressed = now & ~stable;
        last_raw = raw;
        stable = now;
        if (pressed & (1 << BTN_RESET)) events |= EV_RESET;
        if (pressed & (1 << BTN_SW))    events |= EV_SW;

#if TOUCH_ENABLED
        int16_t tx, ty;
        const uint8_t touched = touch_read(&tx, &ty);
        if (touched) { in_touch_x = tx; in_touch_y = ty; }
        if (touched && touch_last && !in_touch) {         // pressed on two samples: a tap
            in_touch = 1;
            in_tap_x = tx;
            in_tap_y = ty;
            events |= EV_TAP;
        } else if (!touched && !touch_last) {             // released on two samples
            in_touch = 0;
        }
        touch_last = touched;
#endif
    }
    return state;
}
