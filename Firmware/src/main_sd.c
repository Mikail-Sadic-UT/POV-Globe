/**
 * @file main_sd.c
 * @brief POV LED globe driver — SD card animation + ROM static image support
 *
 * Drives a 144(-1)-LED SK9822 strip as a persistence-of-vision globe.
 * Supports two display modes:
 *   - 8-bit static images (ROM or SD card, 256-colour palette)
 *   - 4-bit animations   (SD card, 16-colour palette, double-buffered)
 *
 * Column timing is driven by Timer4A, either at a fixed rate or
 * synchronised to motor hall-sensor feedback. Frame data is shifted
 * out via SSI2 + uDMA (ping-pong, channel 13).
 *
 * LCD menu integration: Globe_ToggleMode() and Globe_ChangeContent()
 * are called from LCD.c to switch between content at runtime.
 */

#include <stdint.h>
#include "Drivers/inc/tm4c123gh6pm.h"
#include "Drivers/inc/PLL.h"
#include "Drivers/inc/SK9822_DMA_SSI.h"
#include "Drivers/inc/SysTickInts.h"
#include "Drivers/inc/SK9822.h"
#include "Drivers/inc/Motor.h"
#include "Drivers/inc/GPIO.h"
#include "Drivers/inc/diskio.h"
#include "Drivers/inc/DMA_Common.h"
#include "Drivers/inc/SD_DMA.h"
#include "Drivers/inc/ff.h"
#include "Drivers/inc/LCD.h"
#include "Drivers/inc/ST7735.h"
#include "Drivers/inc/Timer4A.h"
#include "Drivers/inc/Mic.h"
#include "Data/cat_space/cat_space_palette.h"
#include "Data/frog/frog_palette.h"
#include "Data/map_data.h"
#include "Data/map_palette.h"
#include "Data/dermot/dermot_data.h"
#include "Data/dermot/dermot_palette.h"
#include "Data/vote/vote_data.h"
#include "Data/vote/vote_palette.h"
#include "Data/lebron/lebron_data.h"
#include "Data/lebron/lebron_palette.h"
#include "Data/eye/eye_data.h"
#include "Data/eye/eye_palette.h"
#include "Data/baby/baby_palette.h"
#include "Data/boi/boi_palette.h"
#include "main.h"

void DisableInterrupts(void);
void EnableInterrupts(void);
void WaitForInterrupt(void);

static void LoadDMA(void);
static void BuildFrame(void);
static void INITIALIZE(void);
static void SD_LoadFile(const char *filename);
static void SD_LoadNextFrame(void);
static void LEEEED(void);
void Globe_ToggleMode(void);
void Globe_ChangeContent(void);

/* ─────────────────────────────────────────────────────────────────── */
// #define FIXED_TIMER              /* For testing */
#define HALL_SYNC                   /* Period tracks motor hall feedback    */

/*
 * Hall-sync:
 *   colPeriod = revPeriod × MOTOR_GEAR_RATIO / SD_NUM_COLUMNS
 *   140 cols / 6 motor revs per gear rev = ~23.3 cols per motor rev
 */
#define FIXED_PERIOD  152000

/* ─────────────────────────────────────────────────────────────────── */
// #define ISR_BUILD_IN_TIMER       /* LoadDMA + BuildFrame in Timer4A ISR  */
#define ISR_BUILD_ON_DMA_DONE       /* LoadDMA in timer; BuildFrame on DMA done */

/* ─────────────────────────────────────────────────────────────────── */

typedef enum {
    MODE_8BIT_STATIC,       /* Single 8-bit image, loops forever          */
    MODE_4BIT_ANIM,         /* 4-bit animation, double-buffered from SD   */
    MODE_MIC                /* VU meter driven by microphone ADC          */
} DisplayMode_t;

static volatile DisplayMode_t displayMode = MODE_4BIT_ANIM;

/* ── Double buffer (SPI wire format) ────────────────────────────────────── *
 * DMA reads from LIVE while CPU builds the next column into BUFF.
 * Pointers swap after each completed DMA transfer.                         */

static uint8_t  DMA_FRAME[FRAMESIZE];
static uint8_t  BUF_FRAME[FRAMESIZE];
static uint8_t *LIVE = DMA_FRAME;
static uint8_t *BUFF = BUF_FRAME;

extern volatile uint8_t  dmaBusy;
extern volatile uint32_t msTick;

/* ── SD / globe data buffers ────────────────────────────────────────────── *
 * TM4C123 SRAM = 32 KB. Union overlays both layouts on ~20 KB:
 *   8-bit static:  140 × 143 = 20,020 B (one buffer, no animation)
 *   4-bit anim:  2 × 140 × 72 = 20,160 B (double-buffered)               */

