/**
 * @file Mic.h
 * @brief Microphone ADC input — PE3 / AIN0, software-triggered single sample
 */

#ifndef MIC_H
#define MIC_H

#include <stdint.h>

void Mic_Init(void);

uint16_t Mic_Read(void);

#endif /* MIC_H */
