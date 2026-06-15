# SK9822 DMA/SSI Driver

## Hardware Path: CPU → uDMA → SSI2 → LEDs

```
SRAM frame buffer
       │
       │  uDMA channel 13
       ▼
  [uDMA controller] ──── burst 4 bytes when FIFO has space ────▶ [SSI2 TX FIFO] ──▶ SPI pins ──▶ LEDs
                              ▲
                              │ DMA request signal
                         SSI2 TX FIFO draining
```

The SSI2 peripheral has a TX FIFO (8 entries deep on TM4C123). When it drains down to the arbitration threshold, it fires a **DMA request** on its dedicated uDMA channel. The uDMA controller responds by reading bytes from SRAM and writing them into the FIFO — completely autonomously.

---

## Ping-Pong Mode

uDMA has two descriptor slots per channel: **primary** and **alternate**. In ping-pong mode:

- The controller starts executing the **primary** descriptor
- When primary completes, it **automatically switches** to the **alternate** descriptor — with zero gap
- It then fires an interrupt
- The ISR can reload the primary descriptor while alternate is running, creating an endless pipeline

In this driver, ping-pong is used more simply: split a single frame buffer in half, put the first half in primary, the second half in alternate, and let them chain. No reloading needed — just a clean way to send one big buffer that might exceed the 1023-byte single-descriptor limit (the XFERSIZE field is 10 bits).

```
Primary descriptor:          Alternate descriptor:
buf[0 .. half-1]        →   buf[half .. len-1]
                ↘                              ↘
            auto-switch                    interrupt → dmaBusy=0
```

---

## The Control Table

```c
uint32_t ucControlTable[256] __attribute__((aligned(1024)));
```

The uDMA hardware finds its descriptors through a base address register (`UDMA_CTLBASE_R`). The table has a rigid layout mandated by the hardware:

- **128 primary entries** at offset 0
- **128 alternate entries** at offset 512 bytes

Each entry is **4 × uint32_t** (16 bytes):

```
[0]  Source end address      (last byte, not first)
[1]  Destination end address
[2]  Control word            (packed bitfield with sizes, increment, mode, count)
[3]  Reserved/unused
```

The **1024-byte alignment** requirement is because the hardware uses the base address plus a fixed index formula to locate channel N's descriptor — it assumes the table starts at an aligned boundary.

For channel 13, the primary descriptor lives at indices `[52..55]` (13×4), and alternate at `[52+128..55+128]` = `[180..183]`. The `CH13` and `CH13ALT` macros presumably encode these offsets.

### Why "end address" and not start?

Specific to TM4C uDMA design. The controller decrements an internal counter and adds offsets, so it needs to know where the *last* byte is upfront. That's why you see:

```c
ucControlTable[CH13] = (uint32_t)(dma_buf + dma_half - 1);  // end, not start
```

### The control word

```c
UDMA_DST_INC_NONE |   // destination doesn't increment — SSI DR is always same address
UDMA_DST_SIZE_8   |   // 8-bit destination writes
UDMA_SRC_INC_8    |   // source increments by 1 byte after each transfer
UDMA_SRC_SIZE_8   |   // 8-bit source reads
UDMA_ARBSIZE_4    |   // re-arbitrate every 4 bytes (matches FIFO granularity)
((count - 1) << 4)|   // transfer size minus one, 10-bit field
UDMA_MODE_PINGPONG    // ping-pong mode
```

`ARBSIZE_4` tells the DMA controller to release the bus after every 4-byte burst, giving other DMA channels a chance to run. It also matches natural SPI FIFO behavior — 4 bytes in, 4 bytes out.

---

## SSI2 Initialization (`SK9822_SPI_Init`)

### Clock gating

```c
SYSCTL_RCGCSSI_R  |= SSI2_CLK;
SYSCTL_RCGCGPIO_R |= PORTB_CLK;
delay = SYSCTL_RCGC2_R;   // dummy read — stall for ~2 clock cycles
```

On TM4C123, peripherals are clock-gated by default. You write a bit to `RCGC` (Run-mode Clock Gating Control) to enable them. The dummy read is a documented requirement — the peripheral needs a few cycles after the clock enable before you can safely access its registers. Reading any register creates the necessary stall without burning cycles in a counted loop.

### GPIO alternate function

```c
GPIO_PORTB_AFSEL_R |= PB4_7;           // hand pins to peripheral, not GPIO logic
GPIO_PORTB_PCTL_R   = ... | PB4_7_PCTL_SET;  // select which peripheral (AF2 = SSI2)
GPIO_PORTB_DEN_R   |= PB4_7;           // digital enable (not analog)
GPIO_PORTB_AMSEL_R  = 0;               // disable analog (ADC) on these pins
```

TM4C pins are multiplexed. `AFSEL` says "don't use me as GPIO," and `PCTL` selects *which* peripheral among the several that share that pin. The encoding (AF2 = SSI2 on PB4/PB7) comes directly from the datasheet's pin mux table.

### SSI configuration