#define SD_NUM_COLUMNS  140
#define SD_NUM_LEDS     143 // cuz one of them fkn broke bruh
#define SD_PACKED_COL   ((SD_NUM_LEDS + 1) / 2)  /* 72 bytes per 4-bit column */

static union {  // For being able to store 8bit & 4bit in RAM
    uint8_t static8[SD_NUM_COLUMNS][SD_NUM_LEDS];
    struct {
        uint8_t a[SD_NUM_COLUMNS][SD_PACKED_COL];
        uint8_t b[SD_NUM_COLUMNS][SD_PACKED_COL];
    } anim4;
} sdMem;

static uint8_t (*sdActive)[SD_PACKED_COL]  = sdMem.anim4.a;
static uint8_t (*sdLoading)[SD_PACKED_COL] = sdMem.anim4.b;

static uint16_t        numColumns      = SD_NUM_COLUMNS;
static uint16_t        totalFrames     = 0;
static uint16_t        currentFrameIdx = 0;
static volatile uint8_t sdReady        = 0;  /* 1 = sdLoading buffer complete */

static FATFS  g_sFatFs;
static FIL    g_File;

static const uint8_t (*activePalette)[3] = cat_space_palette;

extern volatile MotorRampState_t motorRampState;
extern volatile uint8_t motor_state_changed;

static uint8_t  frameDirty   = 0;
static uint8_t  framePending = 1;
static uint16_t col          = 0;
static uint8_t  brightness   = 16;  /* 0–31 (31 is ALOT) */

/* ─────────────────────────────────────────────────────────────────── */

typedef enum { SRC_ROM, SRC_SD } DataSource_t;

static volatile DataSource_t dataSource = SRC_SD;
static const uint8_t (*romData)[NUM_LEDS] = ((void *)0);
static uint16_t romNumColumns = 0;

/* ─────────────────────────────────────────────────────────────────── */

/** Load 8-bit static image from SD into sdMem.static8 */
void SetMode_8bit(const char *filename, const uint8_t palette[][3]) {
    DisableInterrupts();
    displayMode   = MODE_8BIT_STATIC;
    dataSource    = SRC_SD;
    activePalette = palette;
    col = 0; frameDirty = 0; framePending = 1; sdReady = 0;
    EnableInterrupts();
    SD_LoadFile(filename);
}

/** Load 4-bit animation from SD, double-buffered */
void SetMode_4bit(const char *filename, const uint8_t palette[][3]) {
    DisableInterrupts();
    displayMode   = MODE_4BIT_ANIM;
    dataSource    = SRC_SD;
    activePalette = palette;
    sdActive  = sdMem.anim4.a;
    sdLoading = sdMem.anim4.b;
    col = 0; frameDirty = 0; framePending = 1; sdReady = 0;
    EnableInterrupts();
    SD_LoadFile(filename);
}

/** Display a ROM8-bit image */
void SetMode_ROM(const uint8_t data[][NUM_LEDS], const uint8_t palette[][3], uint16_t cols) {
    DisableInterrupts();
    displayMode   = MODE_8BIT_STATIC;
    dataSource    = SRC_ROM;
    activePalette = palette;
    romData       = data;
    romNumColumns = cols;
    numColumns    = cols;
    col = 0; frameDirty = 0; framePending = 1; sdReady = 0;
    EnableInterrupts();
}

/** Switch to mic mode (Yell into mic to light up globe mode) */
void SetMode_Mic(void) {
    DisableInterrupts();
    displayMode  = MODE_MIC;
    col = 0; frameDirty = 0; framePending = 1; sdReady = 0;
    EnableInterrupts();
}

/* ── Content table ──────────────────────────────────────────────────────── *
 * LCD "Mode" button toggles picture <-> animation.
 * LCD "Change" button cycles within the current category.                  */

// Potentially adding 1-bit mode for long form videos (Bad Apple??)
typedef enum {
    CONTENT_ROM_8BIT,
    CONTENT_SD_8BIT,
    CONTENT_SD_4BIT_ANIM
} ContentType_t;

typedef struct {
    const char       *name;
    ContentType_t     type;
    const uint8_t   (*romData)[NUM_LEDS];   /* ROM images only  */
    const uint8_t   (*palette)[3];
    uint16_t          romCols;              /* ROM images only  */
    const char       *sdFilename;           /* SD images only   */
} ContentEntry_t;

/*
 * When adding stuff into /Data folder, add in here to be able to switch to it
 */
