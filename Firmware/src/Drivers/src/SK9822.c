/**
 * @file SK9822.c
 * @brief SK9822 frame buffer builders — expand compressed data to SPI.
 *
 * Wire format per LED: [0xE0|brt(5b)] [B] [G] [R]
 * Frame layout: 4×0x00 start | NUM_LEDS×4 bytes | END_BYTES×0xFF end
 */

#include <stdint.h>
#include "../inc/SK9822.h"

/* ─────────────────────────────────────────────────────────────────── */

/* Write one LED frame (4 bytes) into frame[] at position idx */
#define WRITE_LED(frame, idx, global, r, g, b) \
    do { (frame)[(idx)++] = (global);          \
         (frame)[(idx)++] = (b);               \
         (frame)[(idx)++] = (g);               \
         (frame)[(idx)++] = (r); } while(0)

/* ─────────────────────────────────────────────────────────────────── */

/* Scale RGB proportionally so r+g+b <= 255. */
static inline void NormalizeRGB(uint8_t *r, uint8_t *g, uint8_t *b) {
    uint16_t sum = (uint16_t)*r + *g + *b;
    if (sum > 255) {
        *r = (uint8_t)((*r * 255) / sum);
        *g = (uint8_t)((*g * 255) / sum);
        *b = (uint8_t)((*b * 255) / sum);
    }
}

static inline void expand565(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = (c >> 8) & 0xF8;
    *g = (c >> 3) & 0xFC;
    *b = (c << 3) & 0xF8;
}

static inline void expand332(uint8_t c, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r =  (c & 0xE0);
    *g =  (c & 0x1C) << 3;
    *b =  (c & 0x03) << 6;
}

static inline void expand1bit(const uint8_t *col, int i, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint8_t on = (col[i >> 3] >> (7 - (i & 7))) & 1;
    *r = *g = *b = on ? 255 : 0;
}

static inline void StartFrame(uint8_t *frame, uint32_t *idx) {
    for (int i = 0; i < START_BYTES; i++) frame[(*idx)++] = 0x00;
}

static inline void EndFrame(uint8_t *frame, uint32_t *idx) {
    for (int i = 0; i < END_BYTES; i++) frame[(*idx)++] = 0xFF;
}

/* ─────────────────────────────────────────────────────────────────── */

void BuildFrame_Solid(uint8_t *frame, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    NormalizeRGB(&r, &g, &b);
    uint32_t idx = 0;
    uint8_t global = START_BITS | (brightness & 0x1F);
    StartFrame(frame, &idx);
    for (int i = 0; i < NUM_LEDS; i++) WRITE_LED(frame, idx, global, r, g, b);
    EndFrame(frame, &idx);
}

void BuildFrame_Pixel(uint8_t *frame, uint16_t pos, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    NormalizeRGB(&r, &g, &b);
    uint32_t idx = 0;
    uint8_t global = START_BITS | (brightness & 0x1F);
    StartFrame(frame, &idx);
    for (int i = 0; i < NUM_LEDS; i++) {
        WRITE_LED(frame, idx,
                  (i == pos) ? global : 0xE0,
                  (i == pos) ? r : 0,
                  (i == pos) ? g : 0,
                  (i == pos) ? b : 0);
    }
    EndFrame(frame, &idx);
}

void BuildFrame_565(uint8_t *frame, const uint16_t *col565, uint8_t brightness) {
    uint32_t idx = 0;
    uint8_t r, g, b;
    uint8_t global = START_BITS | (brightness & 0x1F);
    StartFrame(frame, &idx);
    for (int i = 0; i < NUM_LEDS; i++) {
        expand565(col565[i], &r, &g, &b);
        WRITE_LED(frame, idx, global, r, g, b);
    }
    EndFrame(frame, &idx);
}

void BuildFrame_332(uint8_t *frame, const uint8_t *col332, uint8_t brightness) {
    uint32_t idx = 0;
    uint8_t r, g, b;
    uint8_t global = START_BITS | (brightness & 0x1F);
    StartFrame(frame, &idx);
    for (int i = 0; i < NUM_LEDS; i++) {
        expand332(col332[i], &r, &g, &b);
        WRITE_LED(frame, idx, global, r, g, b);
    }
    EndFrame(frame, &idx);
}

void BuildFrame_1bit(uint8_t *frame, const uint8_t *col1bit, uint8_t brightness) {
    uint32_t idx = 0;
    uint8_t r, g, b;
    uint8_t global = START_BITS | (brightness & 0x1F);
    StartFrame(frame, &idx);
    for (int i = 0; i < NUM_LEDS; i++) {
        expand1bit(col1bit, i, &r, &g, &b);
        WRITE_LED(frame, idx, global, r, g, b);
    }
    EndFrame(frame, &idx);
}

void BuildFrame_4bit(uint8_t *frame, const uint8_t *col4bit,
                     const uint8_t palette[PALETTE_4BIT_SIZE][3], uint8_t brightness) {
    uint32_t idx = 0;
    uint8_t global = START_BITS | (brightness & 0x1F);
    StartFrame(frame, &idx);
    for (int i = 0; i < NUM_LEDS; i++) {
        /* High nibble = even LED, low nibble = odd LED.
         * For odd NUM_LEDS the last byte's low nibble is unused. */
        uint8_t entry = (i & 1) ? (col4bit[i >> 1] & 0x0F) : (col4bit[i >> 1] >> 4);
        WRITE_LED(frame, idx, global, palette[entry][0], palette[entry][1], palette[entry][2]);
    }
    EndFrame(frame, &idx);
}

void BuildFrame_8bit(uint8_t *frame, const uint8_t *col8bit,
                     const uint8_t palette[PALETTE_8BIT_SIZE][3], uint8_t brightness) {
    uint32_t idx = 0;
    uint8_t global = START_BITS | (brightness & 0x1F);
    StartFrame(frame, &idx);
    for (int i = 0; i < NUM_LEDS; i++) {
        WRITE_LED(frame, idx, global,
                  palette[col8bit[i]][0],
                  palette[col8bit[i]][1],
                  palette[col8bit[i]][2]);
    }
    EndFrame(frame, &idx);
}

/* ─────────────────────────────────────────────────────────────────── */

void BuildFrame_VU(uint8_t *frame, uint16_t level, uint8_t brightness) {
    uint32_t idx = 0;
    uint8_t global = START_BITS | (brightness & 0x1F);
    uint16_t back_leds = NUM_LEDS - FRONT_LEDS;

    if (level > FRONT_LEDS) level = FRONT_LEDS;

    StartFrame(frame, &idx);

    for (int i = 0; i < FRONT_LEDS; i++) {
        if (i < level) {
            uint8_t frac = (uint8_t)((uint32_t)i * 255 / FRONT_LEDS);
            WRITE_LED(frame, idx, global, frac, 255 - frac, 0);
        } else WRITE_LED(frame, idx, 0xE0, 0, 0, 0);      
    }

    for (int i = 0; i < back_leds; i++) {
        int hubDist = back_leds - 1 - i;
        if (hubDist == back_leds - 1) WRITE_LED(frame, idx, 0xE0, 0, 0, 0);
        else if (hubDist < level) {
            uint8_t frac = (uint8_t)((uint32_t)hubDist * 255 / FRONT_LEDS);
            WRITE_LED(frame, idx, global, frac, 255 - frac, 0);
        } else WRITE_LED(frame, idx, 0xE0, 0, 0, 0);
        
    }
    EndFrame(frame, &idx);
}
