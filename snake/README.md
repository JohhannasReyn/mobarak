# Snake on a 4.0" 480x320 TFT

A complete Snake game on a 4.0" 480x320
SPI TFT, written as a Concurrent SynchSM in bare-metal AVR C++ with no
Arduino API and no display library.

## Wiring

**The panel's inputs are 3.3 V; the UNO's outputs are 5 V.** Each of the five
signals into the panel goes through a divider:

```
UNO pin ──[ 1 kΩ ]──┬── TFT pin
                    └─[ 2 kΩ ]── GND        5 V × 2/3 = 3.3 V
```

That's five 1 kΩ (brown black black brown brown) and five 2 kΩ (red black
black brown brown) resistors. No 2 kΩ left? Two 1 kΩ in series work.

| Part | Pin | UNO |
|---|---|---|
| TFT | VCC | 5V (the board has its own 3.3 V regulator) |
| | GND | GND |
| | CS | D10 through the divider |
| | RESET | D8 through the divider |
| | DC/RS | D9 through the divider |
| | SDI (MOSI) | D11 through the divider |
| | SCK | D13 through the divider |
| | LED | 3.3V (backlight always on) |
| | SDO (MISO), T_CLK, T_CS, T_DIN, T_DO, T_IRQ | not connected |
| Joystick | VRx / VRy / SW | A0 / A1 / A3 |
| | +5V / GND | 5V / GND |
| RESET button | one leg / diagonal leg | A2 / GND |
| Buzzer | + (S on a 3-pin module) / - | D3 / GND (middle pin of a 3-pin module to 5V) |
| IR receiver | OUT / VCC / GND | D2 / 5V / GND |

Everything except the screen is wired exactly as in the LCD build, so moving
over means pulling the LCD's wires (D4-D9) and adding the TFT. D4-D7, D12,
A4 and A5 are unused. No resistors on the buttons: the code turns on the
internal pull-ups.

## Controls

| Input | Title screen | Playing | Paused | Game over |
|---|---|---|---|---|
| Joystick | | steer | | |
| Stick press | start | | resume | play again |
| RESET button | start | restart now | restart | restart |
| Remote >\|\| | | pause | resume | |
| Remote POWER | | quit to title | quit to title | quit to title |
| Remote VOL- / VOL+ | mute / unmute | same | same | same |

## Screen layout

```
y   0-31   status bar: SCORE 00000 | golden-mode timer bar | HI 00000
y  32-319  the field: 30 x 18 cells of 16 x 16 px, checkerboard
```

The pause banner replaces the status bar, so the field stays visible.
Game over draws a panel over the middle of the field.

## Rules

- Each apple: 10 points (20 in golden mode). The snake grows and speeds up:
  160 ms per move at the start, one tick (10 ms) faster every 4 apples, 70 ms
  at the fastest. The music speeds up with it.
- Every 5th apple brings a **golden apple** that stays 8 s, blinking in its
  last 2 s.
- Golden apple: 50 points and **golden mode** for 10 s: the snake turns gold,
  the walls wrap around and apples are worth double. A bar in the status bar
  runs down; in the last 2 s it and the snake's head blink.
- Hitting a wall (outside golden mode) or yourself ends the game.
- Reaching length 100 wins.
- The top 3 scores live in EEPROM and survive power-off.

## Build and flash

`platformio.ini` has no `framework = arduino` line, so an accidental
`#include <Arduino.h>` fails the build. Size: about 11 KB of 32 KB flash and
700 bytes of 2 KB RAM (the 30 x 18 grid is 540 of them).

## Task structure (Concurrent SynchSM, GCD 10 ms)

| # | Task | Period | States | Job |
|---|---|---|---|---|
| 1 | Input | 20 ms | IN_INIT, IN_SAMPLE | read the joystick, debounce RESET and stick press, raise press events |
| 2 | IR | 50 ms | IR_INIT, IR_WAIT | turn frames from the INT0 decoder into pause / title / mute events |
| 3 | System | 20 ms | SYS_INIT, SYS_TITLE, SYS_PLAY, SYS_PAUSE | title screen, soft reset, pause |
| 4 | Snake | 10 ms | SN_OFF, SN_PLAY, SN_OVER | the game: steering, moves, apples, golden mode, scores; draws the field |
| 5 | Sound | 10 ms | SND_INIT, SND_SILENT, SND_MUSIC, SND_SFX | music and effects on the buzzer |

The LCD build had a sixth task that copied a screen buffer to the display.
A 480x320 buffer would need 300 KB, so here the tasks draw straight to the
panel, taking turns: System owns the screen on the title and pause banner,
Snake owns it during play (`snake_active`, `sys_paused`).

## Hardware resources

| Resource | Used for |
|---|---|
| SPI (D11, D13), D8-D10 | TFT data, clock, reset, data/command, chip select |
| Timer1 (CTC, 1 ms) | scheduler tick, `timer_ms()`, `timer_us()` for IR timing |
| Timer2 (CTC, OC2B toggle on D3) | buzzer tones, no CPU time per cycle |
| INT0 (D2, falling edge) | NEC IR decoding |
| ADC (A0, A1) | joystick |
| EEPROM | top-3 high scores |

## Design decisions

1. **Draw only what changed.** A move touches three cells: the new head, the
   old head (now body) and the old tail. That's about 650 pixels, 2-3 ms of
   SPI. Clearing the whole field takes 0.4 s (0.6 s on the 18-bit chips).
2. **Drop missed ticks after a long draw.** While a full-screen draw blocks the
   loop for half a second, 50 scheduler ticks go by. Replaying them all would
   make the snake jump several cells as soon as the field appears, so the
   timer keeps at most 2 (`MAX_BACKLOG` in `timer.h`).
3. **One driver, three chips.** The chips share the MIPI command set (CASET,
   RASET, RAMWR, MADCTL, COLMOD); only the start-up sequence and the pixel
   format differ. The code keeps colors as RGB565 and converts at the last
   moment, in `push()` in `tft.cpp`.
4. **Status-bar digits redraw individually.** A big digit is 432 pixels; +10
   points usually changes one or two of the five digits.
5. **Big title text is drawn as rectangles**, one per vertical run of lit
   pixels, and only the lit ones. That lets it sit on top of its own shadow.
6. **The scheduler runs in `main()`, not inside the timer interrupt.** The
   Timer1 interrupt only counts ticks, so a long draw or an EEPROM write never
   blocks INT0 and the IR decoder never misses an edge.
7. **The grid stores directions**, so the tail always knows where to go next,
   and moving into your own tail is legal when the snake isn't growing.
8. **Strings live in flash (`PSTR`)**, and so do the init sequences.
