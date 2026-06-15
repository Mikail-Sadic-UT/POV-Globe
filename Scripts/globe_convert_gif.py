"""
globe_convert_gif.py
====================
Converts an equirectangular GIF to a POV globe animation array for TM4C123.

Usage:
    python globe_convert_gif.py animation.gif [options]

    --mode 8bit|4bit     Colour depth (default: 8bit)
    --contrast FLOAT     Contrast multiplier applied before quantisation,
                         e.g. 1.5 boosts contrast 50% (default: 1.0)

Output files are named after the input GIF, e.g. cat_space_data.h/.c and
cat_space_palette.h/.c. All C identifiers inside the files match this name.

Globe geometry:
    144 LEDs span the blade radially (hub to tip).
    LED 0-71   : front face of blade
    LED 72-143 : back face  of blade, stored tip-to-hub so LED 72 sits at
                 the tip adjacent to LED 71 -- seamless image across blade centre.
    NUM_COLUMNS slices cover 180 deg (one full blade pass).
    Front and back faces together reconstruct a full 360 deg equirectangular image.

8-bit output  (--mode 8bit, default):
    <n>_palette.h / .c   -- const uint8_t <n>_palette[256][3]
    <n>_data.h    / .c   -- const uint8_t <n>[NUM_FRAMES][NUM_COLUMNS][NUM_LEDS]
                            one byte per LED = palette index

4-bit output  (--mode 4bit):
    <n>_palette.h / .c   -- const uint8_t <n>_palette[16][3]
    <n>_data.h    / .c   -- const uint8_t <n>[NUM_FRAMES][NUM_COLUMNS][NUM_LEDS/2]
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
from PIL import Image, ImageSequence, ImageEnhance
from sklearn.cluster import MiniBatchKMeans

# =============================================================================
# CONFIG -- edit these values
# =============================================================================
NUM_COLUMNS  = 140   # Angular slices per 180 deg blade sweep
NUM_LEDS     = 144   # Total LEDs on the blade
FRONT_LEDS   = 72    # LEDs on the front face (NUM_LEDS - FRONT_LEDS = back face)
OUTPUT_DIR   = "."   # Parent directory; files go into OUTPUT_DIR/<base_name>/

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

# =============================================================================
# GIF loading
# =============================================================================

def load_gif_frames(gif_path: str, contrast: float) -> tuple[list[np.ndarray], list[int]]:
    """
    Extract every frame from a GIF, composite onto a black background
    (handles transparency / partial-update frames), resize to globe resolution.

    contrast: multiplier passed to PIL ImageEnhance.Contrast before
              quantisation. 1.0 = unchanged, >1 boosts, <1 flattens.

    Returns:
        frames  -- list of (NUM_LEDS, NUM_COLUMNS*2, 3) uint8 RGB arrays
        delays  -- list of per-frame delays in milliseconds
    """
    img_w = NUM_COLUMNS * 2
    img_h = NUM_LEDS

    gif = Image.open(gif_path)
    frames: list[np.ndarray] = []
    delays: list[int] = []

    canvas = Image.new("RGB", gif.size, (0, 0, 0))

    for frame in ImageSequence.Iterator(gif):
        frame_rgba = frame.convert("RGBA")
        canvas.paste(frame_rgba, mask=frame_rgba.split()[3])
        resized = canvas.copy().resize((img_w, img_h), Image.LANCZOS)
        if contrast != 1.0:
            resized = ImageEnhance.Contrast(resized).enhance(contrast)
        frames.append(np.array(resized.convert("RGB"), dtype=np.uint8))
        delays.append(frame.info.get("duration", 100))

    return frames, delays


# =============================================================================
# Palette generation  (global -- built across ALL frames)
# =============================================================================

def build_palette(frames: list[np.ndarray], palette_size: int) -> np.ndarray:
    """
    Build a single global palette covering every frame via k-means.
    Returns (palette_size, 3) uint8 RGB array.
    """
    rng = np.random.default_rng(42)
    samples_per_frame = max(1024, 16384 // len(frames))

    pixel_pool: list[np.ndarray] = []
    for frame in frames:
        pixels = frame.reshape(-1, 3).astype(np.float32)
        idx = rng.choice(len(pixels), size=min(samples_per_frame, len(pixels)), replace=False)
        pixel_pool.append(pixels[idx])

    all_pixels = np.concatenate(pixel_pool, axis=0)
    km = MiniBatchKMeans(n_clusters=palette_size, random_state=42, n_init=3, max_iter=200)
    km.fit(all_pixels)
    return np.clip(km.cluster_centers_, 0, 255).astype(np.uint8)


# =============================================================================
# Quantization
# =============================================================================

def quantize(img: np.ndarray, palette: np.ndarray) -> np.ndarray:
    """Map every pixel to its nearest palette index. Returns (h, w) uint8 array."""
    pixels = img.reshape(-1, 3).astype(np.float32)
    pal_f  = palette.astype(np.float32)
    chunk  = 4096
    out    = np.empty(len(pixels), dtype=np.uint8)
    for i in range(0, len(pixels), chunk):
        diff = pixels[i:i+chunk, None, :] - pal_f[None, :, :]
        out[i:i+chunk] = (diff ** 2).sum(axis=2).argmin(axis=1).astype(np.uint8)
    return out.reshape(img.shape[:2])


# =============================================================================
# Globe sampling
# =============================================================================

def sample_globe(idx_img: np.ndarray) -> np.ndarray:
    """
    Sample a quantized frame into (NUM_COLUMNS, NUM_LEDS).
    Front LEDs hub->tip; back LEDs tip->hub for seamless blade join.
    """
    img_h, img_w = idx_img.shape
    back_leds = NUM_LEDS - FRONT_LEDS
    globe = np.zeros((NUM_COLUMNS, NUM_LEDS), dtype=np.uint8)

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

    return globe


# =============================================================================
# 4-bit packing
# =============================================================================

def pack_4bit(globe: np.ndarray) -> np.ndarray:
    """
    Pack a (NUM_COLUMNS, NUM_LEDS) index array (values 0-15) into
    a (NUM_COLUMNS, NUM_LEDS//2) byte array.

    Packing matches BuildFrame_4bit() in firmware:
        high nibble = even LED index  (i & 1 == 0)
        low  nibble = odd  LED index  (i & 1 == 1)

    NUM_LEDS must be even (144 is fine).
    """
    assert NUM_LEDS % 2 == 0, "NUM_LEDS must be even for 4-bit packing"
    assert globe.max() <= 15,  "Palette index out of range for 4-bit mode (max 15)"

    even = globe[:, 0::2]  # columns of even LEDs
    odd  = globe[:, 1::2]  # columns of odd  LEDs
    packed = ((even & 0x0F) << 4) | (odd & 0x0F)
    return packed


# =============================================================================
# C file writers  -- every identifier inside uses base_name
# =============================================================================

def write_palette_h(dest: str, header_guard: str, palette_size: int, stem: str) -> None:
    with open(dest, "w") as fh:
        fh.write(
            f"/* {os.path.basename(dest)} -- auto-generated by globe_convert_gif.py -- do not edit */\n"
            f"#ifndef {header_guard}\n"
            f"#define {header_guard}\n"
            "#include <stdint.h>\n"
            f"#define PALETTE_SIZE {palette_size}U\n"
            f"extern const uint8_t {stem}_palette[PALETTE_SIZE][3]; /* {{R, G, B}} per entry */\n"
            f"#endif /* {header_guard} */\n"
        )


def write_palette_c(palette: np.ndarray, dest: str, header_name: str, stem: str) -> None:
    lines = [
        f"/* {os.path.basename(dest)} -- auto-generated by globe_convert_gif.py -- do not edit */",
        f'#include "{header_name}"',
        "",
        f"const uint8_t {stem}_palette[PALETTE_SIZE][3] = {{",
    ]
    for i, (r, g, b) in enumerate(palette):
        lines.append(f"    {{{r:3d}, {g:3d}, {b:3d}}},  /* {i:3d} */")
    lines += ["};", ""]
    with open(dest, "w") as fh:
        fh.write("\n".join(lines))


def write_data_h_8bit(dest: str, header_guard: str, stem: str,
                      num_frames: int, delays: list[int]) -> None:
    prefix = stem.upper()
    delay_list = ", ".join(str(d) for d in delays)
    with open(dest, "w") as fh:
        fh.write(
            f"/* {os.path.basename(dest)} -- auto-generated by globe_convert_gif.py -- do not edit */\n"
            f"#ifndef {header_guard}\n"
            f"#define {header_guard}\n"
            "#include <stdint.h>\n"
            f"#define {prefix}_NUM_FRAMES  {num_frames}U  /* GIF frames */\n"
            f"#define {prefix}_NUM_COLUMNS {NUM_COLUMNS}U /* angular slices per 180 deg sweep */\n"
            f"#define {prefix}_NUM_LEDS    {NUM_LEDS}U    /* total LEDs per column */\n"
            "\n"
            "/* Per-frame display duration in milliseconds (from GIF timing) */\n"
            f"extern const uint16_t {stem}_frame_delay_ms[{prefix}_NUM_FRAMES]; /* {{{delay_list}}} */\n"
            "\n"
            f"/* {stem}[frame][col][led] = palette index, one byte per LED */\n"
            f"extern const uint8_t {stem}[{prefix}_NUM_FRAMES][{prefix}_NUM_COLUMNS][{prefix}_NUM_LEDS];\n"
            f"#endif /* {header_guard} */\n"
        )


def write_data_h_4bit(dest: str, header_guard: str, stem: str,
                      num_frames: int, delays: list[int]) -> None:
    prefix = stem.upper()
    packed_leds = NUM_LEDS // 2
    delay_list = ", ".join(str(d) for d in delays)
    with open(dest, "w") as fh:
        fh.write(
            f"/* {os.path.basename(dest)} -- auto-generated by globe_convert_gif.py -- do not edit */\n"
            f"#ifndef {header_guard}\n"
            f"#define {header_guard}\n"
            "#include <stdint.h>\n"
            f"#define {prefix}_NUM_FRAMES    {num_frames}U  /* GIF frames */\n"
            f"#define {prefix}_NUM_COLUMNS   {NUM_COLUMNS}U /* angular slices per 180 deg sweep */\n"
            f"#define {prefix}_NUM_LEDS      {NUM_LEDS}U    /* total LEDs per column */\n"
            f"#define {prefix}_PACKED_BYTES  {packed_leds}U /* bytes per column (2 LEDs per byte) */\n"
            "\n"
            "/* Per-frame display duration in milliseconds (from GIF timing) */\n"
            f"extern const uint16_t {stem}_frame_delay_ms[{prefix}_NUM_FRAMES]; /* {{{delay_list}}} */\n"
            "\n"
            f"/* {stem}[frame][col][byte]: high nibble = even LED, low nibble = odd LED\n"
            " * Pass column pointer directly to BuildFrame_4bit(). */\n"
            f"extern const uint8_t {stem}[{prefix}_NUM_FRAMES][{prefix}_NUM_COLUMNS][{prefix}_PACKED_BYTES];\n"
            f"#endif /* {header_guard} */\n"
        )


def write_data_c(globe_anim: np.ndarray, delays: list[int], dest: str,
                 header_name: str, mode: str, stem: str) -> None:
    num_frames = globe_anim.shape[0]
    prefix = stem.upper()
    led_dim = f"{prefix}_NUM_LEDS" if mode == "8bit" else f"{prefix}_PACKED_BYTES"

    if mode == "8bit":
        comment = (f"/* {num_frames} frames x {NUM_COLUMNS} columns x {NUM_LEDS} LEDs"
                   f" = {num_frames * NUM_COLUMNS * NUM_LEDS:,} bytes */")
    else:
        comment = (f"/* {num_frames} frames x {NUM_COLUMNS} columns x {NUM_LEDS} LEDs packed 4-bit"
                   f" = {globe_anim.size:,} bytes */")

    lines = [
        f"/* {os.path.basename(dest)} -- auto-generated by globe_convert_gif.py -- do not edit */",
        f'#include "{header_name}"',
        "",
        comment,
        "",
    ]

    delay_vals = ", ".join(str(d) for d in delays)
    lines.append(f"const uint16_t {stem}_frame_delay_ms[{prefix}_NUM_FRAMES] = {{")
    lines.append(f"    {delay_vals}")
    lines.append("};")
    lines.append("")

    lines.append(f"const uint8_t {stem}[{prefix}_NUM_FRAMES][{prefix}_NUM_COLUMNS][{led_dim}] = {{")
    for f_idx in range(num_frames):
        lines.append(f"  /* frame {f_idx} */ {{")
        for c in range(globe_anim.shape[1]):
            row = ", ".join(f"0x{v:02X}" for v in globe_anim[f_idx, c])
            lines.append(f"    /* col {c:3d} */ {{{row}}},")
        lines.append("  },")
    lines += ["};", ""]

    with open(dest, "w") as fh:
        fh.write("\n".join(lines))


# =============================================================================
# Entry point
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Convert equirectangular GIF to POV globe animation C arrays."
    )
    parser.add_argument("gif", help="Input GIF file (equirectangular)")
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
             "<1 flattens."
    )
    args = parser.parse_args()

    gif_path     = args.gif
    mode         = args.mode
    contrast     = args.contrast
    palette_size = PALETTE_SIZES[mode]

    if not os.path.exists(gif_path):
        print(f"Error: '{gif_path}' not found.")
        sys.exit(1)

    stem    = sanitize_stem(gif_path)
    out_dir = os.path.join(OUTPUT_DIR, stem)
    os.makedirs(out_dir, exist_ok=True)

    def out(filename: str) -> str:
        return os.path.join(out_dir, filename)

    print(f"[1/6] Loading GIF frames: {gif_path}"
          + (f"  [contrast x{contrast}]" if contrast != 1.0 else ""))
    frames, delays = load_gif_frames(gif_path, contrast)
    num_frames = len(frames)
    print(f"      {num_frames} frames, {NUM_COLUMNS*2}x{NUM_LEDS} px each, "
          f"delays: {delays[:8]}{'...' if num_frames > 8 else ''} ms")

    print(f"[2/6] Palette: k-means {palette_size} colours across all frames  [{mode}]")
    palette = build_palette(frames, palette_size)

    print(f"[3/6] Quantizing {num_frames} frames to {palette_size} palette indices...")
    idx_frames: list[np.ndarray] = []
    for i, frame in enumerate(frames):
        print(f"      frame {i+1}/{num_frames}", end="\r", flush=True)
        idx_frames.append(quantize(frame, palette))
    print()

    print(f"[4/6] Sampling globe columns ({NUM_COLUMNS} cols x {NUM_LEDS} LEDs per frame)...")
    globe_anim = np.zeros((num_frames, NUM_COLUMNS, NUM_LEDS), dtype=np.uint8)
    for i, idx_img in enumerate(idx_frames):
        print(f"      frame {i+1}/{num_frames}", end="\r", flush=True)
        globe_anim[i] = sample_globe(idx_img)
    print()

    if mode == "4bit":
        print(f"[4b]  Packing: 2 LEDs per byte (high nibble=even, low nibble=odd)")
        packed = np.zeros((num_frames, NUM_COLUMNS, NUM_LEDS // 2), dtype=np.uint8)
        for i in range(num_frames):
            packed[i] = pack_4bit(globe_anim[i])
        globe_anim = packed

    palette_h = out(f"{stem}_palette.h")
    palette_c = out(f"{stem}_palette.c")
    data_h    = out(f"{stem}_data.h")
    data_c    = out(f"{stem}_data.c")
    guard     = f"{stem.upper()}_DATA_H"

    print(f"[5/6] Writing C files to: {os.path.abspath(out_dir)}/")
    write_palette_h(palette_h, f"{stem.upper()}_PALETTE_H", palette_size, stem)
    write_palette_c(palette, palette_c, os.path.basename(palette_h), stem)

    if mode == "8bit":
        write_data_h_8bit(data_h, guard, stem, num_frames, delays)
    else:
        write_data_h_4bit(data_h, guard, stem, num_frames, delays)

    write_data_c(globe_anim, delays, data_c, os.path.basename(data_h), mode, stem)

    # -------------------------------------------------------------------------
    # Memory report
    # -------------------------------------------------------------------------
    data_bytes    = globe_anim.size
    palette_bytes = palette_size * 3
    delay_bytes   = num_frames * 2   # uint16_t per frame
    total         = data_bytes + palette_bytes + delay_bytes

    print(f"\n  mode           : {mode}")
    if contrast != 1.0:
        print(f"  contrast       : x{contrast}")
    print(f"  {stem} data : {data_bytes:,} bytes  ({num_frames} frames)")
    print(f"  palette        : {palette_bytes:,} bytes")
    print(f"  frame delays   : {delay_bytes:,} bytes")
    print(f"  total          : {total:,} bytes ({total / 1024:.1f} KB)")

    if total > 32768:
        print(f"\n  NOTE: exceeds TM4C123 32 KB SRAM -- declare arrays as 'const'"
              f" for flash (up to 256 KB).")
    else:
        print(f"  Fits in SRAM with {(32768 - total) / 1024:.1f} KB spare.")


if __name__ == "__main__":
    main()
