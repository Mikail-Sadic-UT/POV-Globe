"""
globe_convert.py
================
Converts an equirectangular image to a POV globe column array for TM4C123.

Usage:
    python globe_convert.py image.png [options]

    --mode 8bit|4bit     Colour depth (default: 8bit)
    --contrast FLOAT     Contrast multiplier applied before quantisation,
                         e.g. 1.5 boosts contrast 50% (default: 1.0)

Globe geometry:
    144 LEDs span the blade radially (hub to tip).
    LED 0-71   : front face of blade
    LED 72-143 : back face  of blade, stored tip-to-hub so LED 72 sits at
                 the tip adjacent to LED 71 -- seamless image across blade centre.
    NUM_COLUMNS slices cover 180 deg (one full blade pass).
    Front and back faces together reconstruct a full 360 deg equirectangular image.

8-bit output  (--mode 8bit, default):
    palette.h / palette.c   --  const uint8_t palette[256][3]
    globe_data.h / .c       --  const uint8_t globe[NUM_COLUMNS][NUM_LEDS]
                                one byte per LED = palette index

4-bit output  (--mode 4bit):
    palette.h / palette.c   --  const uint8_t palette[16][3]
    globe_data.h / .c       --  const uint8_t globe[NUM_COLUMNS][NUM_LEDS/2]
                                two LEDs packed per byte:
                                  high nibble = even LED index  (i & 1 == 0)
                                  low  nibble = odd  LED index  (i & 1 == 1)
                                matches BuildFrame_4bit() in firmware.
"""

import os
import sys
import re
import argparse
from pathlib import Path
import numpy as np
from PIL import Image, ImageEnhance
from sklearn.cluster import MiniBatchKMeans

# =============================================================================
# CONFIG -- edit these values
# =============================================================================

NUM_COLUMNS   = 140     # Angular slices per 180 deg blade sweep
NUM_LEDS      = 143     # Total LEDs on the blade (LED 0 physically removed)
FRONT_LEDS    = 71      # LEDs on the front face (NUM_LEDS - FRONT_LEDS = back face)
OUTPUT_DIR    = "."     # Directory to write generated files into

# Palette sizes for each mode
PALETTE_SIZES = {
    "8bit": 256,
    "4bit": 16,
}

# =============================================================================
# Naming helpers
# =============================================================================

def sanitize_stem(path: str) -> str:
    stem = Path(path).stem
    stem = re.sub(r'[^A-Za-z0-9_]+', '_', stem).strip('_')
    return stem or "image"

def output_names(image_path: str) -> dict[str, str]:
    stem = sanitize_stem(image_path)
    return {
        "palette_h": f"{stem}_palette.h",
        "palette_c": f"{stem}_palette.c",
        "globe_h":   f"{stem}_data.h",
        "globe_c":   f"{stem}_data.c",
        "stem": stem,
    }

# =============================================================================
# Image loading + preprocessing
# =============================================================================

def load_image(path: str, contrast: float) -> np.ndarray:
    """
    Load and resize the equirectangular image.
    Width  = NUM_COLUMNS * 2  (full 360 deg at column resolution)
    Height = NUM_LEDS         (one pixel per LED radially)

    contrast: multiplier passed to PIL ImageEnhance.Contrast before
              quantisation. 1.0 = unchanged, >1 boosts, <1 flattens.
    """
    img_w = NUM_COLUMNS * 2
    img_h = NUM_LEDS
    img = Image.open(path).convert("RGB")
    img = img.resize((img_w, img_h), Image.LANCZOS)
    if contrast != 1.0:
        img = ImageEnhance.Contrast(img).enhance(contrast)
    return np.array(img, dtype=np.uint8)

# =============================================================================
# Palette generation
# =============================================================================

def build_palette(img: np.ndarray, palette_size: int) -> np.ndarray:
    """
    Build an optimal palette via k-means clustering.
    Returns (palette_size, 3) uint8 RGB array.
    """
    pixels = img.reshape(-1, 3).astype(np.float32)
    km = MiniBatchKMeans(
        n_clusters=palette_size,
        random_state=42,
        n_init=3,
        max_iter=100
    )
    km.fit(pixels)
    return np.clip(km.cluster_centers_, 0, 255).astype(np.uint8)

