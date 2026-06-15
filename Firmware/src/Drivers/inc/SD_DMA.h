/**
 * @file SD_DMA.h
 * @brief SD card DMA read — SSI0 RX via uDMA channel 10 on TM4C123GH6PM
 *
 * Uses uDMA basic mode to transfer received SPI bytes from SSI0's RX FIFO
 * directly into a RAM buffer. The CPU feeds dummy 0xFF bytes into the TX
 * FIFO to generate the SPI clock; DMA drains the RX side in parallel.
 *
 * Call DMA_Init() once at startup, then SD_DMA_CH10_Init() before any
 * SD card operations. diskio.c calls SD_DMA_Read_Sector() internally
 * via rcvr_spi_multi().
 */

#ifndef SD_DMA_H
#define SD_DMA_H

#include <stdint.h>
#include "DMA_Common.h"

/* ---------------------------------------------------------------------------
 * uDMA channel 10 — SSI0 RX (SD Card)
 * --------------------------------------------------------------------------- */
#define CH10            (10 * 4)
#define DMA_CH10_BIT    (1U << 10)

/** @brief Init uDMA channel 10 for SSI0 RX. Call DMA_Init() first. */
void SD_DMA_CH10_Init(void);

/**
 * @brief Blocking DMA read from SD card (SSI0 RX).
 *
 * Pumps dummy bytes into SSI0 TX to generate SPI clock while DMA captures
 * the received data into buff. Returns when all count bytes are transferred.
 *
 * @param buff  Destination buffer (must be count bytes or larger)
 * @param count Number of bytes to read (max 1024, typically 512)
 */
void SD_DMA_Read_Sector(uint8_t *buff, uint32_t count);

#endif /* SD_DMA_H */
