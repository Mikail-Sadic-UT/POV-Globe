#pragma once
// PORT-F  LaunchPAD switches and LEDS
//
#define PF0       (*((volatile uint32_t *)0x40025004))    // SW2
#define PF1       (*((volatile uint32_t *)0x40025008))    // RED LED
#define PF2       (*((volatile uint32_t *)0x40025010))    // BLUE LED
#define PF3       (*((volatile uint32_t *)0x40025020))    // GREEN LED
#define PF4       (*((volatile uint32_t *)0x40025040))    // SW1

// PORT-E
//
#define PE0       (*((volatile uint32_t *)0x40024004))
#define PE1       (*((volatile uint32_t *)0x40024008))
#define PE2       (*((volatile uint32_t *)0x40024010))
#define PE3       (*((volatile uint32_t *)0x40024020))    // Heartbeat LED
#define PE4       (*((volatile uint32_t *)0x40024040))
#define PE5       (*((volatile uint32_t *)0x40024080))

// PORT-D
//
#define PD0       (*((volatile uint32_t *)0x40007004))
#define PD1       (*((volatile uint32_t *)0x40007008))
#define PD2       (*((volatile uint32_t *)0x40007010))
#define PD3       (*((volatile uint32_t *)0x40007020))
#define PD6       (*((volatile uint32_t *)0x40007100))
#define PD7       (*((volatile uint32_t *)0x40007200))

// PORT-C Lab Board switches
//
#define PC4       (*((volatile uint32_t *)0x40006040))    // UP    and Logic Analyzer JP2-15
#define PC5       (*((volatile uint32_t *)0x40006080))    // RIGHT and Logic Analyzer JP2-13
#define PC6       (*((volatile uint32_t *)0x40006100))    // LEFT  and Logic Analyzer JP2-11
#define PC7       (*((volatile uint32_t *)0x40006200))    // DOWN  and Logic Analyzer JP2-9

// PORT-B
//
#define PB0       (*((volatile uint32_t *)0x40005004))     
#define PB1       (*((volatile uint32_t *)0x40005008))    // Logic Analyzer JP1-9
#define PB2       (*((volatile uint32_t *)0x40005010))
#define PB3       (*((volatile uint32_t *)0x40005020))
#define PB4       (*((volatile uint32_t *)0x40005040))     
#define PB5       (*((volatile uint32_t *)0x40005080))     
#define PB6       (*((volatile uint32_t *)0x40005100))     
#define PB7       (*((volatile uint32_t *)0x40005200))

// Switch definitions 

#define LEFT      PC6     // Switch on Lab Board
#define RIGHT     PC5     // Switch on Lab Board
#define UP        PC4     // Switch on Lab Board
#define DOWN      PC7     // Switch on Lab Board

#define MODE      PF4     // SW1 on the Launch Pad
#define SW1       PF4     // SW1 on the Launch Pad
#define SW2       PF0     // SW2 on the Launch Pad

// Prototypes for Port Inits

void Unified_Port_Init(void);

void Port_A_Init(void);
void Port_B_Init(void);
void Port_C_Init(void);
void Port_D_Init(void);
void Port_E_Init(void);
void Port_F_Init(void);

