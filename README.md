# POV Globe

A persistence-of-vision LED globe. A blade carrying a strip of SK9822 LEDs spins on a brushless motor; image columns are clocked out in sync with the motor's hall sensor so a stable spherical image hangs in mid-air.

---

## How It Works

A hall sensor feeds back the motor's rotation period. A timer divides that period across the number of angular columns and fires at that rate — each tick, the next column of image data is pushed out to the LEDs via DMA while the CPU stays free.

Three display modes:

| Mode | Description |
|------|-------------|
| Static image | Indexed image loaded from ROM or SD card |
| Animation | GIF streamed from SD card, double-buffered per rotation |
| Mic / VU meter | Electret mic drives a colour bar from hub to tip |

---

## Repository Layout

```
POV-Globe/
├── Firmware/     Code Composer Studio project (TM4C123GH6PM)
├── Hardware/     KiCad 9.0 PCB design
├── Scripts/      Python image conversion tools
└── BOM.xlsx      Bill of materials
```

---

## Firmware

Targets the TI TM4C123GH6PM (Tiva C) using [Code Composer Studio](https://www.ti.com/tool/CCSTUDIO).

### Quick Start

1. Open the project in Code Composer Studio (`Firmware/src/`).
2. Make sure `main_sd.c` is the active entry point.
3. Connect the board and hit **Debug** — CCS flashes and starts execution.
4. Use the LCD buttons to navigate the menu, select content, and control the motor.

### Adding Content

**ROM image** (compiled into flash, no SD card needed):

```bash
cd Scripts
python globe_convert.py myimage.png
```

Move the output into `Firmware/src/Data/myimage/` and register the entry in `main_sd.c`. See [`Scripts/Documentation.md`](Scripts/Documentation.md) for details.

**SD card animation** (streamed at runtime):

```bash
python globe_convert_gif.py myanim.gif
```

Copy the `.bin` file to the SD card and register the palette entry in `main_sd.c`. See [`Scripts/Documentation.md`](Scripts/Documentation.md) for details.

### Docs

Detailed module documentation lives in `Firmware/src/Documentation/`:

- [`main_sd.md`](Firmware/src/Documentation/main_sd.md) — application architecture, display modes, init order
- [`Motor.md`](Firmware/src/Documentation/Motor.md) — PWM control, hall RPM capture, ramp state machine
- [`SK9822.md`](Firmware/src/Documentation/SK9822.md) — frame builders and wire format
- [`SSI-DMA.md`](Firmware/src/Documentation/SSI-DMA.md) — DMA pipeline and SPI setup

---

## Hardware

KiCad 9.0 is required to open the schematic and PCB files. All design files and documentation are inside the `Hardware/` folder.

---

## Scripts

| Script | Purpose |
|--------|---------|
| `globe_convert.py` | Converts an equirectangular image to C arrays for ROM or SD use |
| `globe_convert_gif.py` | Converts a GIF to a binary + palette for SD card streaming |
| `toBin.py` | Binary packing utility |

See [`Scripts/Documentation.md`](Scripts/Documentation.md) for full usage.
