# POV LED Globe — TM4C123 Driver

A persistence-of-vision LED globe powered by a TM4C123GH6PM. A 143-LED SK9822 strip is mounted on a spinning blade; columns of image data are clocked out at a rate locked to the motor's hall-sensor feedback so a stable image fuses in mid-air.

Supports static images (ROM and SD card), animated GIFs streamed from SD, and a microphone-driven VU meter.

---

## Hardware

| Component | Part | Interface |
|-----------|------|-----------|
| MCU | TI TM4C123GH6PM (Tiva C) | 80 MHz |
| LEDs | 143× SK9822 (originally 144) | SSI2 + uDMA, 20 MHz |
| Motor | TEC3650 BLDC + 6:1 gearbox | PWM + hall capture |
| LCD | ST7735 128×160 colour TFT | SSI0 |
| SD card | FAT32 SD/SDHC | SSI0 (shared bus) |
| Microphone | Analog electret | ADC0 / AIN0 |

---

## Pin Map

```
PORT A — LCD + SD card (SSI0)
  PA2  SSI0 SCLK    → ST7735 SCK / SD card SCK
  PA3  SSI0 FSS     → ST7735 CS
  PA4  SSI0 RX      → SD card MISO
  PA5  SSI0 TX      → ST7735 MOSI / SD card MOSI
  PA6  GPIO out     → ST7735 D/C
  PA7  GPIO out     → ST7735 RESET

PORT B — LEDs (SSI2) + LCD buttons
  PB1  GPIO in      → LCD button "Up"
  PB2  GPIO in      → LCD button "Down"
  PB3  GPIO in      → LCD button "Select"
  PB4  SSI2 SCLK    → SK9822 CLK
  PB7  SSI2 TX      → SK9822 DI

PORT C — Motor
  PC4  WT0CCP0      → hall sensor (open-drain, falling edge)
  PC5  M0PWM7       → motor PWM (active-low, 20 kHz)
  PC6  GPIO out     → direction (0 = forward)

PORT E — Microphone
  PE3  AIN0         → mic analog input

PORT F — Motor buttons + RGB heartbeat LED
  PF0  GPIO in      → motor spin-up
  PF1  GPIO out     → red LED
  PF2  GPIO out     → blue LED
  PF3  GPIO out     → green LED
  PF4  GPIO in      → motor spin-down

SD card chip-select: dedicated GPIO (see diskio_DMA.c)
```

---

## Repository Layout

```
.
├── main.c                  Stripped-down version, ROM-only (no SD card)
├── main_sd.c               Full application, the one you'll use
├── main.h
│
├── Drivers/
│   ├── inc/                Driver headers
│   └── src/                Driver source
│       ├── SK9822.c              Frame builders (image → SPI bytes)
│       ├── SK9822_DMA_SSI.c      DMA pipeline (SPI bytes → LEDs)
│       ├── Motor.c               PWM + hall RPM + ramp state machine
│       ├── LCD.c                 ST7735 menu UI
│       ├── GPIO.c                Port B/F button ISRs + heartbeat LED
│       ├── Mic.c                 ADC0 single-sample reader
│       ├── DMA_Common.c          uDMA controller init
│       ├── SD_DMA.c              SD card SPI-RX via uDMA
│       ├── diskio_DMA.c          FatFs disk I/O backend
│       ├── ff.c                  FatFs filesystem (3rd party)
│       ├── ST7735.c              LCD low-level driver (3rd party)
│       ├── Timer*A.c             32-bit periodic timer wrappers
│       └── Unified_Port_Init.c   GPIO bring-up boilerplate
│
├── Data/                   Image and animation data
│   ├── map_*.{c,h}              Earth (ROM, 8-bit)
│   ├── dermot/, vote/, eye/     Other ROM 8-bit images
│   ├── cat_space/, frog/, ...   Animation palettes (data on SD as .bin)
│   └── globe_old/               Legacy 144-LED data, kept for reference
│
├── Tests/                  Standalone test programs
│   ├── DMA_test.c               Exercise SK9822 DMA pipeline alone
│   ├── Motor_test.c             Sweep motor 0%-100% in both directions
│   ├── SD_Card.c                SD card read smoke test
│   ├── SD_Card_Sanity_Check.c   FatFs read/write verification
│   └── SD_Card_Write.c          (unused) write image data to SD
│
├── Documentation/          Module-level docs
│   ├── SSI-DMA.md               LED DMA pipeline deep-dive
│   ├── SK9822.md                Frame builder reference
│   ├── Motor.md                 Motor driver internals
│   └── main_sd.md               Application architecture
│
└── Debug/                  Build artefacts (gitignored)
```

---

## Quick Start

1. Open the project in **Code Composer Studio**.
2. Make sure the build is targeting `main_sd.c` (not `main.c` or any of the test programs — only one `main` can be active at a time).
3. Connect the board, hit **Debug**. CCS auto-flashes and starts execution.
4. The LCD shows the menu. Use PB1/PB2/PB3 to navigate.
5. PF0 spins the motor up, PF4 spins it down (also accessible from the LCD menu).

### Switching what gets built

