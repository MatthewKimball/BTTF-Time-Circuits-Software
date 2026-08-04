# BTTF Time Circuits Software

Firmware (and a companion PC control panel) for a physical, working replica
of the "Time Circuits" display from *Back to the Future* - the three-row
Destination Time / Present Time / Last Time Departed display panel from the
DeLorean.

Runs on an STM32F405 (FreeRTOS), drives three physical date/time displays,
a keypad, sound effects, an IMU-based "glitch" effect, and exposes full
remote control over CAN (CANopen) so the prop can also be driven from a
web-based control panel over a USB-CAN adapter.

## Hardware

| Component | Part | Interface | Notes |
|---|---|---|---|
| MCU | STM32F405RGT6 | - | Cortex-M4, runs FreeRTOS |
| Destination/Present/Last-Departed displays | 3x HT16K33-driven displays | I2C3 | 14-segment alphanumeric (month) + 7-segment digits (day/year/hour/minute), colon LEDs, AM/PM LED. I2C addresses `0x71`/`0x72`/`0x74` |
| External RTC | DS3231 | I2C2 | Keeps the "present time" ticking across power cycles; battery-backed |
| IMU | BNO055 | I2C1 (dedicated bus) | Any-motion detection, used to implement a software "double tap" gesture (see below) |
| Storage | microSD (FATFS) | SPI | Persists the three date/time records (`svDates.txt`) across power cycles |
| Audio | I2S DAC + amplifier | I2S2 | Keypad tones, glitch/lock/enter sound effects, played from WAV files on the SD card |
| Keypad | 3x4 matrix | GPIO | Types a destination date/time |
| CAN | onboard transceiver | CAN1 | CANopen node, 1 Mbit/s, node ID 21 by default |

Four physical switches, all active-low with software debounce:

