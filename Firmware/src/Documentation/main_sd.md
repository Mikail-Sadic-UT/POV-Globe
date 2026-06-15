# main_sd.c — POV Globe Application

## Overview

The application layer for the POV LED globe. Coordinates four subsystems:

- **Frame builders** ([SK9822.md](SK9822.md)) — turn images into SPI byte streams
- **DMA driver** ([SSI-DMA.md](SSI-DMA.md)) — shifts frames out to the LEDs
- **Motor driver** ([Motor.md](Motor.md)) — keeps the blade spinning, provides hall feedback
- **LCD menu** — user input for mode/content selection

```
┌─────────────────┐
│  LCD menu       │ ── selects ───▶ Globe_ToggleMode / Globe_ChangeContent
│  (LCD.c)        │
└─────────────────┘
                                            │
                                            ▼
┌─────────────────┐         ┌────────────────────────────────┐
│  Motor (hall)   │ ──RPM──>│  Timer4A ISR  (HALL_SYNC mode) │
└─────────────────┘         │     ↓                          │
                            │  LEEEED() ──> LoadDMA()        │
                            │                  ↓             │
                            │   SSI2_Handler ──> BuildFrame()│
                            │     (DMA done)                 │
                            └────────────────────────────────┘
                                            │
                                            ▼
                            ┌────────────────────────────────┐
                            │  Mic / SD / ROM / palette LUT  │
                            └────────────────────────────────┘
```

---

## Display Modes

```c
typedef enum {
    MODE_8BIT_STATIC,   // single 8-bit image, ROM or SD
    MODE_4BIT_ANIM,     // 4-bit animation from SD, double-buffered
    MODE_MIC            // VU meter from PE3 ADC
} DisplayMode_t;
```

Three sources of image data feed the same DMA pipeline:

| Mode | Data source | Per-column behaviour |
|------|-------------|---------------------|
| `MODE_8BIT_STATIC` + `SRC_ROM` | flash array (`romData[col]`) | palette lookup |
| `MODE_8BIT_STATIC` + `SRC_SD`  | RAM (`sdMem.static8[col]`)   | palette lookup |
| `MODE_4BIT_ANIM`               | RAM (`sdActive[col]`)        | palette lookup, swap buffers at col=0 |
| `MODE_MIC`                     | ADC sample                 |   `BuildFrame_VU()` |

---

## Memory Budget

The TM4C123 has 32 KB of SRAM. The largest user is the SD/animation buffer pool, which uses a **union** to overlay 8-bit and 4-bit layouts on the same memory:

```c
static union {
    uint8_t static8[140][143];          // 20,020 B — 8-bit static
    struct {
        uint8_t a[140][72];             // 10,080 B — 4-bit buffer A
        uint8_t b[140][72];             // 10,080 B — 4-bit buffer B
    } anim4;
} sdMem;                                // 20,160 B total
```

In addition:

```
DMA_FRAME, BUF_FRAME (double SPI buffer)  =  2 × 592   = 1,184 B
sdMem (union)                             ≈            = 20,160 B
ROM image data (in flash, not SRAM)
Misc state                                ≈            = ~500 B
                                                         ───────
                                                        ~22 KB used
```

Adding more pictures or animations costs **flash** (palette + index data),
not SRAM, since `pictures[]` and `animations[]` are `const`.

---

## Double-Buffering Strategy

Two layers of double-buffering, used at different time scales:

### 1. SPI wire buffers (per column)

```
LIVE  →  being shifted out by DMA
BUFF  →  being filled by BuildFrame_xxx()

After DMA completes:
   swap(LIVE, BUFF)
   restart DMA on new LIVE
   build next column into new BUFF
```

This is what `LoadDMA()` does — it checks `dmaBusy`, swaps pointers if a new column is ready (`frameDirty`), and kicks the next transfer.

### 2. Animation frame buffers (per rotation)

```
sdActive  →  140 columns being read by BuildFrame_4bit
sdLoading →  next animation frame, loaded from SD in main loop

When col == 0 (rotation boundary) and sdReady:
   swap(sdActive, sdLoading)
   sdReady = 0
```

The swap only happens at the rotation boundary so the user never sees a torn frame mid-rotation. The main loop watches `sdReady` and starts loading the next frame as soon as the previous one is consumed.

---

## ISR Architecture

Two compile-time options control where work happens (see toggles in main_sd.c):

### `ISR_BUILD_IN_TIMER`
Both `LoadDMA()` and `BuildFrame()` run in the Timer4A ISR.

```
Timer4A tick:
   ├── HallSync_UpdatePeriod()    // ~3 µs
   ├── LoadDMA()                  // ~2 µs
   └── BuildFrame()               // ~10 µs
                                  total ~15 µs ISR
```

Simple. Fine up to ~50 kHz column rate.

### `ISR_BUILD_ON_DMA_DONE`  (current default)

Timer4A only kicks DMA. Frame building runs from `SSI2_Handler` after the SPI transfer completes.

```
Timer4A tick:                    SSI2 DMA-done (~30 µs after timer):
   ├── HallSync_UpdatePeriod()      └── DMA_Done_Callback()
   └── LoadDMA()                          └── BuildFrame()
       (~5 µs ISR)                              (~10 µs ISR)
```

CPU isn't blocked during the SPI transfer — `BuildFrame()` runs in parallel with SPI shifting. Better for high column rates.

`DMA_Done_Callback()` is called from `SSI2_Handler` (in SK9822_DMA_SSI.c).

---

## Timing: Fixed vs Hall-Sync

```
#define HALL_SYNC      // (current default)
// #define FIXED_TIMER
```

