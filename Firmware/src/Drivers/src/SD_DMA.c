/**
 * @file SD_DMA.c
 * @brief SD card DMA read — SSI0 RX via uDMA channel 10, basic mode.
 *
 * CPU feeds 0xFF bytes into SSI0 TX to generate the SPI clock.
 * uDMA captures received bytes from SSI0 RX FIFO into the destination buffer.
 * SD_DMA_Read_Sector() blocks until the transfer completes.
 */

#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/SD_DMA.h"

#define SSI_SR_TNF  0x00000002
#define SSI_SR_RNE  0x00000004
#define SSI_SR_BSY  0x00000010

void SD_DMA_CH10_Init(void) {
    UDMA_CHMAP1_R &= ~0x00000F00;       /* CH10 = SSI0 RX (encoding 0) */
    UDMA_ENACLR_R      = DMA_CH10_BIT;
    UDMA_PRIOCLR_R     = DMA_CH10_BIT;
    UDMA_ALTCLR_R      = DMA_CH10_BIT;
    UDMA_USEBURSTSET_R = DMA_CH10_BIT;  /* burst-only */
    UDMA_REQMASKSET_R  = DMA_CH10_BIT;  /* masked until needed */
}

void SD_DMA_Read_Sector(uint8_t *buff, uint32_t count) {
    /* Flush stale RX bytes */
    while (SSI0_SR_R & SSI_SR_RNE) { (void)SSI0_DR_R; }

    UDMA_REQMASKSET_R = DMA_CH10_BIT;
    UDMA_ENACLR_R     = DMA_CH10_BIT;
    UDMA_ALTCLR_R     = DMA_CH10_BIT;

    /* CH10 primary: SSI0_DR → buff */
    ucControlTable[CH10]   = (uint32_t)&SSI0_DR_R;
    ucControlTable[CH10+1] = (uint32_t)(buff + count - 1);
    ucControlTable[CH10+2] = UDMA_SRC_INC_NONE | UDMA_SRC_SIZE_8 |
                             UDMA_DST_INC_8    | UDMA_DST_SIZE_8 |
                             UDMA_ARBSIZE_4    |
                             (((count - 1) & 0x3FFU) << 4) |
                             UDMA_MODE_BASIC;
    ucControlTable[CH10+3] = 0;

    UDMA_USEBURSTSET_R = DMA_CH10_BIT;
    UDMA_REQMASKCLR_R  = DMA_CH10_BIT;
    SSI0_DMACTL_R     |= 0x01;          /* RXDMAE on */
    UDMA_ENASET_R      = DMA_CH10_BIT;

    /* Clock out dummy bytes to drive SPI */
    for (uint32_t i = 0; i < count; i++) {      // Honestly, for optimization
        while (!(SSI0_SR_R & SSI_SR_TNF));      // this should be sent via another DMA channel
        SSI0_DR_R = 0xFF;                       // but it works so whatever
    }

    while (UDMA_ENASET_R & DMA_CH10_BIT);  /* wait for DMA completion */
    while (SSI0_SR_R & SSI_SR_BSY);        /* wait for SPI idle */

    SSI0_DMACTL_R &= ~0x01;               /* RXDMAE off */
}
