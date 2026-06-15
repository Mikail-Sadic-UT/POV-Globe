/*------------------------------------------------------------------------*/
/* STM32F100: MMCv3/SDv1/SDv2 (SPI mode) control module                   */
/*------------------------------------------------------------------------*/
/*
/  Copyright (C) 2014, ChaN, all right reserved.
/
/ * This software is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-------------------------------------------------------------------------*/
// converted to TM4C123
// Feb 22, 2016
// added PB0 as a choice for SDC CS
#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/integer.h"
#include "../inc/diskio.h"
#include "../inc/SD_DMA.h"
#define SDC_CS_PB0 1
#define SDC_CS_PD7 0

// SDC CS is PD7 or PB0 , TFT CS is PA3
// to change CS to another GPIO, change SDC_CS and CS_Init
// **********ST7735 TFT and SDC*******************
// ST7735
// 1  ground
// 2  Vcc +3.3V
// 3  PA7 TFT reset
// 4  PA6 TFT data/command
// 5  PD7/PB0 SDC_CS, active low to enable SDC
// 6  PA3 TFT_CS, active low to enable TFT
// 7  PA5 MOSI SPI data from microcontroller to TFT or SDC
// 8  PA2 Sclk SPI clock from microcontroller to TFT or SDC
// 9  PA4 MISO SPI data from SDC to microcontroller
// 10 Light, backlight connected to +3.3 V

// **********wide.hk ST7735R*******************
// Silkscreen Label (SDC side up; LCD side down) - Connection
// VCC  - +3.3 V
// GND  - Ground
// !SCL - PA2 Sclk SPI clock from microcontroller to TFT or SDC
// !SDA - PA5 MOSI SPI data from microcontroller to TFT or SDC
// DC   - PA6 TFT data/command
// RES  - PA7 TFT reset
// CS   - PA3 TFT_CS, active low to enable TFT
// *CS  - PD7/PB0 SDC_CS, active low to enable SDC
// MISO - PA4 MISO SPI data from SDC to microcontroller
// SDA  � (NC) I2C data for ADXL345 accelerometer
// SCL  � (NC) I2C clock for ADXL345 accelerometer
// SDO  � (NC) I2C alternate address for ADXL345 accelerometer
// Backlight + - Light, backlight connected to +3.3 V

