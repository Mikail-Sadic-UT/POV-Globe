/**
 * @file DMA_Common.c
 * @brief uDMA controller init — shared by all DMA channels.
 */

#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/DMA_Common.h"

/* 1024-byte aligned control table shared by all channels */
uint32_t ucControlTable[256] __attribute__((aligned(1024)));

void DMA_Init(void) {
    volatile uint32_t delay;
    SYSCTL_RCGCDMA_R |= 0x01;
    delay = SYSCTL_RCGCDMA_R; (void)delay;
    UDMA_CFG_R     = 0x01;
    UDMA_CTLBASE_R = (uint32_t)ucControlTable;
}
