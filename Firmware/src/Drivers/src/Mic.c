/**
 * @file Mic.c
 * @brief Microphone ADC input — PE3 / AIN0, ADC0 sequencer 3, software trigger.
 */

#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/Mic.h"

void Mic_Init(void) {
    volatile uint32_t d;
    SYSCTL_RCGCADC_R  |= 0x01;
    SYSCTL_RCGCGPIO_R |= 0x10;
    d = SYSCTL_RCGCGPIO_R; (void)d;

    GPIO_PORTE_DIR_R   &= ~0x08;
    GPIO_PORTE_AFSEL_R |=  0x08;
    GPIO_PORTE_DEN_R   &= ~0x08;
    GPIO_PORTE_AMSEL_R |=  0x08;

    ADC0_PC_R    = 0x01;
    ADC0_SSPRI_R = 0x0123;
    ADC0_ACTSS_R &= ~0x08;
    ADC0_EMUX_R  &= ~0xF000;
    ADC0_SSMUX3_R = 0;
    ADC0_SSCTL3_R = 0x06;
    ADC0_IM_R    &= ~0x08;
    ADC0_ACTSS_R |=  0x08;
}

uint16_t Mic_Read(void) {
    ADC0_PSSI_R = 0x08;
    while ((ADC0_RIS_R & 0x08) == 0) {}
    uint16_t val = ADC0_SSFIFO3_R & 0xFFF;
    ADC0_ISC_R = 0x08;
    return val;
}
