#ifndef LCD_H
#define LCD_H

#include <stdint.h>

// Self explanatory

void LCD_Menu_Init(void);

void buttonHandler(void);

void Menu_Up(void);

void Menu_Down(void);

void Menu_Select(void);

extern volatile uint8_t btn_up;
extern volatile uint8_t btn_down;
extern volatile uint8_t btn_select;

#endif 