# =============================================================================
# Quantization
# =============================================================================

def quantize(img: np.ndarray, palette: np.ndarray) -> np.ndarray:
    """
    Map every pixel to its nearest palette index (squared Euclidean distance).
    Returns (height, width) uint8 index array.
    """
    pixels = img.reshape(-1, 3).astype(np.float32)
    pal_f  = palette.astype(np.float32)
    chunk  = 4096
    out    = np.empty(len(pixels), dtype=np.uint8)

    for i in range(0, len(pixels), chunk):
        diff = pixels[i:i+chunk, None, :] - pal_f[None, :, :]  # (chunk, P, 3)
        out[i:i+chunk] = (diff ** 2).sum(axis=2).argmin(axis=1).astype(np.uint8)

    return out.reshape(img.shape[:2])

# =============================================================================
# Globe sampling
# =============================================================================

def sample_globe(idx_img: np.ndarray) -> np.ndarray:
    """
    Sample the quantized index image into a (NUM_COLUMNS, NUM_LEDS) array.

    For each column c:
      angle_front = c / NUM_COLUMNS          (0.0-0.5 across left half of image)
      angle_back  = angle_front + 0.5        (opposite face, right half)

      Front LEDs (0..FRONT_LEDS-1):   sampled hub->tip (top->bottom of image)
      Back  LEDs (FRONT_LEDS..end):   sampled tip->hub (bottom->top of image)

    Returns uint8 array shape (NUM_COLUMNS, NUM_LEDS).
    """
    img_h, img_w = idx_img.shape
    back_leds    = NUM_LEDS - FRONT_LEDS
    globe        = np.zeros((NUM_COLUMNS, NUM_LEDS), dtype=np.uint8)

    for c in range(NUM_COLUMNS):
        a_front = c / NUM_COLUMNS
        a_back  = (a_front + 0.5) % 1.0

        x_front = int(a_front * img_w) % img_w
        x_back  = int(a_back  * img_w) % img_w

        for led in range(FRONT_LEDS):
            y = min(int(((FRONT_LEDS - 1 - led) / FRONT_LEDS) * img_h), img_h - 1)
            globe[c][led] = idx_img[y, x_front]

        for led in range(back_leds):
            y = min(int((led / back_leds) * img_h), img_h - 1)
            globe[c][FRONT_LEDS + led] = idx_img[y, x_back]

    # Force back-face hub LED (last in chain) to black for symmetry.
    # Front = 71 active LEDs, Back = 71 active + 1 black = symmetric.
    globe[:, NUM_LEDS - 1] = 0

    return globe

# =============================================================================
# 4-bit packing
# =============================================================================

