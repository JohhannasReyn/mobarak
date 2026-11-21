// Lab 7, Exercise 1: joystick -> servo position (X) and stepper direction (Y), status on the LCD
//
// Wiring:
//   Joystick  VRx -> A1, VRy -> A2
//   Servo     signal -> D9 (PB1 / OC1A)        -- power it from a different rail than the LCD
//   Stepper   IN1..IN4 -> D10..D13 (PB2..PB5)
//   LCD       as LCD-1-1.h expects: RS -> D2, EN -> D3, D4..D7 -> D4..D7 (RW -> GND)

#include <avr/interrupt.h>
#include <avr/io.h>

#include "timerISR.h"
#include "LCD-1-1.h"
#include "periph_lab7_sp2025-1.h"

#define NUM_TASKS 3

// ---------- Servo (Timer1, fast PWM, TOP = ICR1) ----------
#define SERVO_TOP 39999   // 16 MHz / (8 * 50 Hz) - 1 -> 20 ms period
#define SERVO_MIN   999   // -90 degrees
#define SERVO_MID  2999   //   0 degrees
#define SERVO_MAX  4999   // +90 degrees

// ---------- Joystick ----------
#define JOY_X_CHANNEL 1
#define JOY_Y_CHANNEL 2
#define JOY_CENTER    512
#define JOY_DEADBAND   60   // ADC counts either side of centre that count as "released"
#define JOY_Y_LOW     400
#define JOY_Y_HIGH    600

// ---------- Stepper (PB2..PB5) ----------
#define STEPPER_MASK  0x3C
#define STEPPER_SHIFT 2
const unsigned char phases[8] = {0b0001, 0b0011, 0b0010, 0b0110, 0b0100, 0b1100, 0b1000, 0b1001};

enum Stepper_Dir { DIR_STOP, DIR_FORWARD, DIR_REVERSE };

// ---------- LCD messages (lcd_write_str() in LCD-1-1.h takes a non-const char*) ----------
#define LCD_CMD_ENTRY_MODE 0x06   // cursor moves right after each character, no display shift
char MSG_SERVO[]     = "Servo Active";
char BLANK_SERVO[]   = "            ";
char MSG_STEPPER[]   = "Stepper Active";
char BLANK_STEPPER[] = "              ";

// Shared between tasks (all tasks run from TimerISR, so no locking is needed)
unsigned char stepper_dir  = DIR_STOP;
unsigned char servo_active = 0;   // 1 while the joystick is holding the servo off centre

typedef struct _task {
    signed   char state;
    unsigned long period;
    unsigned long elapsedTime;
    int (*TickFct)(int);
} task;

const unsigned long JOYSTICK_PERIOD = 100;
const unsigned long STEPPER_PERIOD  = 20;
const unsigned long LCD_PERIOD      = 100;
const unsigned long GCD_PERIOD      = 20;

task tasks[NUM_TASKS];

enum Joystick_States { JS_INIT, JS_READ };
enum Stepper_States  { STEP_INIT, STEP_RUN };
enum LCD_States      { LCD_INIT, LCD_UPDATE };

int TickFct_Joystick(int state);
int TickFct_Stepper(int state);
int TickFct_LCD(int state);

void TimerISR() {
    for (unsigned int i = 0; i < NUM_TASKS; i++) {
        if (tasks[i].elapsedTime >= tasks[i].period) {
            tasks[i].state = tasks[i].TickFct(tasks[i].state);
            tasks[i].elapsedTime = 0;
        }
        tasks[i].elapsedTime += GCD_PERIOD;
    }
}

// Map the joystick X reading (0..1023) onto the full servo range.
// Inside the deadband the servo sits at 0 degrees; outside it the angle is
// proportional to the deflection, reaching +/-90 degrees at the end stops.
// If your servo turns the wrong way, swap the '-' and '+' below.
unsigned int ServoFromJoystick(unsigned int adc) {
    int offset = (int)adc - JOY_CENTER;                       // -512 .. +511

    if (offset > JOY_DEADBAND) {
        long travel = (long)(offset - JOY_DEADBAND) * (SERVO_MID - SERVO_MIN)
                      / (JOY_CENTER - 1 - JOY_DEADBAND);
        return (unsigned int)(SERVO_MID - travel);
    }
    if (offset < -JOY_DEADBAND) {
        long travel = (long)(-offset - JOY_DEADBAND) * (SERVO_MAX - SERVO_MID)
                      / (JOY_CENTER - JOY_DEADBAND);
        return (unsigned int)(SERVO_MID + travel);
    }
    return SERVO_MID;
}

// Task 1: joystick -> servo angle and stepper direction
int TickFct_Joystick(int state) {
    switch (state) {
        case JS_INIT: state = JS_READ; break;
        case JS_READ: state = JS_READ; break;
        default:      state = JS_INIT; break;
    }

    switch (state) {
        case JS_READ: {
            unsigned int servo_val = ServoFromJoystick(ADC_read(JOY_X_CHANNEL));
            OCR1A = servo_val;                      // latched at the start of the next PWM period
            servo_active = (servo_val != SERVO_MID);

            unsigned int y = ADC_read(JOY_Y_CHANNEL);
            if (y < JOY_Y_LOW) {
                stepper_dir = DIR_FORWARD;
            } else if (y > JOY_Y_HIGH) {
                stepper_dir = DIR_REVERSE;
            } else {
                stepper_dir = DIR_STOP;
            }
            break;
        }
        default: break;
    }
    return state;
}