static const ContentEntry_t pictures[] = {
    { "Earth",  CONTENT_ROM_8BIT, map_data,    map_palette,    GLOBE_NUM_COLUMNS, ((void *)0) },
    { "Dermot", CONTENT_ROM_8BIT, dermot_data, dermot_palette, GLOBE_NUM_COLUMNS, ((void *)0) },
    { "Vote",   CONTENT_ROM_8BIT, vote_data,   vote_palette,   GLOBE_NUM_COLUMNS, ((void *)0) },
    { "eye",    CONTENT_ROM_8BIT, eye_data,    eye_palette,    GLOBE_NUM_COLUMNS, ((void *)0) },
};
#define NUM_PICTURES (sizeof(pictures) / sizeof(pictures[0]))

/*
 * When adding stuff into SD card, add in here to be able to switch to it
 */
static const ContentEntry_t animations[] = {
    { "Cat",       CONTENT_SD_4BIT_ANIM, ((void *)0), cat_space_palette, 0, "cat.bin" },
    { "Frog",      CONTENT_SD_4BIT_ANIM, ((void *)0), frog_palette,      0, "fro.bin" },
    { "baby",      CONTENT_SD_4BIT_ANIM, ((void *)0), baby_palette,      0, "bby.bin" },
    { "boi",       CONTENT_SD_4BIT_ANIM, ((void *)0), boi_palette,       0, "boi.bin" },
};
#define NUM_ANIMATIONS (sizeof(animations) / sizeof(animations[0]))

static uint8_t pictureIdx   = 0;
static uint8_t animationIdx = 0;

static void Content_Load(const ContentEntry_t *e) {
    switch (e->type) {
        case CONTENT_ROM_8BIT:     SetMode_ROM(e->romData, e->palette, e->romCols); break;
        case CONTENT_SD_8BIT:      SetMode_8bit(e->sdFilename, e->palette);         break;
        case CONTENT_SD_4BIT_ANIM: SetMode_4bit(e->sdFilename, e->palette);         break;
    }
}

/** Cycle display mode: picture -> animation -> mic -> picture.  */
void Globe_ToggleMode(void) {
    extern volatile uint8_t ledModeAnim;

    if (displayMode == MODE_MIC) {
        ledModeAnim = 0;
        Content_Load(&pictures[pictureIdx]);
    } else if (!ledModeAnim) {
        ledModeAnim = 1;
        Content_Load(&animations[animationIdx]);
    } else {
        ledModeAnim = 2;
        SetMode_Mic();
    }
}

/** Cycle to next entry in category. */
void Globe_ChangeContent(void) {
    extern volatile uint8_t ledModeAnim;
    if (ledModeAnim) {
        animationIdx = (animationIdx + 1) % NUM_ANIMATIONS;
        Content_Load(&animations[animationIdx]);
    } else {
        pictureIdx = (pictureIdx + 1) % NUM_PICTURES;
        Content_Load(&pictures[pictureIdx]);
    }
}

/* ─────────────────────────────────────────────────────────────────── */

/** Swap buffers if a new frame is ready, then start DMA. */
static void LoadDMA(void) {
    if (!dmaBusy) {
        if (frameDirty) {
            uint8_t *tmp = LIVE; LIVE = BUFF; BUFF = tmp;
            frameDirty = 0;
            framePending = 1;
        }
        SK9822_DMA_Start(LIVE, FRAMESIZE);
    }
}

volatile uint16_t raw;

/** Build the next column into BUFF from the active data source. */
static void BuildFrame(void) {
    if (!framePending) return;

    if (displayMode == MODE_MIC) {
        raw = Mic_Read();
        uint16_t level = ((raw * FRONT_LEDS) / 4096) - 33;     /* scale to LED */
        BuildFrame_VU(BUFF, level, brightness);
    } else if (displayMode == MODE_8BIT_STATIC) {
        if (dataSource == SRC_ROM && romData != ((void *)0))
             BuildFrame_8bit(BUFF, romData[col], activePalette, brightness);
        else BuildFrame_8bit(BUFF, sdMem.static8[col], activePalette, brightness);
    } else   BuildFrame_4bit(BUFF, sdActive[col], activePalette, brightness);

    col = (col + 1) % numColumns;

    /* Swap animation buffers at rotation boundary */
    if (displayMode == MODE_4BIT_ANIM && col == 0 && sdReady) {
        uint8_t (*tmp)[SD_PACKED_COL] = sdActive;
        sdActive = sdLoading; sdLoading = tmp;
        sdReady = 0;
    }
    frameDirty = 1;
}

/* ─────────────────────────────────────────────────────────────────── */

#ifdef HALL_SYNC
static void HallSync_UpdatePeriod(void) {
    uint32_t rev = Motor_GetRevPeriod();
    if (rev > 0) {
        uint32_t p = (rev * MOTOR_GEAR_RATIO) / SD_NUM_COLUMNS;
        if (p < 5000)   p = 5000;    /* clamp: ~16 kHz max  */
        if (p > 400000) p = 400000;  /* clamp: ~200 Hz min  */
        Timer4A_SetPeriod(p);
    }
}
#endif

