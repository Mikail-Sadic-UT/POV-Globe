// ----------------------------------------------------------------------------
//
// File name: MQTT.c
//
// Description: This code is used to bridge the TM4C123 board and the MQTT Web Application
//              via the ESP8266 WiFi board

// Authors:       Mark McDermott
// Orig gen date: June 3, 2023
// Last update:   July 21, 2023
//
// ----------------------------------------------------------------------------


//#include <stdio.h>
#include <stdint.h>
#include <string.h>
//#include <stdlib.h>
#include "../inc/tm4c123gh6pm.h"
//#include "ST7735.h"
#include "../inc/PLL.h"

#include "../inc/UART.h"
#include "../inc/UART5.h"
#include "../inc/esp8266.h"

#include "../inc/FIFOO.h"

//#include "Dump.h"

#define DEBUG1                // First level of Debug
//#undef  DEBUG1                // Comment out to enable Debug1

#define UART5_FR_TXFF            0x00000020  // UART Transmit FIFO Full
#define UART5_FR_RXFE            0x00000010  // UART Receive FIFO Empty
#define UART5_LCRH_WLEN_8        0x00000060  // 8 bit word length
#define UART5_LCRH_FEN           0x00000010  // UART Enable FIFOs
#define UART5_CTL_UARTEN         0x00000001  // UART Enable

// ----   Function Prototypes not defined in header files  ------
// 
void EnableInterrupts(void);    // Defined in startup.s

// ----------   VARIABLES  ----------------------------

//extern uint32_t         Kp1; //Extern for any variables we need to set/get for MQTT code
//extern uint32_t         Kp2;
//extern uint32_t         Ki1;
//extern uint32_t         Ki2;
//extern uint32_t         Xstar;
uint32_t example_left_motor_speed;

//Buffers for send / recv
char                    input_char;
char                    b2w_buf[64];
char                    w2b_buf[128];
//static uint32_t         bufpos          = 0;

// --------------------------     W2B Parser      ------------------------------
//
// This parser decodes and executes commands from the MQTT Web Appication 
//
void Parser(void) { // did not use this
  //Perform operations on the w2b buffer to set variables as appropriate
	//atoi() and strtok() may be useful
	
	char first_token[8];
	strcpy(first_token, strtok(w2b_buf, ","));
	
	#ifdef DEBUG1
		UART_OutString("Token 1: ");
		UART_OutString(first_token);
		UART_OutString("\r\n");
	#endif
}

// -----------------------  TM4C_to_MQTT Web App -----------------------------
// This routine publishes clock data to the
// MQTT Web Application via the MQTT Broker
// The data is sent using CSV format:
//
// ----------------------------------------------------------------------------
//    
//    Convert this routine to use a FIFO
//
// 
extern FIFO_t FIFO_OUT; 
extern uint8_t MQTT_Send;
void TM4C_to_MQTT(void){ 
  MQTT_Send = 1;  // just setting flag for now
}
 
// -------------------------   MQTT_to_TM4C  -----------------------------------
// This routine receives the command data from the MQTT Web App and parses the
// data and feeds the commands to the TM4C.
// -----------------------------------------------------------------------------
//
//    Convert this routine to use a FIFO
//
// 
extern FIFO_t FIFO_IN;
void MQTT_to_TM4C(void) {

  //JitterMeasure();  // more stupid ass report stuff

  while ((UART5_FR_R & UART5_FR_RXFE) == 0) { // Empty RXD buffer into Fifo
    char data = (UART5_DR_R & 0xFF);
    FIFO_Put(&FIFO_IN, data);
    if(FIFO_Full(&FIFO_IN)) break;
  } 
}
