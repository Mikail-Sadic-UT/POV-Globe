// -------------------------------------------------
// UART5.h
// Runs on TM4C123
// Simple device driver for the UART5 which is connected to ESP8266
//
// Author:      Daniel Valvano
// Date:        May 23, 2014
// Modified by: Mark McDermott
// Date:        June 21, 2018 
//
//------------------------------------------------------


#include <stdio.h>
#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"


// standard ASCII symbols
#define CR   0x0D
#define LF   0x0A
#define BS   0x08
#define ESC  0x1B
#define SP   0x20
#define DEL  0x7F

// initialize index FIFO

void UART5_Fifo_Init(uint32_t size);
	

//------------   UART5_InChar   ------------
// Wait for new serial port input
// Input: none
// Output: ASCII code for key typed
//
char UART5_InChar(void);


//------------UART5_OutChar------------
// Output 8-bit to serial port
// Input: letter is an 8-bit ASCII character to be transferred
// Output: none
//
void UART5_OutChar(char data);


//------------------- UART5_Init-------------------
// intializes uart and gpio needed to communicate with esp8266
// Configure UART5 for serial full duplex operation
// Inputs: none
// Outputs: none
void UART5_Init(void);

//----------UART5_FIFOtoBuffer----------
// - copies uart fifo to RXBuffer
// Inputs: none
// Outputs:none
void UART5_FIFOtoBuffer(void);


//----------UART5_GetString------------
// - gets data from software UART5 FIFO (in a string format)
// Inputs: pointer to empty buffer
// Outputs: Null terminated string
void UART5_GetString(unsigned char *bufPt);

//------------UART_InUDec------------
// InUDec accepts ASCII input in unsigned decimal format
//     and converts to a 32-bit unsigned number
//     valid range is 0 to 4294967295 (2^32-1)
// Input: none
// Output: 32-bit unsigned number
// If you enter a number above 4294967295, it will return an incorrect value
// Backspace will remove last digit typed
uint32_t UART5_InUDec(void);

//-----------------------UART_OutUDec-----------------------
// Output a 32-bit number in unsigned decimal format
// Input: 32-bit number to be transferred
// Output: none
// Variable format 1-10 digits with no space before or after
void UART5_OutUDec(uint32_t n);

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
uint32_t UART5_InUHex(void);

//--------------------------UART_OutUHex----------------------------
// Output a 32-bit number in unsigned hexadecimal format
// Input: 32-bit number to be transferred
// Output: none
// Variable format 1 to 8 digits with no space before or after
void UART5_OutUHex(uint32_t number);


//------------UART_OutString------------
// Output String (NULL termination)
// Input: pointer to a NULL-terminated string to be transferred
// Output: none
void UART5_OutString(char *pt);


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
void UART5_InString(char *bufPt, uint16_t max);


void Test_UART5(void);

