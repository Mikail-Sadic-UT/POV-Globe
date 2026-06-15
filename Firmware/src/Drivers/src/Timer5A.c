// Timer5A.c
// Runs on LM4F120/TM4C123
// Use Timer5 in 32-bit periodic mode to request interrupts at a periodic rate
// Daniel Valvano
// Jan 1, 2020

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2020
  Program 7.5, example 7.6

 Copyright 2020 by Jonathan W. Valvano, valvano@mail.utexas.edu
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
#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"

void (*PeriodicTask5)(void);   // user function

// ***************** Timer5A_Init ****************
// Activate Timer5 interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in units (1/clockfreq)
//          priority 0 (highest) to 7 (lowest)
// Outputs: none
void Timer5A_Init2(void(*task)(void), uint32_t period, uint32_t priority){
  SYSCTL_RCGCTIMER_R |= 0x20;   // 0) activate TIMER5
  PeriodicTask5 = task;         // user function
  TIMER5_CTL_R = 0x00000000;    // 1) disable TIMER5A during setup
  TIMER5_CFG_R = 0x00000000;    // 2) configure for 32-bit mode
  TIMER5_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
  TIMER5_TAILR_R = period-1;    // 4) reload value
  TIMER5_TAPR_R = 0;            // 5) bus clock resolution
  TIMER5_ICR_R = 0x00000001;    // 6) clear TIMER5A timeout flag
  TIMER5_IMR_R = 0x00000001;    // 7) arm timeout interrupt
  NVIC_PRI23_R = (NVIC_PRI23_R&0xFFFFFF00)|(priority<<5); // priority 
// interrupts enabled in the main program after all devices initialized
// vector number 108, interrupt number 92
  NVIC_EN2_R = 1<<28;           // 9) enable IRQ 92 in NVIC
  TIMER5_CTL_R = 0x00000001;    // 10) enable TIMER5A
}

void Timer5A_Handler2(void){
  TIMER5_ICR_R = TIMER_ICR_TATOCINT;// acknowledge TIMER5A timeout
  (*PeriodicTask5)();               // execute user task
}
void Timer5_Stop2(void){
  NVIC_DIS2_R = 1<<28;          // 9) disable interrupt 92 in NVIC
  TIMER5_CTL_R = 0x00000000;    // 10) disable timer5A
}

void disable_send2(void){
  NVIC_DIS2_R = 1<<28;          // 9) disable interrupt 92 in NVIC
}

void enable_send2(void){
  NVIC_EN2_R = 1<<28;           // 9) enable IRQ 92 in NVIC
}
