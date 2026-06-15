/**
 * @file SK9822_DMA_SSI.c
 * @brief SK9822 LED driver — SSI2 + uDMA channel 13 ping-pong
 *
 * Buffer is split at the midpoint into primary and alternate descriptors.
 * uDMA auto-switches between them with no SPI gap. SSI2_Handler fires on
 * alternate completion and clears dmaBusy. DMA_Done_Callback() is called
 * to trigger the next frame build (ISR_BUILD_ON_DMA_DONE mode).
 */

#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/SK9822_DMA_SSI.h"

static const uint8_t *dma_buf;
static uint32_t       dma_half;
static uint32_t       dma_second;

volatile uint8_t dmaBusy = 0;

extern void DMA_Done_Callback(void);

/* ─────────────────────────────────────────────────────────────────── */

void SK9822_SPI_Init(uint8_t cpsdvsr, uint8_t scr) {
    volatile uint32_t delay;

    SYSCTL_RCGCSSI_R  |= SSI2_CLK;
    SYSCTL_RCGCGPIO_R |= PORTB_CLK;
    delay = SYSCTL_RCGC2_R; (void)delay;

    GPIO_PORTB_AFSEL_R |=  PB4_7;
    GPIO_PORTB_DEN_R   |=  PB4_7;
    GPIO_PORTB_PCTL_R   = (GPIO_PORTB_PCTL_R & ~PB4_7_PCTL_CLR) | PB4_7_PCTL_SET;
    GPIO_PORTB_AMSEL_R  = 0;

    SSI2_CR1_R &= ~SSI_CR1_SSE;
    SSI2_CR1_R &= ~SSI_CR1_MS;
    SSI2_CPSR_R = (SSI2_CPSR_R & ~SSI_CPSR_CPSDVSR_M) | cpsdvsr;
    SSI2_CR0_R &= ~(SSI_CR0_SCR_M | SSI_CR0_DSS_M);
    SSI2_CR0_R |= ((uint32_t)scr << 8) | SSI_CR0_DSS_8 | SSI_CR0_FRF_MOTO;
    SSI2_DMACTL_R |= SSI_DMACTL_TXDMAE;

    NVIC_PRI14_R = (NVIC_PRI14_R & ~SSI2_IRQ_PRI_MSK) | SSI2_IRQ_PRI;
    NVIC_EN1_R  |= SSI2_IRQ_EN;

    SSI2_CR1_R |= SSI_CR1_SSE;
}

/* ─────────────────────────────────────────────────────────────────── */

void SK9822_DMA_CH13_Init(void) {
    UDMA_CHMAP1_R = (UDMA_CHMAP1_R & ~0x00F00000) | 0x00200000;  // CH13 = SSI2 TX 
    UDMA_ENACLR_R      = DMA_CH_BIT;
    UDMA_PRIOCLR_R     = DMA_CH_BIT;
    UDMA_ALTCLR_R      = DMA_CH_BIT;
    UDMA_USEBURSTCLR_R = DMA_CH_BIT;
    UDMA_REQMASKCLR_R  = DMA_CH_BIT;
}

/* ─────────────────────────────────────────────────────────────────── */

static void setPrimary(void) {
    ucControlTable[CH13]   = (uint32_t)(dma_buf + dma_half - 1);
    ucControlTable[CH13+1] = (uint32_t)&SSI2_DR_R;
    ucControlTable[CH13+2] = UDMA_DST_INC_NONE | UDMA_DST_SIZE_8 |
                             UDMA_SRC_INC_8    | UDMA_SRC_SIZE_8 |
                             UDMA_ARBSIZE_4    |
                             (((dma_half - 1) & 0x3FFU) << 4) |
                             UDMA_MODE_PINGPONG;
    ucControlTable[CH13+3] = 0;
}

static void setAlternate(void) {
    ucControlTable[CH13ALT]   = (uint32_t)(dma_buf + dma_half + dma_second - 1);
    ucControlTable[CH13ALT+1] = (uint32_t)&SSI2_DR_R;
    ucControlTable[CH13ALT+2] = UDMA_DST_INC_NONE | UDMA_DST_SIZE_8 |
                                UDMA_SRC_INC_8    | UDMA_SRC_SIZE_8 |
                                UDMA_ARBSIZE_4    |
                                (((dma_second - 1) & 0x3FFU) << 4) |
                                UDMA_MODE_PINGPONG;
    ucControlTable[CH13ALT+3] = 0;
}

/* ─────────────────────────────────────────────────────────────────── */

int8_t SK9822_DMA_Start(const uint8_t *buf, uint32_t len) {
    if (len < 2 || len > 2048) return -1;
    if (dmaBusy)               return -1;

    UDMA_REQMASKSET_R = DMA_CH_BIT;

    dma_buf    = buf;
    dma_half   = len / 2;
    dma_second = len - dma_half;

    UDMA_ENACLR_R = DMA_CH_BIT;
    UDMA_ALTCLR_R = DMA_CH_BIT;

    setPrimary();
    setAlternate();

    UDMA_REQMASKCLR_R = DMA_CH_BIT;
    dmaBusy = 1;
    UDMA_ENASET_R = DMA_CH_BIT;

    return 0;
}

int SK9822_DMA_IsBusy(void) { return dmaBusy; }

void SSI2_Handler(void) {
    if (!(UDMA_CHIS_R & DMA_CH_BIT)) return;
    UDMA_CHIS_R = DMA_CH_BIT;

    if ((ucControlTable[CH13ALT+2] & 0x7U) == 0) {
        UDMA_ENACLR_R = DMA_CH_BIT;
        dmaBusy = 0;
        DMA_Done_Callback();
    }
}
