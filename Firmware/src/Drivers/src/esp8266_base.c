// ----------------------------------------------------------------
// File name:     esp8266.c
// Description:   This code is used to bridge the TM4C123 board 
//                and the MQTT Web Application via the ESP8266. 
//                It does not use the preferred FIFO interface.
//                You will need to convert it to the FIFO interface.
//
// Author:        Mark McDermott
// Date:          June 21, 2023
// Updated:       July 19, 2023
//
// -----------------------------------------------------------------

//#include <stdio.h>
#include <stdint.h>
//#include <string.h>			
//#include "inc/tm4c123gh6pm.h"

#include "../inc/UART.h"
#include "../inc/UART5.h"
#include "../inc/ST7735.h"
#include "../inc/Clock.h"
#include "../inc/esp8266.h"

#define DEBUG1                // First level of Debug
#define DEBUG2                // Second level of Debug
#define DEBUG3                // Third level of Debug

//#undef DEBUG1               // Comment out to enable DEBUG1
//#undef DEBUG2               // Comment out to enable DEBUG2
//#undef DEBUG3               // Comment out to enable DEBUG3

#define UART5_FR_TXFF            0x00000020  // UART Transmit FIFO Full
#define UART5_FR_RXFE            0x00000010  // UART Receive FIFO Empty
#define UART5_LCRH_WLEN_8        0x00000060  // 8 bit word length
#define UART5_LCRH_FEN           0x00000010  // UART Enable FIFOs
#define UART5_CTL_UARTEN         0x00000001  // UART Enable

#define PE0   (*((volatile uint32_t *)0x40024004))    // RDY from ESP 8266
#define PE1   (*((volatile uint32_t *)0x40024008))    // RST_B to ESP8266
#define PE2   (*((volatile uint32_t *)0x40024010))    // ACK to ESP8266
#define PE3   (*((volatile uint32_t *)0x40024020))    // LED to indicate that the 8266 connected

#define PF0   (*((volatile uint32_t *) 0x40025004))

#define RDY   PE0
#define RDY1  0x1


#define LOW   0x0

#define BIT0  0x1
#define BIT1  0x2
#define BIT2  0x4
#define BIT3  0x8   

//char  ssid[64]          = "utexas-iot";       // TODO: Update this with your WIFI SSID
//char  pass[64]      = "37198169097890651563";   // TODO: Update this with your WIFI Password

char    eid[32]           = "MFS2447";       // Your EID goes here
char    ssid[64]          = "26West-MyCampusNet-2G";
char    test[32]          = "poop";
char    pass[64]          = "Joemungus1234";
char    mqtt_broker[32]   = "broker.emqx.io";           // MQTT Broker in EER 5.826
// char    mqtt_broker[32]   = "test.mosquitto.org";           // MQTT Broker in EER 5.826
char    mqtt_port[8]      = "1883";

char    inchar;   

void DelayWait1ms(uint32_t n) {
    uint32_t volatile time;
    while (n){
        time = 72724 * 2 / 91;  // ~1msec
        while (time){
            --time;
        }
        --n;
    }
}

void Reset_8266(void) 
{
  ST7735_DrawString(1,1,"Reseting ESP ", ST7735_Color565(255, 63, 0));
  PE1 = LOW;              // Reset the 8266
  DelayWait1ms(5000);     // Wait for 8266 to reset
  PE1 = BIT1;             // Enable the 8266
  DelayWait1ms(5000);     // Wait before setting up 8266
  PE3 = LOW;              // Turn off LED

}

// ----------------------------------------------------------------------
// This routine sets up the Wifi connection between the TM4C and the
// hotspot. Enable the DEBUG flags if you want to watch the transactions.
// 

void SetupWiFi(void) 
{ 
  
  #ifdef DEBUG1
  UART_OutString("\r\nIn WiFI_Setup routine\r\n");
  UART_OutString("Waiting for RDY flag from ESP\r\n");
  #endif
  
  ST7735_DrawString(1,2,"Initializing WIFI ", ST7735_Color565(255, 0, 255 ));
  ST7735_DrawString(1,3,"RDY: ", ST7735_Color565(0, 64, 255 ));
  
  while (!RDY) {      // Wait for ESP8266 indicate it is ready for programming data
    #ifdef DEBUG1
    UART_OutString(".");
    #endif
    
    DelayWait1ms(300);
  }

  ST7735_DrawString(6,3,"Yes", ST7735_Color565(0, 255, 255 ));
  
  UART5_OutString(eid);           // Student EID - Used for individualizing MQTT Topics
  UART5_OutChar(',');
	
  UART5_OutString(ssid);          // Send WiFi SSID to ESP8266
  UART5_OutChar(',');
  
  UART5_OutString(pass);          // Send WiFi Password to ESP8266
  UART5_OutChar(',');      
  
  UART5_OutString(mqtt_broker);   // Send IP address of MQTT Broker   
  UART5_OutChar(',');       
  
  UART5_OutString(mqtt_port);     // Send MQTT port number  
  UART5_OutChar(',');             // Extra comma needed for ESP8266 parser code
  
  UART5_OutChar('\n');            // Send NL to indicate EOT   
  
    #ifdef DEBUG2
    
    UART_OutChar('\n');  
    UART_OutString(eid);    
    UART_OutChar(',');
    UART_OutString(ssid);
    UART_OutChar(',');
    UART_OutString(pass);
    UART_OutChar(',');
    UART_OutString(mqtt_broker);
    UART_OutChar(',');
    UART_OutString(mqtt_port);
    UART_OutChar(',');
    UART_OutChar('\n');      
    #endif
    
    #ifdef DEBUG3
    ST7735_DrawString(0,5,"SSID:", ST7735_Color565(255, 255, 255 ));
    ST7735_DrawString(0,6,ssid, ST7735_Color565(255, 255, 0 ));

    //ST7735_DrawString(0,5,pass, ST7735_Color565(255, 0, 0 ));

    ST7735_DrawString(0,7,"EID: ", ST7735_Color565(255, 255, 255 ));
    ST7735_DrawString(5,7,eid, ST7735_Color565(255, 255, 0 ));

    ST7735_DrawString(0,8,"Broker: ", ST7735_Color565(255, 255, 255 ));
    ST7735_DrawString(1,9,mqtt_broker, ST7735_Color565(255, 255, 0 ));

    ST7735_DrawString(0,10,"Port: ", ST7735_Color565(255, 255, 255 ));
    ST7735_DrawString(5,10,mqtt_port, ST7735_Color565(255, 255, 0 ));
    #endif
  
  //
  // This while loop receives debug info from the 8266 and optionally 
  // sends it out the debug port. The loop exits once the RDY signal
  // is deasserted and the serial port has no more character to xmit
  //
  /*
  while ((RDY) | ((UART5_FR_R & UART5_FR_RXFE) ==0)) { 
    
    if ((UART5_FR_R & UART5_FR_RXFE) ==0 ) 
    {
        inchar =(UART5_DR_R & 0xFF);      
    #ifdef DEBUG1
        UART_OutChar(inchar);                        // Output received character
    #endif
    }
    else if ((UART5_FR_R&UART5_FR_RXFE) !=0 ) { }     // Do nothing
    
  }
  */

  ST7735_DrawString(1,11,"Finished WiFi Setup", ST7735_Color565(0, 255, 0));

  ST7735_DrawString(0,13,"Connecting to WiFi...", ST7735_Color565(255, 255, 255));
 
  PE3 = BIT3;
  //DelayWait1ms(7000);
  
  #ifdef DEBUG1
  UART_OutString("Exiting WiFI_Setup routine\r\n");
  
  #endif
}


