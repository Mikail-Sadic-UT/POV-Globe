#include <stdio.h>
#include <stdint.h>
#include "../inc/ST7735.h"
#include "../inc/PLL.h"
#include "../inc/tm4c123gh6pm.h"
#include "../inc/tm4c123gh6pm.h"
#include "../inc/Timer0A.h"
#include "../inc/Timer2A.h"
#include "../inc/Timer3A.h"
#include "../Drivers/inc/diskio.h"
#include "../Drivers/inc/DMA_Common.h"
#include "../Drivers/inc/SK9822_DMA_SSI.h"
#include "../Drivers/inc/SD_DMA.h"
#include "../Data/map_data.h"

#include "../Drivers/inc/ff.h"

//DID NOT FINISH BC NOT NEEDED

// ---------- Prototypes   -------------------------
void DisableInterrupts(void); 
void EnableInterrupts(void);  
void WaitForInterrupt(void);

static FATFS g_sFatFs;
FIL Handle;
FRESULT MountFresult;
FRESULT Fresult;

extern const uint8_t globe[GLOBE_NUM_COLUMNS][GLOBE_NUM_LEDS];

const char write_Filename[] = "glo.txt";   // 8 characters or fewer

int main88(void) {

    DisableInterrupts();
    UINT successfulwrites;
    PLL_Init(Bus80MHz);    // 80 MHz
    ST7735_InitR(INITR_REDTAB);
    ST7735_FillScreen(0);                 // set screen to black
    DMA_Init();                           // shared uDMA controller
    SK9822_DMA_CH13_Init();               // CH13 — SSI2 TX (LEDs)
    SD_DMA_CH10_Init();                   // CH10 — SSI0 RX (SD card)
    EnableInterrupts();

    //Attempt to mount the card
    MountFresult = f_mount(&g_sFatFs, "", 0);
    if(MountFresult){
      ST7735_DrawString(0, 0, "f_mount error", ST7735_Color565(0, 0, 255));
      while(1){};
    }

    Fresult = f_open(&Handle, write_Filename, FA_WRITE);
    if(Fresult == FR_OK) {
        while(1){
            Fresult = f_write(&Handle, &globe, sizeof(globe), &successfulwrites);
            if(Fresult != FR_OK || successfulwrites == 0) break;
        }
    }
    Fresult = f_close(&Handle);

    while(1){}

}
