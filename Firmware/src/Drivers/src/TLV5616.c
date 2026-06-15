// TLV5616.c
// Runs on TM4C123
// Use SSI1 to send a 16-bit code to the TLV5616 and return the reply.
// Daniel Valvano
// EE445L Fall 2015
//    Jonathan W. Valvano 9/22/15

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to ARM Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2014

 Copyright 2014 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */

// SSIClk (SCLK) connected to PD0
// SSIFss (FS)   connected to PD1
// SSITx (DIN)   connected to PD3

#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/TLV5616.h"

//----------------   DAC_Init     -------------------------------------------
// Initialize TLV5616 12-bit DAC
// assumes bus clock is 80 MHz
// inputs: initial voltage output (0 to 4095)
// outputs:none

#define SSI1 0x02
#define PortD 0x08
#define PD310 (1<<3) | (1<<1) | (1<<0)

void DAC_Init(uint16_t data){

  SYSCTL_RCGCSSI_R |= SSI1;                 // Enable SSI1 clock
  SYSCTL_RCGCGPIO_R |= PortD;               // Enable Port D clock
  while((SYSCTL_PRGPIO_R & PortD)==0){};    // wait for Port D ready

  GPIO_PORTD_AFSEL_R |= PD310;              // PD3,PD1,PD0 alternate function
  GPIO_PORTD_DEN_R   |= PD310;              // digital enable
  GPIO_PORTD_PCTL_R  = (GPIO_PORTD_PCTL_R&0xFFFF0F00) | 0x00002022;
  // PD3=SSI1Tx(2), PD1=SSI1Fss(2), PD0=SSI1Clk(2)

  GPIO_PORTD_AMSEL_R &= ~PD310;             // disable analog

  SSI1_CR1_R = 0;                           // Disable SSI before config
  SSI1_CR1_R &= ~0x00000004;                // master

  // 5) Set clock rate
  // SSIClk = SysClk / (CPSDVSR * (1 + SCR))
  // Want ~5 MHz:
  // 80MHz / (2 * 8) = 5 MHz
  SSI1_CPSR_R = 2;                          // CPSDVSR = 2 (must be even 2-254)

  SSI1_CR0_R = 0;
  SSI1_CR0_R |= (7 << 8);                   // SCR = 7 → divisor = 8
  SSI1_CR0_R |= 0x000F;                     // 16-bit data
  SSI1_CR0_R |= 0x0000;                     // SPI Mode 0 (SPO=0, SPH=0)
  SSI1_CR0_R |= 0;                          // Freescale SPI
  
  SSI1_CR1_R |= 0x00000002;                 // Enable SSI

  DAC_Out(data);                            // Send initial value
}

// --------------     DAC_Out   --------------------------------------------
// Send data to TLV5616 12-bit DAC
// inputs:  voltage output (0 to 4095)
// 
void DAC_Out(uint16_t code){
    code &= 0x0FFF;                         // Clip to 12-bit

    // TLV5616 expects:
    // upper 4 bits = control (0x0)
    // lower 12 bits = data
    uint16_t frame = code;

    while((SSI1_SR_R & 0x00000002) == 0){}; // wait TX FIFO not full
    SSI1_DR_R = frame;

    while(SSI1_SR_R & 0x00000010){};        // wait until not busy
}

// --------------     DAC_OutNonBlocking   ------------------------------------
// Send data to TLV5616 12-bit DAC without checking for room in the FIFO
// inputs:  voltage output (0 to 4095)
// 
void DAC_Out_NB(uint16_t code){
  code &= 0x0FFF;
  SSI1_DR_R = code;  // Just send that john
}
