/**
 * @file SD_Card.c
 * @brief SD card read test — load a .bin file into a RAM buffer and verify.
 */

#include <stdio.h>
#include <stdint.h>
#include "../inc/ST7735.h"
#include "../inc/PLL.h"
#include "../inc/tm4c123gh6pm.h"
#include "../inc/Timer0A.h"
#include "../inc/Timer2A.h"
#include "../inc/Timer3A.h"
#include "../Drivers/inc/diskio.h"
#include "../Drivers/inc/DMA_Common.h"
#include "../Drivers/inc/SK9822_DMA_SSI.h"
#include "../Drivers/inc/SD_DMA.h"
#include "../Drivers/inc/ff.h"

void DisableInterrupts(void);
void EnableInterrupts(void);
void WaitForInterrupt(void);

static FATFS g_sFatFs;
FIL     Handle;
FRESULT MountFresult;
FRESULT Fresult;

static uint8_t ledArray[140][144];

const char in_Filename[] = "cat.bin";

int main420(void) {
    DisableInterrupts();
    UINT successfulreads;
    PLL_Init(Bus80MHz);
    ST7735_InitR(INITR_REDTAB);
    ST7735_FillScreen(0);
    DMA_Init();
    SK9822_DMA_CH13_Init();
    SD_DMA_CH10_Init();
    EnableInterrupts();

    MountFresult = f_mount(&g_sFatFs, "", 0);
    if (MountFresult) {
        ST7735_DrawString(0, 0, "f_mount error", ST7735_Color565(0, 0, 255));
        while (1) {}
    }

    Fresult = f_open(&Handle, in_Filename, FA_READ);
    if (Fresult == FR_OK) {
        Fresult = f_read(&Handle, ledArray, sizeof(ledArray), &successfulreads);
        f_close(&Handle);
    }

    while (1) {}
}
