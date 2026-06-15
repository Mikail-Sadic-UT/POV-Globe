/**
 * @file DMA_Common.h
 * @brief Shared uDMA definitions for TM4C123GH6PM
 */

#ifndef DMA_COMMON_H
#define DMA_COMMON_H

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * uDMA control table (shared)
 *
 * 256 uint32_t entries: 128 primary + 128 alternate.
 *   [+0] source/destination end pointer
 *   [+1] destination/source end pointer
 *   [+2] control word (DMACHCTL)
 *   [+3] reserved
 * --------------------------------------------------------------------------- */
extern uint32_t ucControlTable[256];

/* ---------------------------------------------------------------------------
 * DMACHCTL field encodings (TM4C123 datasheet Table 9-4)
 * --------------------------------------------------------------------------- */
#define UDMA_DST_INC_NONE   (3U << 30)  ///< Destination address fixed
#define UDMA_DST_INC_8      (0U << 30)  ///< Destination increments 1 byte
#define UDMA_DST_SIZE_8     (0U << 28)  ///< Destination width = 8-bit
#define UDMA_SRC_INC_8      (0U << 26)  ///< Source increments 1 byte
#define UDMA_SRC_INC_NONE   (3U << 26)  ///< Source address fixed
#define UDMA_SRC_SIZE_8     (0U << 24)  ///< Source width = 8-bit
#define UDMA_ARBSIZE_4      (2U << 14)  ///< Re-arbitrate after 4 transfers
#define UDMA_MODE_BASIC     (1U)        ///< Basic mode (single descriptor)
#define UDMA_MODE_PINGPONG  (3U)        ///< Ping-pong mode (auto-alternate)

/**
 * @brief Enable the uDMA module.
 */
void DMA_Init(void);

#endif /* DMA_COMMON_H */