#ifdef ISR_BUILD_IN_TIMER

static void LEEEED(void) {
#ifdef HALL_SYNC
    HallSync_UpdatePeriod();
#endif
    LoadDMA();
    BuildFrame();
}

#endif /* ISR_BUILD_IN_TIMER */

#ifdef ISR_BUILD_ON_DMA_DONE

void DMA_Done_Callback(void) { BuildFrame(); }

static void LEEEED(void) {
#ifdef HALL_SYNC
    HallSync_UpdatePeriod();
#endif
    LoadDMA();
}

#endif /* ISR_BUILD_ON_DMA_DONE */

/* ─────────────────────────────────────────────────────────────────── */

int main(void) {
    DisableInterrupts();
    INITIALIZE();
    EnableInterrupts();

    SetMode_ROM(map_data, map_palette, GLOBE_NUM_COLUMNS);
    Timer4A_Init(LEEEED, FIXED_PERIOD, 3);  /* starting period until hall-sync locks */

    while (1) {
        if (displayMode == MODE_4BIT_ANIM && !sdReady) SD_LoadNextFrame();

        buttonHandler();    // mmm butts

        if (motor_state_changed) {          // Always display motor status
            switch (motorRampState) {
                case MOTOR_IDLE:          ST7735_DrawString2(22, 4, "~", ST7735_YELLOW, 3); break;
                case MOTOR_SPINNING_UP:   ST7735_DrawString2(22, 4, "+", ST7735_GREEN, 3);  break;
                case MOTOR_SPINNING_DOWN: ST7735_DrawString2(22, 4, "-", ST7735_RED, 3);    break;
            }
            motor_state_changed = 0;
        }
    }
}

/* ─────────────────────────────────────────────────────────────────── */

/**
 * Mount SD, open file, load first frame.
 * 8-bit: reads full frame into sdMem.static8, closes file.
 * 4-bit: reads first frame into sdLoading, keeps file open for streaming.
 *
 * SD card bs, most driver code given by Valvanoware.
 */
static void SD_LoadFile(const char *filename) {
    FRESULT fr;
    UINT    br;

    fr = f_mount(&g_sFatFs, "", 0);
    if (fr != FR_OK) { while (1) {} }

    fr = f_open(&g_File, filename, FA_READ);
    if (fr != FR_OK) { while (1) {} }

    if (displayMode == MODE_8BIT_STATIC) {
        uint32_t frameBytes = (uint32_t)SD_NUM_COLUMNS * SD_NUM_LEDS;
        fr = f_read(&g_File, sdMem.static8, frameBytes, &br);
        if (fr != FR_OK || br < SD_NUM_LEDS) { while (1) {} }
        numColumns  = (uint16_t)(br / SD_NUM_LEDS);
        totalFrames = 1;
        f_close(&g_File);
    } else {
        uint32_t frameBytes = (uint32_t)SD_NUM_COLUMNS * SD_PACKED_COL;
        DWORD fileSize = f_size(&g_File);
        totalFrames = (uint16_t)(fileSize / frameBytes);
        if (totalFrames == 0) { while (1) {} }
        fr = f_read(&g_File, sdLoading, frameBytes, &br);
        if (fr != FR_OK || br < SD_PACKED_COL) { while (1) {} }
        numColumns      = (uint16_t)(br / SD_PACKED_COL);
        currentFrameIdx = 0;
        sdReady         = 1;
    }
}

/** Read next animation frame into sdLoading. Wraps at end of file. */
static void SD_LoadNextFrame(void) {
    FRESULT fr;
    UINT    br;
    uint32_t frameBytes = (uint32_t)SD_NUM_COLUMNS * SD_PACKED_COL;

    currentFrameIdx = (currentFrameIdx + 1) % totalFrames;
    if (currentFrameIdx == 0) f_lseek(&g_File, 0);

    fr = f_read(&g_File, sdLoading, frameBytes, &br);
    if (fr != FR_OK || br < SD_PACKED_COL) { while (1) {} }

    numColumns = (uint16_t)(br / SD_PACKED_COL);
    sdReady = 1;
}

/* ─────────────────────────────────────────────────────────────────── */

static void INITIALIZE(void) {
    PLL_Init(Bus80MHz);
    SK9822_20MHZ();
    DMA_Init();
    SK9822_DMA_CH13_Init();
    SD_DMA_CH10_Init();
    Motor_Init();
    Motor_Stop();
    PF_Init();
    PB_Init();
    SysTick_Init(80000);    /* 1 ms tick */
    Mic_Init();
    BuildFrame_Solid(LIVE, 0, 0, 0, 0);
    BuildFrame_Solid(BUFF, 0, 0, 0, 0);
    LCD_Menu_Init();
}
