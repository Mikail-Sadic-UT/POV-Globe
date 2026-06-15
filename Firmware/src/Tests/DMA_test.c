/**
 * @file DMA_test.c
 * @brief SK9822 LED strip driver — TM4C123GH6PM @ 80MHz
 *
 * Double-buffered DMA pipeline:
 *   LIVE  → currently being transmitted by uDMA over SSI0 at 20MHz
 *   BUFF  → being written by CPU
 *   Swap occurs once DMA completes (dmaBusy = 0) and BUFF has new data.
 *
 * Frame rate: ~4,193 fps theoretical (596 bytes @ 20MHz = 238µs/frame)
 */

#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/PLL.h"
#include "../Drivers/inc/GPIO.h"
#include "../Drivers/inc/SK9822_DMA_SSI.h"
#include "../Drivers/inc/DMA_Common.h"
#include "../Drivers/inc/SK9822.h"
#include "../Drivers/inc/Motor.h"
#include "main.h"

#include "../Data/globe_old/globe_data.h"
#include "../Data/globe_old/palette.h"

void EnableInterrupts(void);
void DisableInterrupts(void);

/* Double buffer — DMA reads LIVE, CPU writes BUFF.
 * Pointers are swapped after each completed transfer. */
static uint8_t  DMA_FRAME[FRAMESIZE];
static uint8_t  BUF_FRAME[FRAMESIZE];
static uint8_t *LIVE = DMA_FRAME;   ///< Currently transmitting
static uint8_t *BUFF = BUF_FRAME;   ///< Currently being built

extern volatile uint8_t dmaBusy;    ///< Set by SK9822_DMA_Start, cleared by SSI0_Handler

/** @brief System initialisation — clock, SPI, uDMA. */
static void INITIALIZE(void){
    PLL_Init(Bus80MHz);
    SK9822_20MHZ();
    DMA_Init();
    SK9822_DMA_CH13_Init();
}

int main2(void){
    DisableInterrupts();
    INITIALIZE();
    BuildFrame_Solid(LIVE, 0, 0, 0, 0);
    BuildFrame_Solid(BUFF, 0, 0, 0, 0);
    LED_Init();
    EnableInterrupts();

    uint8_t  frameDirty   = 0;
    uint8_t  framePending = 1;           /* build first frame immediately */
    uint16_t col          = 0;           /* current angular column index */
    uint8_t  brightness   = 1;           /* global brightness (0-31) */
    uint32_t lastStep     = 0;           // For slowing down

    while(1){
        if(!(delay % 1000000)) LED_HBT();

        if(!dmaBusy){
            if(frameDirty){
                uint8_t *tmp = LIVE;
                LIVE = BUFF;
                BUFF = tmp;
                frameDirty  = 0;
                framePending = 1;       /* BUFF is free — load next column */
            }
            SK9822_DMA_Start(LIVE, FRAMESIZE);
        }

        /* Globe column playback — advance one column per completed frame.
         * BuildFrame_8bit expands palette indices to full BGR wire format. */
        if(framePending){
           if((delay - lastStep) >= 8000){
                BuildFrame_8bit(BUFF, globe[col], palette, brightness);
                col = (col + 1) % GLOBE_NUM_COLUMNS;
                frameDirty  = 1;
                lastStep = delay;
            }
        }
        delay++;
    }
}

int main8(void){
    DisableInterrupts();
    INITIALIZE();
    BuildFrame_Solid(LIVE, 0, 0, 0, 0);
    BuildFrame_Solid(BUFF, 0, 0, 0, 0);
    LED_Init();
    EnableInterrupts();
   
    uint8_t r = 25, g = 45, b = 90, brt = 1, phase = 0; // Init RGB vals

    uint8_t frameDirty = 0;   /* set when BUFF has new data ready to promote */

    uint16_t pixelPos = 0;
    uint8_t  pixelPending = 1;   // For pixel demo
    uint32_t lastStep = 0;

    while(1){
        if(!(delay % 1000000)) LED_HBT();   // LaunchPad LEDs cycle if alive

        if(!dmaBusy){               /* DMA pipeline — swap and restart as soon as previous transfer completes */
            if(frameDirty){ 
                uint8_t *tmp = LIVE;
                LIVE = BUFF;
                BUFF = tmp;
                frameDirty = 0;
                pixelPending = 1;
            }
            SK9822_DMA_Start(LIVE, FRAMESIZE);
        }

        /* RGB wheel  */
        // if(!(delay % 5000)){
        //     switch(phase){
        //         case 0: g++; if(g == 255) phase = 1; break;
        //         case 1: r--; if(r == 0)   phase = 2; break;
        //         case 2: b++; if(b == 255) phase = 3; break;
        //         case 3: g--; if(g == 0)   phase = 4; break;
        //         case 4: r++; if(r == 255) phase = 5; break;
        //         case 5: b--; if(b == 0)   phase = 0; break;
        //     }
        //     BuildFrame_Solid(BUFF, r, g, b, brt);
        //     frameDirty = 1;
        // }

        /* Running Pixel */
        if(pixelPending){
            if((delay - lastStep) >= 8000){
                pixelPos++;
                if(pixelPos >= NUM_LEDS) pixelPos = 0;
                BuildFrame_Pixel(BUFF, pixelPos, 16, 16, 16, 2);
                //BuildFrame_Pixel(BUFF, pixelPos, 255, 255, 255, 31);
                frameDirty = 1;
                pixelPending = 0;
                lastStep = delay;
            }
        }
        delay++;
    }
}
