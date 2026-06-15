// UART.h
// Runs on LM3S811, LM3S1968, LM3S8962, LM4F120, TM4C123
// Simple device driver for the UART.
// Daniel Valvano
// Jan 3, 2020
// Modified by EE345L students Charlie Gough && Matt Hawk
// Modified by EE345M students Agustinus Darmawan && Mingjie Qiu

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2020
   Program 4.12, Section 4.9.4, Figures 4.26 and 4.40

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

// U0Rx (VCP receive) connected to PA0
// U0Tx (VCP transmit) connected to PA1
#ifndef UART_H
#define UART_H
#include <stdint.h>

// standard ASCII symbols
#define CR   0x0D
#define LF   0x0A
#define BS   0x08
#define TAB  0x09
#define ESC  0x1B
#define SP   0x20
#define DEL  0x7F
#define CTRL_C  0x03
#define CTRL_S  0x13

//------------UART_Init------------
// Initialize the UART for 115,200 baud rate (assuming 80 MHz clock),
// 8 bit word length, no parity bits, one stop bit, FIFOs enabled
// Input: none
// Output: none
void UART_Init(void);

//------------UART_InChar------------
// Wait for new serial port input
// Input: none
// Output: ASCII code for key typed
char UART_InChar(void);

//------------UART_InCharNonBlock------------
// input ASCII character from UART
// output: 0 if RxFifo is empty
//         character if
char UART_InCharNonBlock(void);

//------------UART_OutChar------------
// Output 8-bit to serial port
// Input: letter is an 8-bit ASCII character to be transferred
// Output: none
void UART_OutChar(char data);

//------------UART_OutCharNonBlock------------
// non blocking output ASCII character to UART
// Input: letter is an 8-bit ASCII character to be transferred
// Output: none
// Error: return with lost data if TxFifo is full
void UART_OutCharNonBlock(char data);

//------------UART_OutString------------
// Output String (NULL termination)
// Input: pointer to a NULL-terminated string to be transferred
// Output: none
void UART_OutString(char *pt);

//------------UART_InUDec------------
// InUDec accepts ASCII input in unsigned decimal format
//     and converts to a 32-bit unsigned number
//     valid range is 0 to 4294967295 (2^32-1)
// Input: none
// Output: 32-bit unsigned number
// If you enter a number above 4294967295, it will return an incorrect value
// Backspace will remove last digit typed
uint32_t UART_InUDec(void);

//-----------------------UART_OutUDec-----------------------
// Output a 32-bit number in unsigned decimal format
// Input: 32-bit number to be transferred
// Output: none
// Variable format 1-10 digits with no space before or after
void UART_OutUDec(uint32_t n);


/**
 * @details  Output a 32-bit number in signed decimal format
 * @param  n 32-bit number to be transferred
 * @note Variable format 1-10 digits with no space before or after
 * @return none
 * @brief  Number serial output.
 */
void UART_OutSDec(long n);

//---------------------UART_InUHex----------------------------------------
// Accepts ASCII input in unsigned hexadecimal (base 16) format
// Input: none
// Output: 32-bit unsigned number
// No '$' or '0x' need be entered, just the 1 to 8 hex digits
// It will convert lower case a-f to uppercase A-F
//     and converts to a 16 bit unsigned number
//     value range is 0 to FFFFFFFF
// If you enter a number above FFFFFFFF, it will return an incorrect value
// Backspace will remove last digit typed
uint32_t UART_InUHex(void);

//--------------------------UART_OutUHex----------------------------
// Output a 32-bit number in unsigned hexadecimal format
// Input: 32-bit number to be transferred
// Output: none
// Variable format 1 to 8 digits with no space before or after
void UART_OutUHex(uint32_t number);

//------------UART_InString------------
// Accepts ASCII characters from the serial port
//    and adds them to a string until <enter> is typed
//    or until max length of the string is reached.
// It echoes each character as it is inputted.
// If a backspace is inputted, the string is modified
//    and the backspace is echoed
// terminates the string with a null character
// uses busy-waiting synchronization on RDRF
// Input: pointer to empty buffer, size of buffer
// Output: Null terminated string
// -- Modified by Agustinus Darmawan + Mingjie Qiu --
void UART_InString(char *bufPt, uint16_t max);
/**
 * @details  Output a 32-bit number in 0.01 fixed-point format
 * @param  number 32-bit signed number to be transferred, -99999 to +99999
 * @note Fixed format, always 7 characters <br>
  12345 to " 123.45"  <br>
  -22100 to "-221.00"<br>
  -102 to "  -1.02" <br>
     31 to "   0.31" <br>
  error     " ***.**" <br>  
 * @return none
 * @brief  Fixed-point output.
 */
void UART_Fix2(long number);
//********UART_OutUFix1*****************
// Output a 16-bit number in unsigned 4-digit fixed point, 0.1 resolution
// numbers 0 to 9999 printed as "  0.0" to "999.9"
// Inputs: n  16-bit unsigned number
// Outputs: none
void UART_OutUFix1(uint16_t n);
//------------Output_Init------------
// Initialize the UART for 115,200 baud rate (assuming 80 MHz bus clock),
// 8 bit word length, no parity bits, one stop bit, FIFOs enabled
// Input: none
// Output: none
void Output_Init(void);

#endif