```c
SSI2_CR1_R &= ~SSI_CR1_SSE;  // MUST disable SSI before changing any config register
SSI2_CR1_R &= ~SSI_CR1_MS;   // 0 = master mode
SSI2_CPSR_R = cpsdvsr;       // clock prescaler (must be even, ≥2)
SSI2_CR0_R |= (scr << 8) | SSI_CR0_DSS_8 | SSI_CR0_FRF_MOTO;
```

The SPI clock rate is: `f_SPI = f_SYS / (CPSDVSR × (1 + SCR))`. The caller passes both parameters to tune the rate. `FRF_MOTO` selects Motorola SPI frame format (CPOL=0, CPHA=0 by default), which is what SK9822 expects.

### Arming DMA before enabling SSI

```c
SSI2_DMACTL_R |= SSI_DMACTL_TXDMAE;  // route TX events to uDMA
// ... NVIC setup ...
SSI2_CR1_R |= SSI_CR1_SSE;           // enable SSI last
```

DMA is enabled on the SSI side *before* SSI itself turns on. This avoids a race where SSI enables and immediately fires a TX request before uDMA is ready to respond.

---

## uDMA Initialization (`SK9822_DMA_Init`)

```c
UDMA_CFG_R     = 0x01;                        // enable the uDMA controller itself
UDMA_CTLBASE_R = (uint32_t)ucControlTable;    // tell it where the descriptor table lives
```

### Channel mapping

```c
UDMA_CHMAP1_R = (UDMA_CHMAP1_R & ~0x00F00000) | 0x00200000;
```

Each uDMA channel can be connected to several possible peripherals. `CHMAP1` is a 32-bit register where each nibble (4 bits) selects the peripheral for one channel. Channel 13 lives in `CHMAP1` bits [23:20]. Writing `2` selects SSI2 TX — this encoding comes from Table 9-1 in the TM4C123 datasheet and is hardware-fixed.

### Channel reset sequence

```c
UDMA_ENACLR_R      = DMA_CH_BIT;   // disabled, not running
UDMA_PRIOCLR_R     = DMA_CH_BIT;   // normal (not high) priority
UDMA_ALTCLR_R      = DMA_CH_BIT;   // will start on primary descriptor
UDMA_USEBURSTCLR_R = DMA_CH_BIT;   // respond to single requests too, not just burst
UDMA_REQMASKCLR_R  = DMA_CH_BIT;   // unmask — let peripheral requests through
```

Each of these is a write-1-to-clear register (writing a bit clears that channel's flag without affecting others). This is the standard "reset to known state" sequence before you configure a channel.

---

## The Start Sequence and the REQMASK Safety Pattern

```c
UDMA_REQMASKSET_R = DMA_CH_BIT;  // BLOCK peripheral requests
// ... write descriptors to SRAM ...
UDMA_REQMASKCLR_R = DMA_CH_BIT;  // UNBLOCK
dmaBusy = 1;
UDMA_ENASET_R     = DMA_CH_BIT;  // ARM channel
```

This is a subtle but important safety pattern. If SSI2 is already running and its FIFO drains mid-descriptor-write, it could fire a DMA request at the exact moment you're halfway through writing a descriptor — the controller would read garbage. The mask temporarily blocks that peripheral's requests from reaching the channel, giving you an atomic window to commit all 8 words (two descriptors) before the channel is live.

The commented-out `__DSB()`/`__DMB()` calls are about the same concern at the CPU level — Cortex-M4 has a write buffer that can defer SRAM stores. In practice the REQMASK sandwich provides sufficient ordering, but the barriers are there as a documented escape hatch.

---

## The Interrupt Handler

```c
extern void DMA_Done_Callback(void);

void SSI2_Handler(void) {
    if(!(UDMA_CHIS_R & DMA_CH_BIT)) return;   // guard against spurious
    UDMA_CHIS_R = DMA_CH_BIT;                 // clear by writing 1

    if((ucControlTable[CH13ALT+2] & 0x7U) == 0) {   // mode bits == STOP?
        UDMA_ENACLR_R = DMA_CH_BIT;
        dmaBusy = 0;
        DMA_Done_Callback();                  // hook for next-frame build
    }
}
```

The handler fires on *both* descriptor completions (primary → alternate switch, and alternate completion), because ping-pong generates an interrupt each time. The primary completion is a no-op here — the transfer is already continuing in the alternate descriptor automatically.

For the alternate completion, the mode bits in the control word drop to `0b000` (STOP mode), which is the hardware's way of saying "this descriptor is exhausted." That's the signal that the entire frame is done.

`DMA_Done_Callback()` is provided by the application (defined in `main_sd.c` when `ISR_BUILD_ON_DMA_DONE` mode is selected). It runs the next frame's `BuildFrame()` immediately so the CPU can prepare column N+1 in parallel with the SPI hardware finishing column N.

The commented-out `while(SSI2_SR_R & SSI_SR_BSY)` is worth noting: uDMA completion means the FIFO has been *filled*, not that SPI has finished *clocking out*. If you reconfigured the pins or started a new frame immediately, you could clip the last byte. The busy-wait ensures the shift register fully empties first.
