// ----------------------------------------------------------------------------
//
// TLV5616.h
// Runs on TM4C123
// Use SSI1 to send a 16-bit code to the TLV5616.
// Daniel Valvano
// Jonathan W. Valvano 10/11/14
//
// ----------------------------------------------------------------------------

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to ARM Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2014

 Copyright 2011 by Jonathan W. Valvano, valvano@mail.utexas.edu
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

// SSIClk (SCLK)  connected to PD0
// SSIFss (FS)    connected to PD1
// SSITx  (DIN)   connected to PD3
#ifndef __TLV5616_H__
#define __TLV5616_H__
#include <stdint.h>

// --------------     DAC_Init     --------------------------------------------
//
// Initialize TLV5616 12-bit DAC
// assumes bus clock is 80 MHz
// inputs: initial voltage output (0 to 4095)
// outputs:none
//
void DAC_Init(uint16_t data);

// --------------      DAC_Out     --------------------------------------------
//
// Send data to TLV5616 12-bit DAC
// inputs:  voltage output (0 to 4095)
// outputs: none
//
void DAC_Out(uint16_t code);

   // -----------------   DAC_OutNonBlocking      --------------------------------
//
// Send data to TLV5616 12-bit DAC without
// waiting for a response.  This is useful
// in audio applications where DAC outputs
// are relatively infrequent and where the
// DAC response is meaningless.  Since the
// state of the FIFO is not checked before
// sending, data may be lost.
// inputs:  voltage output (0 to 4095)
// outputs: none
//
#define DAC_OutNonBlocking(code) SSI1_DR_R = code;     


#endif
