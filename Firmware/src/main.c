/**
 * @file main.c
 * @brief POV LED globe driver — ROM-only static images (no SD card)
 *
 * Drives a 143-LED SK9822 strip as a persistence-of-vision globe
 * using image data baked into flash. Column timing via Timer4A,
 * frame data shifted out via SSI2 + uDMA (ping-pong, channel 13).
 */

#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "Drivers/inc/PLL.h"
#include "Drivers/inc/SK9822_DMA_SSI.h"
#include "Drivers/inc/SysTickInts.h"
#include "Drivers/inc/SK9822.h"
#include "Drivers/inc/Motor.h"
#include "Drivers/inc/GPIO.h"
#include "Drivers/inc/LCD.h"
#include "Data/map_data.h"
#include "Data/map_palette.h"
#include "Drivers/inc/Timer4A.h"

#include "Data/globe_old/globe_data.h"
#include "Data/globe_old/palette.h"

#include "Drivers/inc/ST7735.h"
#include "main.h"

void DisableInterrupts(void);
void EnableInterrupts(void);
void WaitForInterrupt(void);

static void INITIALIZE(void);
static void LoadDMA(uint8_t *frameDirty, uint8_t *framePending);
static void BuildFrame(uint8_t *frameDirty, uint8_t framePending, uint16_t *col, uint8_t brt);

/* ── Column timing mode (uncomment one) ─────────────────────────────────── */
// #define FIXED_TIMER              /* Constant Timer4A period              */
#define HALL_SYNC                   /* Period tracks motor hall feedback    */

/*
 * FIXED_PERIOD: Timer4A reload value (80 MHz ticks).
 *   152000 → ~526 Hz  (42% duty, ~3.8 rev/s, 140 cols)
 *    76000 → ~1053 Hz (2× motor speed)
 *
 * Hall-sync formula:
 *   colPeriod = revPeriod × MOTOR_GEAR_RATIO / GLOBE_NUM_COLUMNS
 *   140 cols / 6 motor revs per gear rev ≈ 23.3 cols per motor rev
 */
#define FIXED_PERIOD       152000
#define COLS_PER_MOTOR_REV (GLOBE_NUM_COLUMNS / MOTOR_GEAR_RATIO)

/* ── ISR structure (uncomment one) ──────────────────────────────────────── */
// #define ISR_BUILD_IN_TIMER       /* LoadDMA + BuildFrame in Timer4A ISR  */
#define ISR_BUILD_ON_DMA_DONE       /* LoadDMA in timer; BuildFrame on DMA done */

/*
 * ISR_BUILD_IN_TIMER:
 *   Simple. Both functions run in one ISR (~10 µs). Fine up to ~50 kHz.
 *
 * ISR_BUILD_ON_DMA_DONE:
 *   Timer ISR only kicks DMA (~2 µs). BuildFrame runs in SSI2_Handler
 *   after the 592-byte SPI transfer completes (~30 µs at 20 MHz).
 *   Requires DMA_Done_Callback() hook in SK9822_DMA_SSI.c.
 */

/* ── Double buffer ──────────────────────────────────────────────────────── *
 * DMA reads from LIVE while CPU builds the next column into BUFF.
 * Pointers swap after each completed DMA transfer.                         */

static uint8_t  DMA_FRAME[FRAMESIZE];
static uint8_t  BUF_FRAME[FRAMESIZE];
static uint8_t *LIVE = DMA_FRAME;
static uint8_t *BUFF = BUF_FRAME;

extern volatile uint8_t dmaBusy;
extern volatile uint32_t msTick;
extern volatile MotorRampState_t motorRampState;
extern volatile uint8_t motor_state_changed;

static uint8_t  frameDirty   = 0;
static uint8_t  framePending = 1;
static uint16_t col          = 0;
static uint8_t  brightness   = 8;  /* 0–31 */

/* ── Frame pipeline ─────────────────────────────────────────────────────── */

static void LoadDMA(uint8_t *frameDirty, uint8_t *framePending) {
    if (!dmaBusy) {
        if (*frameDirty) {
            uint8_t *tmp = LIVE; LIVE = BUFF; BUFF = tmp;
            *frameDirty  = 0;
            *framePending = 1;
        }
        SK9822_DMA_Start(LIVE, FRAMESIZE);
    }
}

static void BuildFrame(uint8_t *frameDirty, uint8_t framePending, uint16_t *col, uint8_t brt) {
    if (framePending) {
        BuildFrame_8bit(BUFF, map_data[*col], map_palette, brt);
        //BuildFrame_8bit(BUFF, globe[*col], palette, brt);
        //BuildFrame_8bit(BUFF, dot[*col], dot_palette, brt);
        *col = (*col + 1) % GLOBE_NUM_COLUMNS;
        *frameDirty = 1;
    }
}

/* ── Timer4A ISR ────────────────────────────────────────────────────────── */

#ifdef HALL_SYNC
static void HallSync_UpdatePeriod(void) {
    uint32_t rev = Motor_GetRevPeriod();
    if (rev > 0) {
        uint32_t p = (rev * MOTOR_GEAR_RATIO) / GLOBE_NUM_COLUMNS;
        if (p < 5000)   p = 5000;
        if (p > 400000) p = 400000;
        Timer4A_SetPeriod(p);
    }
}
#endif

#ifdef ISR_BUILD_IN_TIMER

static void LEEEED(void) {
#ifdef HALL_SYNC
    HallSync_UpdatePeriod();
#endif
    LoadDMA(&frameDirty, &framePending);
    BuildFrame(&frameDirty, framePending, &col, brightness);
}

#endif /* ISR_BUILD_IN_TIMER */

#ifdef ISR_BUILD_ON_DMA_DONE

void DMA_Done_Callback(void) {
    BuildFrame(&frameDirty, framePending, &col, brightness);
}

static void LEEEED(void) {
#ifdef HALL_SYNC
    HallSync_UpdatePeriod();
#endif
    LoadDMA(&frameDirty, &framePending);
}

#endif /* ISR_BUILD_ON_DMA_DONE */

/* ── Main ───────────────────────────────────────────────────────────────── */

int main01(void) {
    DisableInterrupts();
    INITIALIZE();
    EnableInterrupts();

    while (1) {
        buttonHandler();
        if (motor_state_changed) {
            switch (motorRampState) {
                case MOTOR_IDLE:          ST7735_DrawString2(22, 4, "~", ST7735_YELLOW, 3); break;
                case MOTOR_SPINNING_UP:   ST7735_DrawString2(22, 4, "+", ST7735_GREEN, 3);  break;
                case MOTOR_SPINNING_DOWN: ST7735_DrawString2(22, 4, "-", ST7735_RED, 3);    break;
            }
            motor_state_changed = 0;
        }
    }
}

/* ── Hardware init ──────────────────────────────────────────────────────── */

static void INITIALIZE(void) {
    PLL_Init(Bus80MHz);
    SK9822_20MHZ();
    DMA_Init();
    SK9822_DMA_CH13_Init();
    Motor_Init();
    Motor_Stop();
    PF_Init();
    PB_Init();
    SysTick_Init(80000);                       /* 1 ms tick */
    BuildFrame_Solid(LIVE, 0, 0, 0, 0);
    BuildFrame_Solid(BUFF, 0, 0, 0, 0);
    Timer4A_Init(LEEEED, FIXED_PERIOD, 3);     /* fallback period until hall-sync locks */
    LCD_Menu_Init();
}