#define TFT_CS           (*((volatile uint32_t *)0x40004020))
#define TFT_CS_LOW       0 // CS controlled by software PA3
#define TFT_CS_HIGH      0x08
#if SDC_CS_PD7
// PD7 output used for SDC CS
#define SDC_CS           (*((volatile uint32_t *)0x40007200))
#define SDC_CS_LOW       0           // CS controlled by software
#define SDC_CS_HIGH      0x80
void CS_Init(void){
  SYSCTL_RCGCGPIO_R |= 0x08;            // activate clock for Port D
  while((SYSCTL_PRGPIO_R&0x08) == 0){}; // allow time for clock to stabilize
  GPIO_PORTD_LOCK_R = 0x4C4F434B;       // unlock GPIO Port D
  GPIO_PORTD_CR_R = 0xFF;               // allow changes to PD7-0
  // only PD7 needs to be unlocked, other bits can't be locked
  GPIO_PORTD_DIR_R |= 0x80;             // make PD7 out
  GPIO_PORTD_AFSEL_R &= ~0x80;          // disable alt funct on PD7
  GPIO_PORTD_DR4R_R |= 0x80;            // 4mA drive on outputs
  GPIO_PORTD_PUR_R |= 0x80;             // enable weak pullup on PD7
  GPIO_PORTD_DEN_R |= 0x80;             // enable digital I/O on PD7
                                        // configure PD7 as GPIO
  GPIO_PORTD_PCTL_R = (GPIO_PORTD_PCTL_R&0x0FFFFFFF)+0x00000000;
  GPIO_PORTD_AMSEL_R &= ~0x80;          // disable analog functionality on PD7
  SDC_CS = SDC_CS_HIGH;
}
#endif
// PB0 output used for SDC CS
#if SDC_CS_PB0
#define SDC_CS           (*((volatile uint32_t *)0x40005004))
#define SDC_CS_LOW       0           // CS controlled by software
#define SDC_CS_HIGH      0x01
void CS_Init(void){
  SYSCTL_RCGCGPIO_R |= 0x02;            // activate clock for Port B
  while((SYSCTL_PRGPIO_R&0x02) == 0){}; // allow time for clock to stabilize
  GPIO_PORTB_DIR_R |= 0x01;             // make PB0 out
  GPIO_PORTB_AFSEL_R &= ~0x01;          // disable alt funct on PB0
  GPIO_PORTB_DR4R_R |= 0x01;            // 4mA drive on outputs
  GPIO_PORTB_PUR_R |= 0x01;             // enable weak pullup on PB0
  GPIO_PORTB_DEN_R |= 0x01;             // enable digital I/O on PB0
                                        // configure PB0 as GPIO
  GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0xFFFFFFF0)+0x00000000;
  GPIO_PORTB_AMSEL_R &= ~0x01;          // disable analog functionality on PB0
  SDC_CS = SDC_CS_HIGH;
}
#endif
//********SSI0_Init*****************
// Initialize SSI0 interface to SDC
// inputs:  clock divider to set clock frequency
// outputs: none
// SSIClk = PIOSC / (CPSDVSR * (1 + SCR)) = 16 MHz/CPSDVSR
// 40 for   400,000 bps slow mode, used during initialization
// 2  for 8,000,000 bps fast mode, used during disk I/O
void Timer5_Init(void);
void SSI0_Init(uint32_t CPSDVSR){
  Timer5_Init();                        // initialize Timer5 for 1 ms interrupts
  CS_Init();                            // initialize whichever GPIO pin is CS for the SD card
  // initialize Port A
  SYSCTL_RCGCGPIO_R |= 0x01;            // activate clock for Port A
  while((SYSCTL_PRGPIO_R&0x01) == 0){}; // allow time for clock to stabilize
  GPIO_PORTA_DIR_R |= 0x08;             // make PA3 out
  GPIO_PORTA_AFSEL_R |= 0x34;           // enable alt funct on PA2,4,5
  GPIO_PORTA_AFSEL_R &= ~0x08;          // disable alt funct on PA3
  GPIO_PORTA_DR4R_R |= 0xFC;            // 4mA drive on outputs
  GPIO_PORTA_PUR_R |= 0x3C;             // enable weak pullup on PA2,3,4,5
  GPIO_PORTA_DEN_R |= 0x3C;             // enable digital I/O on PA2,3,4,5
                                        // configure PA2,4,5 as SSI
  GPIO_PORTA_PCTL_R = (GPIO_PORTA_PCTL_R&0xFF00F0FF)+0x00220200;
                                        // configure PA3 as GPIO
  GPIO_PORTA_PCTL_R = (GPIO_PORTA_PCTL_R&0xFFFF0FFF)+0x00000000;
  GPIO_PORTA_AMSEL_R &= ~0x3C;          // disable analog functionality on PA2,3,4,5
  TFT_CS = TFT_CS_HIGH;                 // disable LCD
  // initialize SSI0
  SYSCTL_RCGCSSI_R |= 0x01;             // activate clock for SSI0
  while((SYSCTL_PRSSI_R&0x01) == 0){};  // allow time for clock to stabilize
  SSI0_CR1_R &= ~SSI_CR1_SSE;           // disable SSI
  SSI0_CR1_R &= ~SSI_CR1_MS;            // master mode
                                        // configure for clock from source PIOSC for baud clock source
  SSI0_CC_R = (SSI0_CC_R&~SSI_CC_CS_M)+SSI_CC_CS_PIOSC;
                                        // clock divider for SSIClk (16 MHz PIOSC/CPSDVSR)
  SSI0_CPSR_R = (SSI0_CPSR_R&~SSI_CPSR_CPSDVSR_M)+CPSDVSR;
  // CPSDVSR must be even from 2 to 254
  SSI0_CR0_R &= ~(SSI_CR0_SCR_M |       // SCR = 0 (16/CPSDVSR Mbps data rate)
                  SSI_CR0_SPH |         // SPH = 0
                  SSI_CR0_SPO);         // SPO = 0
                                        // FRF = Freescale format
  SSI0_CR0_R = (SSI0_CR0_R&~SSI_CR0_FRF_M)+SSI_CR0_FRF_MOTO;
                                        // DSS = 8-bit data
  SSI0_CR0_R = (SSI0_CR0_R&~SSI_CR0_DSS_M)+SSI_CR0_DSS_8;
  SSI0_CR1_R |= SSI_CR1_SSE;            // enable SSI

  //SSI0_DMACTL_R |= 0x01; // Enable RXDMAE (Receive DMA)
}
//void MakeTxhigh(void){
//  GPIO_PORTA_AFSEL_R &= ~0x10;          // disable alt funct on PA4
//  GPIO_PORTA_PCTL_R = (GPIO_PORTA_PCTL_R&0xFFF0FFFF);
//  GPIO_PORTA_DATA_R |= 0x10;            // PA4 high
//}
//void MakeTxSSI(void){
//  GPIO_PORTA_AFSEL_R |= 0x10;           // enable alt funct on PA4
//  GPIO_PORTA_PCTL_R = (GPIO_PORTA_PCTL_R&0xFFF0FFFF)+0x000F0000;
//}