// Task 2: one half-step per tick in the commanded direction
int TickFct_Stepper(int state) {
    static unsigned char phase_index = 0;

    switch (state) {
        case STEP_INIT: state = STEP_RUN; break;
        case STEP_RUN:  state = STEP_RUN; break;
        default:        state = STEP_INIT; break;
    }

    switch (state) {
        case STEP_RUN:
            if (stepper_dir == DIR_STOP) {
                PORTB &= ~STEPPER_MASK;             // coils off: no holding current, no heat
            } else {
                phase_index = (stepper_dir == DIR_FORWARD) ? (phase_index + 1) & 7
                                                           : (phase_index + 7) & 7;
                PORTB = (PORTB & ~STEPPER_MASK) | (phases[phase_index] << STEPPER_SHIFT);
            }
            break;
        default: break;
    }
    return state;
}

// Task 3: LCD. LCD-1-1.h takes 8 ms per character and that time is spent inside the
// timer interrupt, so a row is rewritten only when its message changes, and at most
// one row per tick. Each rewrite holds the other tasks up for about 0.1 s.
int TickFct_LCD(int state) {
    static unsigned char shown_servo   = 0;         // both rows start blank (LCD_Start clears)
    static unsigned char shown_stepper = 0;

    switch (state) {
        case LCD_INIT:   state = LCD_UPDATE; break;
        case LCD_UPDATE: state = LCD_UPDATE; break;
        default:         state = LCD_INIT;   break;
    }

    switch (state) {
        case LCD_UPDATE: {
            unsigned char stepper_active = (stepper_dir != DIR_STOP);

            if (servo_active != shown_servo) {
                lcd_goto_xy(0, 0);
                lcd_write_str(servo_active ? MSG_SERVO : BLANK_SERVO);
                shown_servo = servo_active;
            } else if (stepper_active != shown_stepper) {
                lcd_goto_xy(1, 0);
                lcd_write_str(stepper_active ? MSG_STEPPER : BLANK_STEPPER);
                shown_stepper = stepper_active;
            }
            break;
        }
        default: break;
    }
    return state;
}

// lcd_init() in LCD-1-1.h assumes the LCD has only just powered up. After a reset or a
// re-upload the LCD is still in 4-bit mode, so the single nibble lcd_init() starts with
// leaves it half a byte out of step and the screen fills with garbage. Three "8-bit
// interface" strobes first put the LCD back in its power-up state from any starting
// point (HD44780 datasheet, "initializing by instruction"); then lcd_init() works.
void LCD_Start(void) {
    DATA_DDR |= 0xF0;
    CTL_DDR  |= (1 << LCD_EN) | (1 << LCD_RS);
    CTL_BUS  &= ~((1 << LCD_EN) | (1 << LCD_RS));
    _delay_ms(50);                                   // LCD power-up time

    for (unsigned char i = 0; i < 3; i++) {
        DATA_BUS = (DATA_BUS & 0x0F) | 0x30;
        CTL_BUS |=  (1 << LCD_EN);
        _delay_ms(1);
        CTL_BUS &= ~(1 << LCD_EN);
        _delay_ms(5);
    }

    lcd_init();                                      // provided: 4-bit, 2 rows, display on
    lcd_send_command(LCD_CMD_ENTRY_MODE);
    lcd_send_command(LCD_CMD_DISPLAY_NO_CURSOR);     // hide the blinking cursor
    lcd_clear();
}

int main(void) {
    // PB1 = servo PWM (must be an output for OC1A to reach the pin), PB2..PB5 = stepper
    DDRB  |=  (1 << PB1) | STEPPER_MASK;
    PORTB &= ~((1 << PB1) | STEPPER_MASK);

    // PORTC: all inputs, no pull-ups (A1/A2 are the joystick's analog outputs)
    DDRC  = 0x00;
    PORTC = 0x00;

    ADC_init();
    LCD_Start();

    // Timer1: fast PWM, TOP = ICR1 (mode 14), non-inverting on OC1A, prescaler 8
    TCCR1A = (1 << WGM11) | (1 << COM1A1);
    TCCR1B = (1 << WGM12) | (1 << WGM13) | (1 << CS11);
    ICR1   = SERVO_TOP;
    OCR1A  = SERVO_MID;

    tasks[0].period      = JOYSTICK_PERIOD;
    tasks[0].state       = JS_INIT;
    tasks[0].elapsedTime = JOYSTICK_PERIOD;
    tasks[0].TickFct     = &TickFct_Joystick;

    tasks[1].period      = STEPPER_PERIOD;
    tasks[1].state       = STEP_INIT;
    tasks[1].elapsedTime = STEPPER_PERIOD;
    tasks[1].TickFct     = &TickFct_Stepper;

    tasks[2].period      = LCD_PERIOD;
    tasks[2].state       = LCD_INIT;
    tasks[2].elapsedTime = LCD_PERIOD;
    tasks[2].TickFct     = &TickFct_LCD;

    // Scheduler tick on Timer2 (Timer1 belongs to the servo)
    TimerSet(GCD_PERIOD);
    TimerOn();

    while (1) {
    }

    return 0;
}