def pack_4bit(globe: np.ndarray) -> np.ndarray:
    """
    Pack a (NUM_COLUMNS, NUM_LEDS) index array (values 0-15) into
    a (NUM_COLUMNS, packed_cols) byte array.

    Packing matches BuildFrame_4bit() in firmware:
        high nibble = even LED index  (i & 1 == 0)
        low  nibble = odd  LED index  (i & 1 == 1)

    Handles odd NUM_LEDS: last byte has one LED in high nibble, low nibble = 0.
    """
    assert globe.max() <= 15, "Palette index out of range for 4-bit mode (max 15)"

    packed_cols = (NUM_LEDS + 1) // 2  # 72 bytes for 143 LEDs
    packed = np.zeros((NUM_COLUMNS, packed_cols), dtype=np.uint8)

    for c in range(NUM_COLUMNS):
        for i in range(0, NUM_LEDS - 1, 2):
            packed[c, i // 2] = ((globe[c, i] & 0x0F) << 4) | (globe[c, i + 1] & 0x0F)
        # Odd last LED: high nibble only, low nibble = 0
        if NUM_LEDS % 2 == 1:
            packed[c, packed_cols - 1] = (globe[c, NUM_LEDS - 1] & 0x0F) << 4

    return packed

# =============================================================================
# C file writers
# =============================================================================

def write_palette_h(path: str, header_guard: str, palette_size: int, stem: str) -> None:
    with open(path, "w") as f:
        f.write(
            f"/* {os.path.basename(path)} -- auto-generated by globe_convert.py -- do not edit */\n"
            f"#ifndef {header_guard}\n"
            f"#define {header_guard}\n"
            "#include <stdint.h>\n"
            f"#define PALETTE_SIZE {palette_size}U\n"
            f"extern const uint8_t {stem}_palette[PALETTE_SIZE][3];  /* {{R, G, B}} per entry */\n"
            f"#endif /* {header_guard} */\n"
        )

def write_palette_c(palette: np.ndarray, path: str, header_name: str, stem: str) -> None:
    lines = [
        f"/* {os.path.basename(path)} -- auto-generated by globe_convert.py -- do not edit */",
        f'#include "{header_name}"',
        "",
        f"const uint8_t {stem}_palette[PALETTE_SIZE][3] = {{",
    ]
    for i, (r, g, b) in enumerate(palette):
        lines.append(f"    {{{r:3d}, {g:3d}, {b:3d}}},  /* {i:3d} */")
    lines += ["};", ""]
    with open(path, "w") as f:
        f.write("\n".join(lines))

def write_globe_h_8bit(path: str, header_guard: str, stem: str) -> None:
    with open(path, "w") as f:
        f.write(
            f"/* {os.path.basename(path)} -- auto-generated by globe_convert.py -- do not edit */\n"
            f"#ifndef {header_guard}\n"
            f"#define {header_guard}\n"
            "#include <stdint.h>\n"
            f"#define GLOBE_NUM_COLUMNS {NUM_COLUMNS}U  /* angular slices per 180 deg sweep */\n"
            f"#define GLOBE_NUM_LEDS    {NUM_LEDS}U     /* total LEDs per column */\n"
            f"/* {stem}_data[col][led] = palette index, one byte per LED */\n"
            f"extern const uint8_t {stem}_data[GLOBE_NUM_COLUMNS][GLOBE_NUM_LEDS];\n"
            f"#endif /* {header_guard} */\n"
        )

def write_globe_h_4bit(path: str, header_guard: str, stem: str) -> None:
    packed_leds = (NUM_LEDS + 1) // 2
    with open(path, "w") as f:
        f.write(
            f"/* {os.path.basename(path)} -- auto-generated by globe_convert.py -- do not edit */\n"
            f"#ifndef {header_guard}\n"
            f"#define {header_guard}\n"
            "#include <stdint.h>\n"
            f"#define GLOBE_NUM_COLUMNS   {NUM_COLUMNS}U  /* angular slices per 180 deg sweep */\n"
            f"#define GLOBE_NUM_LEDS      {NUM_LEDS}U     /* total LEDs per column */\n"
            f"#define GLOBE_PACKED_BYTES  {packed_leds}U  /* bytes per column (2 LEDs per byte) */\n"
            f"/* {stem}_data[col][byte]: high nibble = even LED, low nibble = odd LED\n"
            " * Pass column pointer directly to BuildFrame_4bit(). */\n"
            f"extern const uint8_t {stem}_data[GLOBE_NUM_COLUMNS][GLOBE_PACKED_BYTES];\n"
            f"#endif /* {header_guard} */\n"
        )

def write_globe_c(globe: np.ndarray, path: str, header_name: str, mode: str, stem: str) -> None:
    rows, cols = globe.shape
    comment = (
        f"/* {NUM_COLUMNS} columns x {NUM_LEDS} LEDs = {NUM_COLUMNS * NUM_LEDS:,} bytes */"
        if mode == "8bit" else
        f"/* {NUM_COLUMNS} columns x {NUM_LEDS} LEDs packed 4-bit = {rows * cols:,} bytes */"
    )
    dim = "GLOBE_NUM_LEDS" if mode == "8bit" else "GLOBE_PACKED_BYTES"
    lines = [
        f"/* {os.path.basename(path)} -- auto-generated by globe_convert.py -- do not edit */",
        f'#include "{header_name}"',
        "",
        comment,
        f"const uint8_t {stem}_data[GLOBE_NUM_COLUMNS][{dim}] = {{",
    ]
    for c in range(rows):
        row = ", ".join(f"0x{v:02X}" for v in globe[c])
        lines.append(f"    /* col {c:3d} */ {{{row}}},")
    lines += ["};", ""]
    with open(path, "w") as f:
        f.write("\n".join(lines))

# =============================================================================
# Entry point
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Convert equirectangular image to POV globe C arrays."
    )
    parser.add_argument("image", help="Input image file (equirectangular)")
    parser.add_argument(
        "--mode", choices=["8bit", "4bit"], default="8bit",
        help="Colour depth: 8bit = 256-colour palette (default), "
             "4bit = 16-colour palette packed 2 LEDs/byte"
    )
    parser.add_argument(
        "--contrast", type=float, default=1.0,
        metavar="FLOAT",
        help="Contrast enhancement factor applied before quantisation. "
             "1.0 = no change (default), >1 boosts contrast (e.g. 1.5), "
             "<1 flattens. Useful when source colours are perceptually similar "
             "(e.g. earth blues and greens)."
    )
    args = parser.parse_args()

    image_path   = args.image
    mode         = args.mode
    contrast     = args.contrast
    palette_size = PALETTE_SIZES[mode]

    if not os.path.exists(image_path):
        print(f"Error: '{image_path}' not found.")
        sys.exit(1)

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    print(f"[1/5] Loading:    {image_path}  ({NUM_COLUMNS*2}x{NUM_LEDS} px)"
          + (f"  [contrast x{contrast}]" if contrast != 1.0 else ""))
    img = load_image(image_path, contrast)

    print(f"[2/5] Palette:    k-means {palette_size} colours  [{mode}]")
    palette = build_palette(img, palette_size)

    print(f"[3/5] Quantize:   mapping pixels to {palette_size} palette indices")
    idx_img = quantize(img, palette)

    print(f"[4/5] Sampling:   {NUM_COLUMNS} columns x {NUM_LEDS} LEDs "
          f"({FRONT_LEDS} front / {NUM_LEDS - FRONT_LEDS} back)")
    globe = sample_globe(idx_img)

    if mode == "4bit":
        print(f"[4b]  Packing:   2 LEDs per byte (high nibble=even, low nibble=odd)")
        globe = pack_4bit(globe)

    names     = output_names(image_path)
    stem      = sanitize_stem(image_path)
    palette_h = os.path.join(OUTPUT_DIR, names["palette_h"])
    palette_c = os.path.join(OUTPUT_DIR, names["palette_c"])
    globe_h   = os.path.join(OUTPUT_DIR, names["globe_h"])
    globe_c   = os.path.join(OUTPUT_DIR, names["globe_c"])

    print(f"[5/5] Writing:    {os.path.abspath(OUTPUT_DIR)}/")
    write_palette_h(palette_h, f"{stem.upper()}_PALETTE_H", palette_size, stem)
    write_palette_c(palette, palette_c, os.path.basename(palette_h), stem)

    if mode == "8bit":
        write_globe_h_8bit(globe_h, f"{stem.upper()}_DATA_H", stem)
    else:
        write_globe_h_4bit(globe_h, f"{stem.upper()}_DATA_H", stem)

    write_globe_c(globe, globe_c, os.path.basename(globe_h), mode, stem)

    globe_bytes   = globe.size
    palette_bytes = palette_size * 3
    total         = globe_bytes + palette_bytes

    print(f"\n  mode       : {mode}")
    if contrast != 1.0:
        print(f"  contrast   : x{contrast}")
    print(f"  globe_data : {globe_bytes:,} bytes")
    print(f"  palette    : {palette_bytes:,} bytes")
    print(f"  total      : {total:,} bytes  ({total / 1024:.1f} KB)")

    if total > 32768:
        print(f"\n  NOTE: exceeds TM4C123 32 KB SRAM -- declare arrays as 'const' for flash.")
    else:
        print(f"  Fits in SRAM with {(32768 - total) / 1024:.1f} KB spare.")


if __name__ == "__main__":
    main()