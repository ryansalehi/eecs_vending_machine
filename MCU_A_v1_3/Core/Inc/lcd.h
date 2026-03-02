#ifndef LCD_H
#define LCD_H

#include "stm32f4xx_hal.h"

void LCD_Init(void);
void LCD_FillScreen(uint8_t r, uint8_t g, uint8_t b);

#endif