// SSIClk = PIOSC / (CPSDVSR * (1 + SCR)) = 16 MHz/CPSDVSR
// 40 for   400,000 bps slow mode, used during initialization
// 2  for 8,000,000 bps fast mode, used during disk I/O
#define FCLK_SLOW() { SSI0_CPSR_R = (SSI0_CPSR_R&~SSI_CPSR_CPSDVSR_M)+40; }
#define FCLK_FAST() { SSI0_CPSR_R = (SSI0_CPSR_R&~SSI_CPSR_CPSDVSR_M)+2; }

// de-asserts the CS pin to the card
#define CS_HIGH()  SDC_CS = SDC_CS_HIGH;
// asserts the CS pin to the card
#define CS_LOW()  SDC_CS = SDC_CS_LOW;
//#define  MMC_CD    !(GPIOC_IDR & _BV(4))  /* Card detect (yes:true, no:false, default:true) */
#define  MMC_CD    1  /* Card detect (yes:true, no:false, default:true) */
#define  MMC_WP    0 /* Write protected (yes:true, no:false, default:false) */
//#define  SPIx_CR1  SPI1_CR1
//#define  SPIx_SR    SPI1_SR
//#define  SPIx_DR    SPI1_DR
#define  SPIxENABLE() {SSI0_Init(40);}

/*--------------------------------------------------------------------------

   Module Private Functions

---------------------------------------------------------------------------*/
/* MMC/SD command */
#define CMD0   (0)      /* GO_IDLE_STATE */
#define CMD1   (1)      /* SEND_OP_COND (MMC) */
#define ACMD41 (0x80+41)  /* SEND_OP_COND (SDC) */
#define CMD8   (8)     /* SEND_IF_COND */
#define CMD9   (9)     /* SEND_CSD */
#define CMD10  (10)    /* SEND_CID */
#define CMD12  (12)    /* STOP_TRANSMISSION */
#define ACMD13 (0x80+13)  /* SD_STATUS (SDC) */
#define CMD16  (16)    /* SET_BLOCKLEN */
#define CMD17  (17)    /* READ_SINGLE_BLOCK */
#define CMD18  (18)    /* READ_MULTIPLE_BLOCK */
#define CMD23  (23)    /* SET_BLOCK_COUNT (MMC) */
#define ACMD23 (0x80+23)  /* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24  (24)    /* WRITE_BLOCK */
#define CMD25  (25)    /* WRITE_MULTIPLE_BLOCK */
#define CMD32  (32)    /* ERASE_ER_BLK_START */
#define CMD33  (33)    /* ERASE_ER_BLK_END */
#define CMD38  (38)    /* ERASE */
#define CMD55  (55)    /* APP_CMD */
#define CMD58  (58)    /* READ_OCR */

static volatile DSTATUS Stat = STA_NOINIT;  /* Physical drive status */

static volatile UINT Timer1, Timer2;  /* 1kHz decrement timer stopped at zero (disk_timerproc()) */

static BYTE CardType;      /* Card type flags */



