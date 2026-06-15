/**
 * @file GPIO.c
 * @brief Port F (motor buttons) and Port B (LCD buttons) GPIO driver.
 *
 * Port F:
 *   PF0 — motor spinup  (falling edge, pull-up)
 *   PF4 — motor spindown (falling edge, pull-up)
 *   PF1-3 — RGB LEDs (heartbeat)
 *
 * Port B:
 *   PB1 — LCD Up
 *   PB2 — LCD Down
 *   PB3 — LCD Select
 */

#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/Motor.h"

#define LEDS      (*((volatile uint32_t *)0x40025038))
#define RED       0x02
#define BLUE      0x04
#define GREEN     0x08

#define PortF   0x20
#define PortB   0x02

#define UNLOCK  0x4C4F434B
#define PF4_0   0x1F
#define PU      0x11
#define IN_OUT  0x0E
#define PF4     (1 << 4)
#define PF0     (1)

#define DEBOUNCE_MS     100
#define DEBOUNCE_MS_LCD 150

// For fancy HBT :P
static const int32_t COLORWHEEL[7] = { RED, RED+GREEN, GREEN, GREEN+BLUE, BLUE, BLUE+RED, RED+GREEN+BLUE };

extern uint32_t msTick;
extern volatile uint32_t lcd_last_press;

void Menu_Up(void);
void Menu_Down(void);
void Menu_Select(void);

void LED_Init(void) {
    volatile uint32_t delay;
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGC2_GPIOF;
    (void)delay;
    GPIO_PORTF_DIR_R   = 0x0E;
    GPIO_PORTF_AFSEL_R &= ~0x0E;
    GPIO_PORTF_DEN_R   |= 0x0E;
    GPIO_PORTF_PCTL_R  = 0;
    GPIO_PORTF_AMSEL_R = 0;
    LEDS = 0;
}

void PF_Init(void) {
    SYSCTL_RCGCGPIO_R |= PortF;
    while ((SYSCTL_PRGPIO_R & PortF) == 0) {}
    GPIO_PORTF_LOCK_R  = UNLOCK;
    GPIO_PORTF_CR_R    = PF4_0;
    GPIO_PORTF_DIR_R   = IN_OUT;
    GPIO_PORTF_DR2R_R  = 0x0E;
    GPIO_PORTF_PUR_R   = PU;
    GPIO_PORTF_DEN_R   = PF4_0;
    GPIO_PORTF_DATA_R  = 0x0E;

    GPIO_PORTF_IS_R  &= ~(PF4 | PF0);
    GPIO_PORTF_IBE_R &= ~(PF4 | PF0);
    GPIO_PORTF_IEV_R &= ~(PF4 | PF0);
    GPIO_PORTF_ICR_R  =  (PF4 | PF0);
    GPIO_PORTF_IM_R  |=  (PF4 | PF0);

    NVIC_PRI7_R  = (NVIC_PRI7_R & 0x00FFFFFF) | 0xA0000000; /* pri 5 */
    NVIC_EN0_R  |= (1 << 30);
    LEDS = 0;
}

static uint8_t idx = 0;

void LED_HBT(void) {        // cycle
    LEDS = COLORWHEEL[idx];
    idx = (idx + 1) % 7;
}

uint8_t PF_In(void) {
    uint32_t data = ~GPIO_PORTF_DATA_R;
    return (uint8_t)(((data & 0x10) >> 3) | (data & 0x01));
}

volatile uint32_t last_press = 0;
volatile uint8_t  PF4_High   = 0;
volatile uint8_t  PF0_High   = 0;

void GPIOPortF_Handler(void) {
    GPIO_PORTF_ICR_R = PF4_0;
    if ((msTick - last_press) < DEBOUNCE_MS) return;
    last_press = msTick;

    uint8_t sw = PF_In();
    if (sw & 0x01) { PF0_High = 1; Motor_Spinup(MOTOR_SPINUP_MS, MOTOR_SETPOINT); }
    if (sw & 0x02) { PF4_High = 1; Motor_Spindown(MOTOR_SPINDOWN_MS); }
}

void PB_Init(void) {
    SYSCTL_RCGCGPIO_R |= PortB;
    while ((SYSCTL_PRGPIO_R & PortB) == 0) {}

    GPIO_PORTB_DIR_R   &= ~0x0E;
    GPIO_PORTB_AFSEL_R &= ~0x0E;
    GPIO_PORTB_DEN_R   |=  0x0E;
    GPIO_PORTB_PCTL_R  &= ~0x0000FFF0;
    GPIO_PORTB_AMSEL_R  =  0;
    GPIO_PORTB_PUR_R   |=  0x0E;

    GPIO_PORTB_IS_R  &= ~0x0E;
    GPIO_PORTB_IBE_R &= ~0x0E;
    GPIO_PORTB_IEV_R &= ~0x0E;
    GPIO_PORTB_ICR_R  =  0x0E;
    GPIO_PORTB_IM_R  |=  0x0E;

    NVIC_EN0_R  |= (1 << 1);
    NVIC_PRI0_R  = (NVIC_PRI0_R & 0xFFFF00FF) | 0x0000A000; /* pri 5 */
}

uint8_t PB_In(void) { return (~GPIO_PORTB_DATA_R & 0x0E); }

void GPIOPortB_Handler(void) {
    GPIO_PORTB_ICR_R = 0x0E;
    if ((msTick - lcd_last_press) < DEBOUNCE_MS_LCD) return;
    lcd_last_press = msTick;

    uint8_t pb = ~GPIO_PORTB_DATA_R & 0x0E;
    if (pb & 0x02) Menu_Up();
    if (pb & 0x04) Menu_Down();
    if (pb & 0x08) Menu_Select();
}