Each `main*.c` defines a different `main()` entry point. To change which one runs, edit the file you don't want to use so its entry point is named something other than `main` (e.g. `main2`, `main88`). Only the file containing the actual `main` will be the program entry.

---

## How It Works

The motor spins a blade carrying a strip of LEDs. A hall sensor tells us the rotation period. We divide that period by the number of angular columns we want, and use that as the firing rate for the hardware Timer. Each Timer tick, the next column of pre-computed image data is expanded to the SPI wire format and shifted out via **DMA + SSI2** — fast enough that the LEDs change colour many times per revolution, painting an image in space. The CPU is mostly idle during all this, free to handle the LCD menu, motor ramp, and SD card streaming.

For a thorough walkthrough see [Documentation/main_sd.md](Documentation/main_sd.md).

---

## Adding New Content

### New ROM image (compiled into flash)

```
python globe_convert.py myimage.png --mode 8bit
mkdir Data/myimage
mv myimage_data.{c,h} myimage_palette.{c,h}  Data/myimage/
```

Then in `main_sd.c`:

```c
#include "Data/myimage/myimage_data.h"
#include "Data/myimage/myimage_palette.h"

static const ContentEntry_t pictures[] = {
    /* ...existing entries... */
    { "MyImage", CONTENT_ROM_8BIT, myimage_data, myimage_palette,
      GLOBE_NUM_COLUMNS, NULL },
};
```

### New animation (streamed from SD)

```
python globe_convert_gif.py myanim.gif --mode 4bit
cp myanim.bin /path/to/sd/  →  rename to 8.3 format, e.g. mya.bin
mkdir Data/myanim
mv myanim_palette.{c,h}  Data/myanim/
```

Then in `main_sd.c`:

```c
#include "Data/myanim/myanim_palette.h"

static const ContentEntry_t animations[] = {
    /* ...existing entries... */
    { "MyAnim", CONTENT_SD_4BIT_ANIM, NULL, myanim_palette, 0, "mya.bin" },
};
```

The data file goes on the SD card; the palette is small enough to live in flash.

---

## Compile-Time Toggles

In `main_sd.c`:

| Macro | Default | Effect |
|-------|---------|--------|
| `HALL_SYNC` / `FIXED_TIMER` | `HALL_SYNC` | Column rate locked to motor RPM, or fixed at `FIXED_PERIOD` |
| `ISR_BUILD_ON_DMA_DONE` / `ISR_BUILD_IN_TIMER` | `ISR_BUILD_ON_DMA_DONE` | Where `BuildFrame()` runs |
| `FIXED_PERIOD` | `152000` | Initial / fallback Timer4A reload (80 MHz ticks) |
| `brightness` | `16` | Global LED brightness 0–31 (variable, not macro) |

In `Drivers/inc/Motor.h`:

| Macro | Default | Effect |
|-------|---------|--------|
| `MOTOR_SETPOINT` | `67` | Default duty% the spin-up button targets |
| `MIN_STARTSPEED` | `20` | Minimum duty% needed to overcome friction |
| `MAX_SPEED` | `90` | Hard ceiling for `Motor_SetSpeed()` |
| `MOTOR_SPINUP_MS` | `250` | ms per 1% step on spin-up |
| `MOTOR_SPINDOWN_MS` | `100` | ms per 1% step on spin-down |

---

## Interrupt Priority Map

| IRQ | Source | Priority | Notes |
|-----|--------|----------|-------|
| 94  | Wide Timer 0A (hall) | **1** | missed edge = wrong RPM = drift|
| 57  | SSI2 (LED DMA done) | 2 | Triggers next frame build |
| —   | SysTick (1 ms) | 2 | Motor ramp, stall check |
| 70  | Timer4A (column clock) | 3 | Fires `LEEEED()` |
| 92  | Timer5A (FatFs disk timer) | 4 | SD card timeouts |
| 1   | Port B (LCD buttons) | 5 | Human input |
| 30  | Port F (motor buttons) | 5 | Human input |

---

## Memory Footprint

```
SRAM (32 KB total):
  sdMem (image union)              ~20 KB
  DMA_FRAME + BUF_FRAME             1.2 KB
  stack, ISR vars, FatFs state    ~ 1.5 KB
  free                            ~ 9 KB

Flash:
  application code                  ~25 KB
  ROM image data                    ~20 KB per 8-bit image
  ROM palette data                   768 B per 8-bit, 48 B per 4-bit
  FatFs                             ~10 KB
```

ROM image data dominates flash usage. Each additional 8-bit image adds 140 × 143 = 20,020 bytes.

---

## Documentation

For deeper dives into specific subsystems:

- [Documentation/main_sd.md](Documentation/main_sd.md) — application architecture, mode switching, content table, init order
- [Documentation/Motor.md](Documentation/Motor.md) — PWM control, hall capture ISR, RPM math, ramp state machine
- [Documentation/SK9822.md](Documentation/SK9822.md) — frame builders, wire format, compressed formats
- [Documentation/SSI-DMA.md](Documentation/SSI-DMA.md) — LED DMA pipeline, ping-pong descriptors, SSI2 init
