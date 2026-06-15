/**
 * @file SK9822_DMA_SSI.h
 * @brief SK9822 LED driver — SSI2 + uDMA ping-pong on TM4C123GH6PM
 *
 * SSI2 (PB4=SCLK, PB7=TX) drives the SK9822 data line at a configurable rate.
 * uDMA channel 13 transfers frame data in ping-pong mode — the buffer is split
 * in two and the halves alternate continuously, keeping the TX FIFO fed with no
 * re-arbitration gap. Completion signals via SSI2 interrupt (IRQ 57).
 *
 * Usage:
 *   SK9822_20MHZ() // SK9822_SPI_Init(CPSDVSR_20MHZ, SCR_20MHZ);
 *   SK9822_DMA_CH13_Init();
 *
 *   // In main loop:
 *   if(!dmaBusy){
 *       // swap LIVE/BUFF, build next frame into BUFF
 *       SK9822_DMA_Start(LIVE, FRAMESIZE);
 *   }
 *
 * Frame format (SK9822 datasheet §5):
 *   Start : 4 x 0x00
 *   LEDs  : [0xE0 | brightness(5b)] [B] [G] [R]  x NUM_LEDS
 *   End   : >= ceil(NUM_LEDS/2) bytes of 0xFF to latch the last LED
 */

#ifndef SK9822_DMA_SSI_H
#define SK9822_DMA_SSI_H

#include <stdint.h>
#include "DMA_Common.h"

/* ---------------------------------------------------------------------------
 * SSI clock presets — SPI rate = 80MHz / (CPSDVSR * (1 + SCR))
 * --------------------------------------------------------------------------- */
#define CPSDVSR_1MHZ    80
#define SCR_1MHZ         0
#define SK9822_1MHZ()    SK9822_SPI_Init(CPSDVSR_1MHZ,  SCR_1MHZ)

#define CPSDVSR_2MHZ    40
#define SCR_2MHZ         0
#define SK9822_2MHZ()    SK9822_SPI_Init(CPSDVSR_2MHZ,  SCR_2MHZ)

#define CPSDVSR_4MHZ    20
#define SCR_4MHZ         0
#define SK9822_4MHZ()    SK9822_SPI_Init(CPSDVSR_4MHZ,  SCR_4MHZ)

#define CPSDVSR_5MHZ    16
#define SCR_5MHZ         0
#define SK9822_5MHZ()    SK9822_SPI_Init(CPSDVSR_5MHZ,  SCR_5MHZ)

#define CPSDVSR_6MHZ    12
#define SCR_6MHZ         0
#define SK9822_6MHZ()    SK9822_SPI_Init(CPSDVSR_6MHZ,  SCR_6MHZ)

#define CPSDVSR_8MHZ    10
#define SCR_8MHZ         0
#define SK9822_8MHZ()    SK9822_SPI_Init(CPSDVSR_8MHZ,  SCR_8MHZ)

#define CPSDVSR_10MHZ    8
#define SCR_10MHZ        0
#define SK9822_10MHZ()   SK9822_SPI_Init(CPSDVSR_10MHZ,  SCR_10MHZ)

#define CPSDVSR_13MHZ    6
#define SCR_13MHZ        0
#define SK9822_13MHZ()   SK9822_SPI_Init(CPSDVSR_13MHZ,  SCR_13MHZ)

#define CPSDVSR_20MHZ    4
#define SCR_20MHZ        0
#define SK9822_20MHZ()   SK9822_SPI_Init(CPSDVSR_20MHZ,  SCR_20MHZ)

#define CPSDVSR_40MHZ    2
#define SCR_40MHZ        0
#define SK9822_40MHZ()   SK9822_SPI_Init(CPSDVSR_40MHZ,  SCR_40MHZ)

/* ---------------------------------------------------------------------------
 * SSI register field masks (TM4C123 datasheet §15)
 * --------------------------------------------------------------------------- */
#define SSI_CR0_DSS_8       0x00000007
#define SSI_CR0_DSS_M       0x0000000F
#define SSI_CR0_SCR_M       0x0000FF00
#define SSI_CR0_FRF_MOTO    0x00000000
#define SSI_CR1_SSE         0x00000002
#define SSI_CR1_MS          0x00000004
#define SSI_SR_TNF          0x00000002
#define SSI_SR_RNE          0x00000004
#define SSI_SR_BSY          0x00000010
#define SSI_CPSR_CPSDVSR_M  0x000000FF
#define SSI_DMACTL_TXDMAE   0x00000002
#define SSI_DMACTL_RXDMAE   0x00000001

#define SSI2_CLK  0x04
#define PORTB_CLK 0x02

#define PB4_7             0x90
#define PB4_7_PCTL_SET    0x20020000
#define PB4_7_PCTL_CLR    0xF00F0000

#define SSI2_IRQ_PRI_MSK  0x0000E000
#define SSI2_IRQ_PRI      (2 << 13)
#define SSI2_IRQ_EN       (1 << 25)

/* ---------------------------------------------------------------------------
 * uDMA channel 13 — SSI2 TX (LEDs)
 * --------------------------------------------------------------------------- */
#define CH13        (13 * 4)
#define CH13ALT     (13 * 4 + 128)
#define DMA_CH_NUM  13U
#define DMA_CH_BIT  (1U << DMA_CH_NUM)

/* ---------------------------------------------------------------------------
 * API
 * --------------------------------------------------------------------------- */

void SK9822_SPI_Init(uint8_t cpsdvsr, uint8_t scr);

/** @brief Init uDMA channel 13 for SSI2 TX. Call DMA_Init() first. */
void SK9822_DMA_CH13_Init(void);

int8_t SK9822_DMA_Start(const uint8_t *buf, uint32_t len);

int SK9822_DMA_IsBusy(void);

#endif /* SK9822_DMA_SSI_H */