/*-----------------------------------------------------------------------*/
/* SPI controls (Platform dependent)                                     */
/*-----------------------------------------------------------------------*/

/* Initialize MMC interface */
static void init_spi(void){
  SPIxENABLE();    /* Enable SPI function */
  CS_HIGH();       /* Set CS# high */

  for (Timer1 = 10; Timer1; ) ;  /* 10ms */
}


/* Exchange a byte */
// Inputs:  byte to be sent to SPI
// Outputs: byte received from SPI
// assumes it has been selected with CS low
static BYTE xchg_spi(BYTE dat){ BYTE volatile rcvdat;
// wait until SSI0 not busy/transmit FIFO empty
  while((SSI0_SR_R&SSI_SR_BSY)==SSI_SR_BSY){};
  SSI0_DR_R = dat;                      // data out
  while((SSI0_SR_R&SSI_SR_RNE)==0){};   // wait until response
  rcvdat = SSI0_DR_R;                   // acknowledge response
  return rcvdat;
}

/*-----------------------------------------------------------------------*/
/* Receive a byte from MMC via SPI  (Platform dependent)                 */
/*-----------------------------------------------------------------------*/
// Inputs:  none
// Outputs: byte received from SPI
// assumes it has been selected with CS low
static BYTE rcvr_spi(void){
// wait until SSI0 not busy/transmit FIFO empty
  while((SSI0_SR_R&SSI_SR_BSY)==SSI_SR_BSY){};
  SSI0_DR_R = 0xFF;                     // data out, garbage
  while((SSI0_SR_R&SSI_SR_RNE)==0){};   // wait until response
  return (BYTE)SSI0_DR_R;               // read received data
}

/* Receive multiple byte */
// Input:  buff Pointer to empty buffer into which data will be received
//         btr  Number of bytes to receive (even number)
// Output: none
/*
static void rcvr_spi_multi(BYTE *buff, UINT btr){
  while(btr){
    *buff = rcvr_spi();   // return by reference
    btr--; buff++;
  }
  while((SSI0_SR_R & SSI_SR_BSY) == SSI_SR_BSY){}; //CHANGE BACK IF DOESNT FIX
}
*/

// Entries in ucControlTable for Channel 10 (SSI0 RX)
// Ch 10 is at index 10*4 = 40 in the table

static void rcvr_spi_multi(BYTE *buff, UINT btr) {
    SD_DMA_Read_Sector(buff, btr);
}

#if _USE_WRITE
/* Send multiple byte */
// Input:  buff Pointer to the data which will be sent
//         btx  Number of bytes to send (even number)
// Output: none
static void xmit_spi_multi(const BYTE *buff, UINT btx){
  BYTE volatile rcvdat;
  while(btx){
    SSI0_DR_R = *buff;                  // data out
    while((SSI0_SR_R&SSI_SR_RNE)==0){}; // wait until response
    rcvdat = SSI0_DR_R;                 // acknowledge response
    btx--; buff++;
  }
}
#endif


/*-----------------------------------------------------------------------*/
/* Wait for card ready                                                   */
/*-----------------------------------------------------------------------*/
// Input:  time to wait in ms
// Output: 1:Ready, 0:Timeout
static int wait_ready(UINT wt){
  BYTE d;
  Timer2 = wt;
  do {
    d = xchg_spi(0xFF);
    /* This loop takes a time. Insert rot_rdq() here for multitask environment. */
  } while (d != 0xFF && Timer2);  /* Wait for card goes ready or timeout */
  return (d == 0xFF) ? 1 : 0;
}



/*-----------------------------------------------------------------------*/
/* Deselect card and release SPI                                         */
/*-----------------------------------------------------------------------*/
static void deselect(void){
  CS_HIGH();       /* CS = H */
  xchg_spi(0xFF);  /* Dummy clock (force DO hi-z for multiple slave SPI) */
}