### `FIXED_TIMER`
Timer4A fires at a constant period (`FIXED_PERIOD = 152000` ticks at 80 MHz, ≈526 Hz). Image position drifts as motor RPM varies. (Only as accurate as what you are reading directly from the motor, and slipage down the line will cause image drift)

### `HALL_SYNC`
Every Timer4A tick, the period is recomputed from the most recent measured motor revolution:

```
colPeriod = revPeriod × MOTOR_GEAR_RATIO / SD_NUM_COLUMNS
          = revPeriod × 6 / 140
```

Image stays locked to physical position even if motor RPM drifts. Until the first revolution is measured, falls back to `FIXED_PERIOD`.

The clamps `5000 ≤ colPeriod ≤ 400000` bound the column rate to roughly **200 Hz – 16 kHz**, avoids silly values and nothing happening when motor off.

---

## Content Table

`pictures[]` and `animations[]` register all selectable content. Each entry is a `ContentEntry_t`:

```c
typedef struct {
    const char       *name;
    ContentType_t     type;          // ROM_8BIT / SD_8BIT / SD_4BIT_ANIM
    const uint8_t   (*romData)[NUM_LEDS];
    const uint8_t   (*palette)[3];
    uint16_t          romCols;
    const char       *sdFilename;
} ContentEntry_t;
```

### Adding new content

**ROM-resident image** (compiled into flash, no SD card needed):

1. Run `globe_convert.py` on your image
2. Copy the generated `*_data.h/c` and `*_palette.h/c` into `Data/myImage/`
3. `#include` the headers in main_sd.c
4. Add an entry to `pictures[]`:
   ```c
   { "MyImage", CONTENT_ROM_8BIT, my_data, my_palette, GLOBE_NUM_COLUMNS, NULL },
   ```

**SD card animation** (4-bit GIF, no flash cost):

1. Convert the GIF: `python globe_convert_gif.py myAnim.gif --mode 4bit`
2. Copy the resulting `myAnim.bin` to the SD card
3. `#include` the palette header (palette still goes in flash)
4. Add an entry to `animations[]`:
   ```c
   { "MyAnim", CONTENT_SD_4BIT_ANIM, NULL, my_palette, 0, "mya.bin" },
   ```

The SD filename is limited to 8.3 characters by FatFs in default config.

---

## LCD Menu Integration

The LED menu in `LCD.c` calls into main_sd.c through two extern functions:

```c
void Globe_ToggleMode(void);    // cycles picture → animation → mic → picture
void Globe_ChangeContent(void); // cycles within current category
```

`ledModeAnim` is shared between the two files (defined in LCD.c, read here):

| Value | Meaning |
|-------|---------|
| 0 | Picture mode |
| 1 | Animation mode |
| 2 | Mic mode |

LCD.c handles the button input and screen rendering. main_sd.c handles the actual mode switching and content loading.

---

## Mic Mode Detail

```c
if (displayMode == MODE_MIC) {
    raw = Mic_Read();
    uint16_t level = ((raw * FRONT_LEDS) / 4096) - 33;
    BuildFrame_VU(BUFF, level, brightness);
}
```

`Mic_Read()` triggers ADC0 sequencer 3 (single sample on PE3/AIN0) and polls for completion. Returns 0–4095.

Scaling to LED count:
```
level = raw × 71 / 4096 - 33
```

The `-33` offset trims out ambient ADC bias so the bar sits near zero in silence. Tune per microphone.

`BuildFrame_VU()` ([SK9822.md](SK9822.md)) draws a green→yellow→red bar from hub to tip on both faces.

The mic is sampled once per column (~500 Hz with default timing). The current code shows the *instantaneous* sample.

---

## Initialisation Sequence

`INITIALIZE()` brings up subsystems in dependency order:

```
1. PLL_Init(80MHz)               clock first — everything else needs it
2. SK9822_20MHZ()                SSI2 + GPIO PB4/PB7
3. DMA_Init()                    uDMA controller
4. SK9822_DMA_CH13_Init()        LED DMA channel
5. SD_DMA_CH10_Init()            SD card DMA channel
6. Motor_Init()                  PWM + hall capture
7. Motor_Stop()                  ensure motor is off
8. PF_Init() / PB_Init()         button GPIO + IRQs
9. SysTick_Init(1ms)             tick + ramp + stall check
10. Mic_Init()                   ADC0 seq3
11. BuildFrame_Solid(LIVE/BUFF)  clear LED buffers
12. LCD_Menu_Init()              ST7735 + initial render
```

`Timer4A_Init()` is called from `main()` after `SetMode_ROM()` loads the first picture, so the ISR doesn't fire on uninitialised data.

---

## Main Loop

```c
while (1) {
    if (displayMode == MODE_4BIT_ANIM && !sdReady)
        SD_LoadNextFrame();          // SD I/O happens here, not in ISR

    buttonHandler();                  // consume LCD button flags

    if (motor_state_changed) {
        // redraw motor status indicator on LCD
    }
}
```

All slow / blocking operations (SD card reads, LCD redraws) happen here so the ISRs stay short.

---

## Compile-time Toggles Summary

| Toggle | Default | Effect |
|--------|---------|--------|
| `HALL_SYNC` / `FIXED_TIMER` | `HALL_SYNC` | Column rate locked to motor or fixed |
| `ISR_BUILD_ON_DMA_DONE` / `ISR_BUILD_IN_TIMER` | `ISR_BUILD_ON_DMA_DONE` | Where BuildFrame runs |
| `FIXED_PERIOD` | `152000` | Initial / fallback Timer4A period |
| `brightness` (variable) | `16` | Global LED brightness, 0–31 |
