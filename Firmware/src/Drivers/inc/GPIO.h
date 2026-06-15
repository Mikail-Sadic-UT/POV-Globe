// Initializes on-board LEDs (no interrupts)
void LED_Init(void);
 
// Initializes Port F with SW1/SW2 interrupt for motor control
void PF_Init(void);
 
// Heartbeat — cycles on-board LED through colours; call from SysTick
void LED_HBT(void);
 
// Initializes Port B PB1(UP) PB2(DOWN) PB3(SELECT) for LCD menu navigation
void PB_Init(void);
 
// Port B ISR — called automatically by NVIC; sets menu flags
void GPIOPortB_Handler(void);
 
// Read Port B buttons PB3-1, positive logic in bits 3-1
uint8_t PB_In(void);