/*-----------------------------------------------------------------------*/
/* Select card and wait for ready                                        */
/*-----------------------------------------------------------------------*/
// Input:  none
// Output: 1:OK, 0:Timeout in 500ms
static int select(void){

  // Wait for any in-flight DMA to finish, then reclaim SSI0 for CPU use
  while(UDMA_ENASET_R & DMA_CH10_BIT);
  while(SSI0_SR_R & SSI_SR_BSY);
  UDMA_ENACLR_R = DMA_CH10_BIT;
  SSI0_DMACTL_R &= ~0x01;             // RXDMAE off — CPU owns the FIFO now

  TFT_CS = TFT_CS_HIGH; // make sure TFT is off
  CS_LOW();

  // flush any stale bytes from RX FIFO
  while(SSI0_SR_R & SSI_SR_RNE){ 
      (void)SSI0_DR_R; 
  }

  xchg_spi(0xFF);  /* Dummy clock (force DO enabled) */
  if(wait_ready(500)) return 1;  /* OK */
  deselect();
  return 0;  /* Timeout */
}



/*-----------------------------------------------------------------------*/
/* Receive a data packet from the MMC                                    */
/*-----------------------------------------------------------------------*/
// Input:  buff Pointer to empty buffer into which data will be received
//         btr  Number of bytes to receive (even number)
// Output: 1:OK, 0:Error on timeout
static int rcvr_datablock(BYTE *buff, UINT btr){
  BYTE token;
  Timer1 = 200;
  do {              /* Wait for DataStart token in timeout of 200ms */
    token = xchg_spi(0xFF);
    /* This loop will take a time. Insert rot_rdq() here for multitask envilonment. */
  } while ((token == 0xFF) && Timer1);
  if(token != 0xFE) return 0;    /* Function fails if invalid DataStart token or timeout */

  rcvr_spi_multi(buff, btr);    /* Store trailing data to the buffer */
  xchg_spi(0xFF); xchg_spi(0xFF);      /* Discard CRC */
  return 1;            /* Function succeeded */
}



/*-----------------------------------------------------------------------*/
/* Send a data packet to the MMC                                         */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
// Input:  buff Pointer to 512 byte data which will be sent
//         token  Token
// Output: 1:OK, 0:Failed on timeout
static int xmit_datablock(const BYTE *buff, BYTE token){
  BYTE resp;
  if (!wait_ready(500)) return 0;    /* Wait for card ready */
  //if (!wait_ready(2000)) return 0;  // increase to 2 seconds (SINCE 32GB, CHANGE BACK IF DOESNT FIX)

  xchg_spi(token);                   /* Send token */
  if (token != 0xFD) {               /* Send data if token is other than StopTran */
    xmit_spi_multi(buff, 512);       /* Data */
    while((SSI0_SR_R & SSI_SR_BSY) == SSI_SR_BSY){};  // wait for last byte to finish (CHANGE BACK IF DOESNT FIX)
    xchg_spi(0xFF); xchg_spi(0xFF);  /* Dummy CRC */

    BYTE n = 8;                       // poll up to 8 bytes (CHANGE BACK IF DOESNT FIX)
    do {
      resp = xchg_spi(0xFF);
    } while (resp == 0xFF && --n);

    //resp = xchg_spi(0xFF);        /* Receive data resp */
    if ((resp & 0x1F) != 0x05)    /* Function fails if the data packet was not accepted */
      return 0;
  }
  return 1;
}
#endif


/*-----------------------------------------------------------------------*/
/* Send a command packet to the MMC                                      */
/*-----------------------------------------------------------------------*/
// Inputs:  cmd Command index
//          arg    /* Argument
// Outputs: R1 resp (bit7==1:Failed to send)
static BYTE send_cmd(BYTE cmd, DWORD arg){
  BYTE n, res;
  if (cmd & 0x80) {  /* Send a CMD55 prior to ACMD<n> */
    cmd &= 0x7F;
    res = send_cmd(CMD55, 0);
    if (res > 1) return res;
  }

  /* Select the card and wait for ready except to stop multiple block read */
  if (cmd != CMD12) {
    deselect();
    if (!select()) return 0xFF;
  }

  /* Send command packet */
  xchg_spi(0x40 | cmd);        /* Start + command index */
  xchg_spi((BYTE)(arg >> 24));    /* Argument[31..24] */
  xchg_spi((BYTE)(arg >> 16));    /* Argument[23..16] */
  xchg_spi((BYTE)(arg >> 8));      /* Argument[15..8] */
  xchg_spi((BYTE)arg);        /* Argument[7..0] */
  n = 0x01;              /* Dummy CRC + Stop */
  if (cmd == CMD0) n = 0x95;      /* Valid CRC for CMD0(0) */
  if (cmd == CMD8) n = 0x87;      /* Valid CRC for CMD8(0x1AA) */
  xchg_spi(n);

  /* Receive command resp */
  if (cmd == CMD12) xchg_spi(0xFF);  /* Diacard following one byte when CMD12 */
  n = 10;                /* Wait for response (10 bytes max) */
  do
    res = xchg_spi(0xFF);
  while ((res & 0x80) && --n);

  return res;              /* Return received response */
}



