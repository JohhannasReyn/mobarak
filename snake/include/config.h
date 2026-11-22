#pragma once
// =====================================================================
// Snake: pin map and build options (Elegoo UNO R3, ATmega328P)
//
//   4.0" 480x320 SPI TFT, hardware SPI. The panel's inputs are 3.3 V:
//   each signal goes through a divider, 1 kOhm from the UNO pin and
//   2 kOhm from the panel pin to GND (5 V * 2/3 = 3.3 V).
//     VCC -> 5V     GND -> GND     LED -> 3.3V
//     SCK -> D13    SDI (MOSI) -> D11    CS -> D10    DC/RS -> D9    RESET -> D8
//     SDO (MISO, data Out of the panel) stays unconnected.
//   Touch (XPT2046, same SPI bus):
//     T_CLK -> joined to the panel's SCK, after the divider
//     T_DIN -> joined to the panel's SDI, after the divider
//     T_CS  -> D4 through its own divider
//     T_DO  -> D12 straight (3.3 V into the UNO reads HIGH)
//     T_IRQ -> unconnected (the code checks the touch pressure instead)
//     SDI/SDO end in the letters I and O (In/Out), not 1 and 0.
//   Joystick: VRx -> A0, VRy -> A1, SW -> A3, +5V -> 5V, GND -> GND
//   RESET button: A2 to GND (no resistor: the code turns on the pull-up)
//   Buzzer: + (or S on a 3-pin module) -> D3, - -> GND
//
//   D0/D1 stay free for USB serial. D2, D5-D7, A4 and A5 are unused.
// =====================================================================

// ---- TFT driver chip ----
// These boards ship with one of three controllers. If the screen stays
// white (backlight on, nothing drawn), try the next one.
#define TFT_ST7796S 1        // 16 bits per pixel over SPI
#define TFT_ILI9486 2        // 18 bits per pixel over SPI
#define TFT_ILI9488 3        // 18 bits per pixel over SPI
#ifndef TFT_DRIVER
#define TFT_DRIVER TFT_ST7796S
#endif

// Panel variants
#define TFT_FLIP   0         // 1: rotate the picture 180 degrees (header on the other side)
#define TFT_BGR    1         // red and blue swapped? flip this
#define TFT_INVERT 0         // colors look like a photo negative? flip this
#define TFT_SPI_SLOW 0       // 1: 4 MHz SPI instead of 8, if long jumpers garble the picture

// ---- TFT control pins (PORTB) ----
#define TFT_RST PB0          // D8
#define TFT_DC  PB1          // D9, labeled DC/RS
#define TFT_CS  PB2          // D10
// MOSI = PB3 (D11), SCK = PB5 (D13): fixed by the SPI hardware

// ---- Touch panel ----
#define TOUCH_ENABLED 1      // 0: build without touch (set this if T_* isn't wired)
#define TOUCH_CS PD4         // D4

// Calibration: hold the stick pressed while powering up to get the touch
// test screen. Touch its two crosses and copy the RAW numbers here.
// If RAW X follows your finger up and down instead of sideways, flip TOUCH_SWAP_XY.
#define TOUCH_SWAP_XY  1
#define TOUCH_X_LEFT   300   // RAW X at the top-left cross     (screen x = 20)
#define TOUCH_X_RIGHT  3800  // RAW X at the bottom-right cross (screen x = 459)
#define TOUCH_Y_TOP    300   // RAW Y at the top-left cross     (screen y = 20)
#define TOUCH_Y_BOTTOM 3800  // RAW Y at the bottom-right cross (screen y = 299)
#define TOUCH_PRESSURE 300   // a touch counts once PRESSURE reads above this

// ---- Buttons (PORTC, active LOW with internal pull-ups) ----
#define BTN_RESET PC2        // A2: soft reset, restarts the game at any time
#define BTN_SW    PC3        // A3: joystick press

// ---- Joystick (analog) ----
#define ADC_JOY_X 0          // A0
#define ADC_JOY_Y 1          // A1

// Joystick orientation. Flip these if a direction comes out backwards
// for the way the joystick is mounted.
#define JOY_INVERT_X 0       // 0: higher X reading = RIGHT
#define JOY_INVERT_Y 0       // 0: lower Y reading = UP
#define JOY_SWAP_XY  0

// ---- Buzzer: PD3 (D3) is Timer2's OC2B output ----
