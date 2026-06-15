# globe_convert.py

Converts an equirectangular image into C arrays ready for use on the TM4C123 POV globe driver.

---

## Usage

```bash
python globe_convert.py <image>
```

**Example:**
```bash
python globe_convert.py world_map.png
```

All tuning parameters are configured inside the script — no command-line flags needed.

---

## Configuration

Edit the `CONFIG` block at the top of the script before running:

```python
NUM_COLUMNS   = 140     # Angular slices per 180° blade sweep <-- Should be theoretical limit
NUM_LEDS      = 144     # Total LEDs on the blade
FRONT_LEDS    = 72      # LEDs on the front face
PALETTE_SIZE  = 256     # Colours in the 8-bit palette (max 256)
OUTPUT_DIR    = "."     # Directory to write generated files into
```

| Parameter | Description |
|---|---|
| `NUM_COLUMNS` | Angular resolution — number of image slices per half rotation. 140 gives ~1.28° per step at 900 RPM. |
| `NUM_LEDS` | Total LEDs on the blade. Must match `NUM_LEDS` in `SK9822.h`. |
| `FRONT_LEDS` | How many LEDs are on the front face. Remaining LEDs are the back face. |
| `PALETTE_SIZE` | Number of colours in the palette. 256 = 8-bit. Reducing to 16 enables 4-bit mode. |
| `OUTPUT_DIR` | Where the four output files are written. Created if it doesn't exist. |

---

## Input Image

The script for now expects an **equirectangular projection** image (but will work with whatever):

- **X axis** → angular position (0–360°, left to right)
- **Y axis** → radial position (hub at top, tip at bottom)

The image is automatically resized to `(NUM_COLUMNS × 2) × NUM_LEDS` pixels internally, so any input resolution works.

Standard equirectangular images (world maps, 360° photos) work directly. If the image is not equirectangular it will produce distorted output. Will probably have to make another script to do this, or figure something out for general photos.

---

## Globe Geometry

The blade has 144 LEDs arranged radially from hub to tip:

```
Hub                                       Tip                                      Hub
 |-------- Front face (LED 0–71) ----------|-------- Back face (LED 72–143) --------|
```

- **Front face** (LED 0–71): sampled hub→tip from the column angle
- **Back face** (LED 72–143): sampled tip→hub from column angle + 180°, stored in reverse radial order so LED 72 sits at the tip adjacent to LED 71

This means the image is seamless across the physical blade centre — the pixel at LED 71 (front tip) and LED 72 (back tip) are adjacent in the source image.

A full rotation is reconstructed from two blade passes:

```
Pass 1 (0°–180°):   columns 0–139   →  front face = 0°–180°,  back face = 180°–360°
Pass 2 (180°–360°): same columns    →  front face = 180°–360°, back face = 0°–180°
```

---

## Processing Pipeline

```
Load image
    │
    ▼
Resize to (NUM_COLUMNS×2) × NUM_LEDS
    │
    ▼
K-means clustering (256 colours)  ←  builds optimal palette for this image
    │
    ▼
Quantize pixels → palette indices
    │
    ▼
Sample globe columns
    │   for each column: sample front strip at angle,
    │                    sample back strip at angle + 180°
    ▼
Write C files
```

### Palette generation

The palette is built using **MiniBatchKMeans clustering** — it finds the 256 colours that best represent the full colour distribution of the image. This is strictly better than a fixed 3-3-2 mapping because the 256 entries are chosen specifically for your image rather than being uniformly distributed.

### Quantization

Each pixel is mapped to its nearest palette entry using squared Euclidean distance in RGB space. Processed in chunks of 4096 pixels to avoid memory issues on large images.

---

## Output Files

Four files are generated in `OUTPUT_DIR`:

| File | Contents | Size |
|---|---|---|
| `palette.h` | `extern` declaration, `PALETTE_SIZE` define | — |
| `palette.c` | `const uint8_t palette[256][3]` — RGB entries | 768 bytes |
| `globe_data.h` | `extern` declaration, `GLOBE_NUM_COLUMNS` / `GLOBE_NUM_LEDS` defines | — |
| `globe_data.c` | `const uint8_t globe[140][144]` — palette indices | 20,160 bytes |

**Total: ~20,928 bytes (~20.4 KB)** — fits in TM4C123 32 KB SRAM with ~11.6 KB spare.

> Set as `const` to place them in flash (256 KB) instead of SRAM, freeing SRAM for frame buffers and stack.

---

## MCU Integration

Add the four generated files to your project under `Data/`:

```
Data/
  palette.h
  palette.c
  globe_data.h
  globe_data.c
```

Include in your main file and call `BuildFrame_8bit` with the current column:

```c
#include "../Data/globe_data.h"
#include "../Data/palette.h"

uint16_t col        = 0;
uint8_t  brightness = 2;   // 0-31

// In main loop:
if(framePending){
    BuildFrame_8bit(BUFF, globe[col], palette, brightness);
    col = (col + 1) % GLOBE_NUM_COLUMNS;
    frameDirty  = 1;
    framePending = 0;
}
```

`BuildFrame_8bit` expands the 144 palette indices for column `col` into the full 596-byte SK9822 wire format (start frame + BGR LED frames + end frame) and writes it into `BUFF`.

---

## Dependencies

```bash
pip install pillow numpy scikit-learn
```

| Package | Use |
|---|---|
| `Pillow` | Image loading and resizing |
| `numpy` | Array operations and vectorised quantization |
| `scikit-learn` | `MiniBatchKMeans` for palette generation |

---

## Notes

- **One palette per image** — the palette is optimised for the specific input image. If you load multiple images on the globe, each needs its own palette, or you can generate a shared palette by running k-means across all images combined.
- **Equirectangular only** — other projections (Mercator, stereographic, etc.) will produce distorted output without a pre-conversion step.
- **Brightness** — the `brightness` parameter (0–31) is the SK9822 global brightness field, separate from pixel colour values. Stay low (1–3) because this shit is bright af.