/*--------------------------------------------------------------------------

   Public Functions

---------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------*/
/* Initialize disk drive                                                 */
/*-----------------------------------------------------------------------*/
// Inputs:  Physical drive number, which must be 0
// Outputs: status (see DSTATUS)
DSTATUS disk_initialize(BYTE drv){
  BYTE n, cmd, ty, ocr[4];

  if (drv) return STA_NOINIT;      /* Supports only drive 0 */
  init_spi();              /* Initialize SPI */

  if (Stat & STA_NODISK) return Stat;  /* Is card existing in the soket? */

  FCLK_SLOW();
  for (n = 10; n; n--) xchg_spi(0xFF);  /* Send 80 dummy clocks */

  ty = 0;
  if (send_cmd(CMD0, 0) == 1) {      /* Put the card SPI/Idle state */
    Timer1 = 1000;            /* Initialization timeout = 1 sec */
    if (send_cmd(CMD8, 0x1AA) == 1) {  /* SDv2? */
      for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF);  /* Get 32 bit return value of R7 resp */
      if (ocr[2] == 0x01 && ocr[3] == 0xAA) {        /* Is the card supports vcc of 2.7-3.6V? */
        while (Timer1 && send_cmd(ACMD41, 1UL << 30)) ;  /* Wait for end of initialization with ACMD41(HCS) */
        if (Timer1 && send_cmd(CMD58, 0) == 0) {    /* Check CCS bit in the OCR */
          for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF);
          ty = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2;  /* Card id SDv2 */
        }
      }
    } else {  /* Not SDv2 card */
      if (send_cmd(ACMD41, 0) <= 1)   {  /* SDv1 or MMC? */
        ty = CT_SD1; cmd = ACMD41;  /* SDv1 (ACMD41(0)) */
      } else {
        ty = CT_MMC; cmd = CMD1;  /* MMCv3 (CMD1(0)) */
      }
      while (Timer1 && send_cmd(cmd, 0)) ;    /* Wait for end of initialization */
      if (!Timer1 || send_cmd(CMD16, 512) != 0)  /* Set block length: 512 */
        ty = 0;
    }
  }
  CardType = ty;  /* Card type */
  deselect();

  if (ty) {      /* OK */
    FCLK_FAST();      /* Set fast clock */
    Stat &= ~STA_NOINIT;  /* Clear STA_NOINIT flag */
  } else {      /* Failed */
    Stat = STA_NOINIT;
  }

  return Stat;
}



/*-----------------------------------------------------------------------*/
/* Get disk status                                                       */
/*-----------------------------------------------------------------------*/
// Inputs:  Physical drive number, which must be 0
// Outputs: status (see DSTATUS)
DSTATUS disk_status(BYTE drv){
  if (drv) return STA_NOINIT;    /* Supports only drive 0 */
  return Stat;  /* Return disk status */
}