| Switch | Purpose |
|---|---|
| Glitch | Enables the destination-display "glitch" effect (also remotely controllable - see [CANopen](#canopen-remote-control) below; either source turns it on) |
| Keypad Enter | Confirms whatever's currently in the keypad buffer as the new destination date |
| Mute | Mutes the colon tick sound only (general "mute all" is remote/software-only - see Settings below) |
| Time Travel | Triggers the time travel sequence (see below) |

## How the hardware controls work

### Startup

On power-up, all three displays stay blank while the board initializes.
Once ready, the startup sound plays and the displays light up with
whatever date/time was last saved to the SD card (or compiled-in defaults,
if none was saved yet or the card can't be read). The destination display's
keypad buffer is pre-loaded with that same restored date, so pressing
Keypad Enter without typing anything re-confirms it rather than blanking
the display.

### Keypad entry

Typing on the 3x4 keypad fills an internal buffer (month/day/year/hour/
minute, 12 digits) with an audible tone per keypress. Pressing **Keypad
Enter** validates the buffer (day-of-month/leap-year aware) and, if valid,
redraws the destination display with the new date, plays a confirmation
sound, and saves it to the SD card. An invalid entry is silently rejected -
the display keeps showing whatever was last valid.

### Present time

The present-time display always tracks the external RTC, refreshing once a
minute. It also drives the "last departed" and "destination" records during
a time travel event (see below).

### Time travel sequence

Triggered by the physical **Time Travel** switch (or remotely, by
requesting the `Travel` state - see [CANopen](#canopen-remote-control)):
displays clear, a sound plays, and after a short delay the present time
becomes the new "last departed" time, the old destination time becomes the
new present time, the RTC is updated to match, and the new state is saved
to the SD card.

### The glitch effect and IMU double-tap

When enabled (by the physical switch or remotely), the destination display
periodically glitches - clearing, flashing scrambled characters, then
restoring the real date, with a sound cue on each cycle. This is purely
cosmetic and never changes the actual stored destination date.

The BNO055 IMU watches for physical taps on the enclosure (it has no native
tap detection, so this is implemented as an "any motion" interrupt with a
1-second double-hit window in software). A **double tap** forces an
immediate glitch cycle - clear, briefly show scrambled characters, then
restore - as a physical "smack the console to fix the glitch" gesture,
matching the movie prop's behavior.

### Sound effects

WAV files are read from the SD card and played over I2S. Keypad digits get
individual DTMF-style tones; other cues (`enter2.wav`, `locked.wav`,
`glitch2.wav`, `beep6.wav`, `enter_v1.wav` for startup) confirm specific
actions - entering a keypad digit, confirming/locking in a date (including
remote CAN-triggered updates), a glitch cycle, and the once-a-second colon
tick, respectively. A dedicated FreeRTOS task drains a sound queue and
handles WAV playback via I2S DMA, independent of the main control loop.

The physical **Mute** switch only silences the colon tick; general
"mute all" is controlled exclusively via CAN/software (see Settings below) -
this split is deliberate, not a bug.

## Firmware architecture

FreeRTOS tasks:

| Task | Role |
|---|---|
| `MainTask` | The main ~20ms control loop: reads switches/keypad, updates displays, runs the glitch state machine, processes CAN-driven settings/function-control bits, services the IMU |
| `SoundTask` | Drains the sound-effect queue and plays WAV files over I2S DMA |
| `ColonTask` | Blinks the colon LEDs once a second |
| `CANopenTask` | Runs the CANopenNode stack (SDO/PDO/heartbeat processing) |
| `RtcInitTask` | One-shot: initializes the external RTC after the scheduler starts, in its own task so a stuck/faulty RTC bus can never hang the rest of the board |

Key source layout:

- `App/Time_Circuits/` - all of the above: display driving (`datetime_display.c`), the main control loop and switch/keypad/glitch/time-travel logic (`timecircuit_control.c`), sound (`sound_effects.c`), the IMU (`imu.c`), SD card storage (`storagedevice_control.c`)
- `Core/` - STM32CubeMX-generated peripheral init (clocks, GPIO, I2C, SPI, I2S, CAN, TIM, RTC) plus the FreeRTOS task definitions (`Core/Src/freertos.c`)
- `CANopen/` - the object dictionary (`OD.h`/`OD.c`) for this node
- `Libraries/` - vendored drivers: HT16K33 display driver, BNO055 driver, DS3231 driver, a 3x4 keypad driver, and the CANopenNode STM32 port
- `Middlewares/`, `FATFS/`, `Drivers/` - ST HAL/CMSIS and FATFS middleware

## CANopen (remote control)

The board is a CANopen node (default node ID **21**, **1 Mbit/s**) exposing
the following object dictionary entries:

| Index | Object | Access | Purpose |
|---|---|---|---|
| `0x2000` | Destination Time | rw | day/month/year/hour/minute/AM-PM |
| `0x2001` | Present Time | rw | same layout; kept in sync with the RTC |
| `0x2002` | Last Time Departed | rw | same layout |
| `0x2100` | Buttons/Switches State | ro | live bitmask mirroring the four physical switches |
| `0x2101` | Time Circuits State (current) | ro | `0`=Idle, `1`=Armed, `2`=Travel, `3`=Complete |
| `0x2102` | Time Circuits State (requested) | rw | write a target state; firmware validates the transition (e.g. Armed requires a valid destination date) and silently ignores invalid requests |
| `0x2200` | Function Control | rw | one-shot action bits: clear/update/set displays, update destination date, update RTC from present time, save dates to SD - firmware clears each bit automatically once actioned |
| `0x2300` | Settings | rw | glitch period/enable, mute colon/mute all, IMU any-motion threshold/duration (applied via their own one-shot bits) |

All date/time and control writes over CAN take effect the same way a local
keypad entry or switch would - including saving to the SD card and updating
the physical displays.

## PC control panel (`pc-gui/`)

A local web app (FastAPI backend + plain HTML/JS frontend, no build step)
that talks to the board as a CANopen SDO client over a USBtin (or any
slcan-compatible) USB-CAN adapter. It exposes every object dictionary entry
above through a browser UI: date/time editing, function control, settings,
live switch/state monitoring, a "Movie-Accurate Defaults" button, a
history-themed "Randomiser" for the destination time, and a real-time clock
sync feature. See `pc-gui/README.md` for setup and usage.

## Building and flashing

Build system: CMake + Ninja, using the STM32CubeCLT toolchain (migrated
from STM32CubeIDE; the original `.project`/`.cproject`/`.ioc` files are
still present for reference).

```
cmake --preset Debug      # first time only
cmake --build build/Debug
```

Flash over SWD (ST-Link):

```
STM32_Programmer_CLI -c port=SWD -w build/Debug/Time_Circuit_Control.elf -v -rst
```
