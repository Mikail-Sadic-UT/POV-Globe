/**
 * @file SK9822.h
 * @brief SK9822 frame buffer builders for TM4C123 POV globe driver.
 *
 * All builders write a complete FRAMESIZE byte buffer:
 *   [start: 4 × 0x00] [143 × LED frame] [end: 16 × 0xFF]
 *
 * LED frame wire order is BGR (SK9822 datasheet §5). All functions accept
 * RGB and reorder internally — callers always pass natural (r, g, b) order.
 *
 * Strip geometry (after LED 0 removal, it broked):
 *   Physical chain = 143 LEDs. (broke one)
 *   Front face: LED 0–70   (71 LEDs, hub → tip)
 *   Back  face: LED 71–142 (72 LEDs, tip → hub)
 *   LED 142 (back hub) is forced black so both faces show 71 active LEDs,
 *   keeping the image symmetric across the blade centre.
 *
 * Compressed formats expand to full 24-bit at build time:
 *
 *   Format        | Bytes/LED | Colors        | RAM per rotational frame
 *   1-bit         | 1/8       | 2 (+ brt LUT) | ~2.5 KB
 *   4-bit palette | 1/2       | 16 chosen     | ~10  KB
 *   8-bit 332     | 1         | 256 fixed     | ~20  KB
 *   8-bit palette | 1         | 256 chosen    | ~20  KB
 *   16-bit 565    | 2         | 65K           | ~40  KB
 */

#ifndef SK9822_H
#define SK9822_H

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Strip geometry
 *
 * NUM_LEDS = 143 (physical chain after LED 0 removal)
 * FRONT_LEDS = 71 (hub to tip on front face)
 * Back face = NUM_LEDS - FRONT_LEDS = 72 (tip to hub), last LED forced black
 * Active image LEDs per face = 71 (symmetric)
 * --------------------------------------------------------------------------- */

#define NUM_LEDS      143
#define FRONT_LEDS     71
#define START_BYTES     4
#define END_BYTES      16
#define FRAMESIZE     (START_BYTES + (NUM_LEDS * 4) + END_BYTES)  ///< 592 bytes
#define START_BITS    0xE0

/* ---------------------------------------------------------------------------
 * Compressed format storage sizes (bytes per column)
 * --------------------------------------------------------------------------- */

#define COL_BYTES_1BIT   ((NUM_LEDS + 7) / 8)       ///< 18  — 1 bit per LED, MSB first
#define COL_BYTES_4BIT   ((NUM_LEDS + 1) / 2)       ///< 72  — 2 LEDs per byte, last byte has 1 LED in high nibble
#define COL_BYTES_8BIT    NUM_LEDS                   ///< 143 — 1 byte per LED (332 or palette index)
#define COL_BYTES_565    (NUM_LEDS * 2)              ///< 286 — 2 bytes per LED

#define PALETTE_4BIT_SIZE   16   ///< Entries in a 4-bit palette
#define PALETTE_8BIT_SIZE  256   ///< Entries in an 8-bit palette

// Max Cols with 143 LEDs at 20MHz 900RPM
#define NUM_COLUMNS  140

/* ---------------------------------------------------------------------------
 * Frame builders
 * --------------------------------------------------------------------------- */

/** @brief Fill all LEDs with one solid colour. r+g+b is normalised if > 255. */
void BuildFrame_Solid(uint8_t *frame, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);

/** @brief Light one LED at @p pos, all others off. Used for hardware testing. */
void BuildFrame_Pixel(uint8_t *frame, uint16_t pos, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);

/**
 * @brief Expand a 16-bit 565 column into a full frame.
 * @param col565  Array of NUM_LEDS uint16_t values (RRRRRGGGGGGBBBBB).
 */
void BuildFrame_565(uint8_t *frame, const uint16_t *col565, uint8_t brightness);

/**
 * @brief Expand an 8-bit 332 column into a full frame.
 * @param col332  Array of NUM_LEDS uint8_t values (RRRGGGBB).
 */
void BuildFrame_332(uint8_t *frame, const uint8_t *col332, uint8_t brightness);

/**
 * @brief Expand a 1-bit column into a full frame. On = white, off = black.
 *
 * Vary @p brightness per column to add a free greyscale dimension with no
 * extra storage (e.g. sine wave LUT over NUM_COLUMNS entries).
 *
 * @param col1bit  Packed bits, MSB = LED 0. Must be COL_BYTES_1BIT bytes.
 */
void BuildFrame_1bit(uint8_t *frame, const uint8_t *col1bit, uint8_t brightness);

/**
 * @brief Expand a 4-bit palette column into a full frame.
 *
 * Each byte holds two LED indices: high nibble = even LED, low nibble = odd LED.
 * Palette is generated offline (median-cut / k-means) for the target image.
 *
 * @param col4bit  Packed nibbles, COL_BYTES_4BIT bytes.
 * @param palette  16-entry RGB palette [PALETTE_4BIT_SIZE][3] — {R, G, B} per entry.
 */
void BuildFrame_4bit(uint8_t *frame, const uint8_t *col4bit,
                     const uint8_t palette[PALETTE_4BIT_SIZE][3], uint8_t brightness);

/**
 * @brief Expand an 8-bit palette column into a full frame.
 *
 * Same storage as 332 but palette is chosen per-image — strictly better quality
 * at identical RAM cost. Palette generated offline and stored in ROM.
 *
 * @param col8bit  Array of NUM_LEDS palette indices.
 * @param palette  256-entry RGB palette [PALETTE_8BIT_SIZE][3] — {R, G, B} per entry.
 */
void BuildFrame_8bit(uint8_t *frame, const uint8_t *col8bit,
                     const uint8_t palette[PALETTE_8BIT_SIZE][3], uint8_t brightness);

/**
 * @brief VU meter frame — light LEDs from hub to tip proportional to level.
 *
 * Both faces show the same bar. Green at hub, yellow in the middle, red at tip.
 *
 * @param level  Number of LEDs to light per face (0 = all off, FRONT_LEDS = full).
 */
void BuildFrame_VU(uint8_t *frame, uint16_t level, uint8_t brightness);

#endif /* SK9822_H */