/*-----------------------------------------------------------------------*/
/* Read sector(s)                                                        */
/*-----------------------------------------------------------------------*/
//Inputs:  drv    Physical drive number (0)
//         buff   Pointer to the data buffer to store read data
//         sector Start sector number (LBA)
//         count  Number of sectors to read (1..128)
// Outputs: status (see DRESULT)
DRESULT disk_read(BYTE drv, BYTE *buff, DWORD sector, UINT count){
  if (drv || !count) return RES_PARERR;    /* Check parameter */
  if (Stat & STA_NOINIT) return RES_NOTRDY;  /* Check if drive is ready */

  if (!(CardType & CT_BLOCK)) sector *= 512;  /* LBA ot BA conversion (byte addressing cards) */

  if (count == 1) {  /* Single sector read */
    if ((send_cmd(CMD17, sector) == 0)  /* READ_SINGLE_BLOCK */
      && rcvr_datablock(buff, 512))
      count = 0;
  }
  else {        /* Multiple sector read */
    if (send_cmd(CMD18, sector) == 0) {  /* READ_MULTIPLE_BLOCK */
      do {
        if (!rcvr_datablock(buff, 512)) break;
        buff += 512;
      } while (--count);
      send_cmd(CMD12, 0);        /* STOP_TRANSMISSION */
    }
  }
  deselect();

  return count ? RES_ERROR : RES_OK;  /* Return result */
}



