#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>

extern volatile uint32_t delay;

/* Call these from button handlers / menu to switch display mode at runtime */
void SetMode_8bit(const char *filename, const uint8_t palette[][3]);
void SetMode_4bit(const char *filename, const uint8_t palette[][3]);

#endif /* MAIN_H */