/*-----------------------------------------------------------------------*/
/* Write sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
//Inputs:  drv    Physical drive number (0)
//         buff   Pointer to the data buffer to write to disk
//         sector Start sector number (LBA)
//         count  Number of sectors to write (1..128)
// Outputs: status (see DRESULT)
DRESULT disk_write(BYTE drv, const BYTE *buff, DWORD sector, UINT count){
  if (drv || !count) return RES_PARERR;    /* Check parameter */
  if (Stat & STA_NOINIT) return RES_NOTRDY;  /* Check drive status */
  if (Stat & STA_PROTECT) return RES_WRPRT;  /* Check write protect */

  if (!(CardType & CT_BLOCK)) sector *= 512;  /* LBA ==> BA conversion (byte addressing cards) */

  //if (count == 1) {  /* Single sector write */
  //  if ((send_cmd(CMD24, sector) == 0)  /* WRITE_BLOCK */
  //    && xmit_datablock(buff, 0xFE))
  //    count = 0;
  //}

  if (count == 1) {
    BYTE cmdResp = send_cmd(CMD24, sector);
    if (cmdResp == 0) {
        if (xmit_datablock(buff, 0xFE))
            count = 0;
        // else: xmit_datablock failed
    }
    // else: CMD24 rejected, cmdResp has the R1 error bits
  }
  else {        /* Multiple sector write */
    if (CardType & CT_SDC) send_cmd(ACMD23, count);  /* Predefine number of sectors */
    if (send_cmd(CMD25, sector) == 0) {  /* WRITE_MULTIPLE_BLOCK */
      do {
        if (!xmit_datablock(buff, 0xFC)) break;
        buff += 512;
      } while (--count);
      if (!xmit_datablock(0, 0xFD))  /* STOP_TRAN token */
        count = 1;
    }
  }
  deselect();

  return count ? RES_ERROR : RES_OK;  /* Return result */
}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous drive controls other than data read/write               */
/*-----------------------------------------------------------------------*/
// Inputs:  drv,   Physical drive number (0)
//          cmd,   Control command code
//          buff   Pointer to the control data
// Outputs: status (see DRESULT)
#if _USE_IOCTL
DRESULT disk_ioctl(BYTE drv, BYTE cmd, void *buff){
  DRESULT res;
  BYTE n, csd[16];
  DWORD *dp, st, ed, csize;


  if (drv) return RES_PARERR;          /* Check parameter */
  if (Stat & STA_NOINIT) return RES_NOTRDY;  /* Check if drive is ready */

  res = RES_ERROR;

  switch (cmd) {
  case CTRL_SYNC :    /* Wait for end of internal write process of the drive */
    if (select()) res = RES_OK;
    break;

  case GET_SECTOR_COUNT :  /* Get drive capacity in unit of sector (DWORD) */
    if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
      if ((csd[0] >> 6) == 1) {  /* SDC ver 2.00 */
        csize = csd[9] + ((WORD)csd[8] << 8) + ((DWORD)(csd[7] & 63) << 16) + 1;
        *(DWORD*)buff = csize << 10;
      } else {          /* SDC ver 1.XX or MMC ver 3 */
        n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
        csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
        *(DWORD*)buff = csize << (n - 9);
      }
      res = RES_OK;
    }
    break;

  case GET_BLOCK_SIZE :  /* Get erase block size in unit of sector (DWORD) */
    if (CardType & CT_SD2) {  /* SDC ver 2.00 */
      if (send_cmd(ACMD13, 0) == 0) {  /* Read SD status */
        xchg_spi(0xFF);
        if (rcvr_datablock(csd, 16)) {        /* Read partial block */
          for (n = 64 - 16; n; n--) xchg_spi(0xFF);  /* Purge trailing data */
          *(DWORD*)buff = 16UL << (csd[10] >> 4);
          res = RES_OK;
        }
      }
    } else {          /* SDC ver 1.XX or MMC */
      if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {  /* Read CSD */
        if (CardType & CT_SD1) {  /* SDC ver 1.XX */
          *(DWORD*)buff = (((csd[10] & 63) << 1) + ((WORD)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
        } else {          /* MMC */
          *(DWORD*)buff = ((WORD)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
        }
        res = RES_OK;
      }
    }
    break;

  case CTRL_TRIM :  /* Erase a block of sectors (used when _USE_ERASE == 1) */
    if (!(CardType & CT_SDC)) break;        /* Check if the card is SDC */
    if (disk_ioctl(drv, MMC_GET_CSD, csd)) break;  /* Get CSD */
    if (!(csd[0] >> 6) && !(csd[10] & 0x40)) break;  /* Check if sector erase can be applied to the card */
    dp = buff; st = dp[0]; ed = dp[1];        /* Load sector block */
    if (!(CardType & CT_BLOCK)) {
      st *= 512; ed *= 512;
    }
    if (send_cmd(CMD32, st) == 0 && send_cmd(CMD33, ed) == 0 && send_cmd(CMD38, 0) == 0 && wait_ready(30000))  /* Erase sector block */
      res = RES_OK;  /* FatFs does not check result of this command */
    break;

  default:
    res = RES_PARERR;
  }

  deselect();

  return res;
}
#endif


/*-----------------------------------------------------------------------*/
/* Device timer function                                                 */
/*-----------------------------------------------------------------------*/
/* This function must be called from timer interrupt routine in period
/  of 1 ms to generate card control timing.
*/

void disk_timerproc (void)
{
  WORD n;
  BYTE s;


  n = Timer1;            /* 1kHz decrement timer stopped at 0 */
  if (n) Timer1 = --n;
  n = Timer2;
  if (n) Timer2 = --n;

  s = Stat;
  if (MMC_WP)    /* Write protected */
    s |= STA_PROTECT;
  else    /* Write enabled */
    s &= ~STA_PROTECT;
  if (MMC_CD)  /* Card is in socket */
    s &= ~STA_NODISK;
  else    /* Socket empty */
    s |= (STA_NODISK | STA_NOINIT);
  Stat = s;
}


void Timer5_Init(void){
  SYSCTL_RCGCTIMER_R |= 0x20;
  while((SYSCTL_PRTIMER_R&0x20) == 0){};
  TIMER5_CTL_R = 0x00000000;       // 1) disable timer5 during setup
  TIMER5_CFG_R = 0x00000000;       // 2) configure for 32-bit mode
  TIMER5_TAMR_R = 0x00000002;      // 3) configure for periodic mode, default down-count settings
  TIMER5_TAILR_R = 79999;          // 4) reload value, 1 ms, 80 MHz clock
  TIMER5_TAPR_R = 0;               // 5) bus clock resolution
  TIMER5_ICR_R = 0x00000001;       // 6) clear timer5A timeout flag
  TIMER5_IMR_R = 0x00000001;       // 7) arm timeout interrupt
  NVIC_PRI23_R = (NVIC_PRI23_R&0xFFFFFF00)|0x00000080; // 8) priority 4
// interrupts enabled in the main program after all devices initialized
// vector number 108, interrupt number 92
  NVIC_EN2_R = 0x10000000;         // 9) enable interrupt 92 in NVIC
  TIMER5_CTL_R = 0x00000001;       // 10) enable timer5A
}
// Executed every 1 ms
void Timer5A_Handler(void){
  TIMER5_ICR_R = 0x00000001;       // acknowledge timer5A timeout
  disk_timerproc();
}
