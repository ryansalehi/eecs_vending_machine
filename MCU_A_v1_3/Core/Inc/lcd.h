#ifndef LCD_H
#define LCD_H

#include "stm32f4xx_hal.h"

void LCD_Init(void);
void LCD_FillScreen(uint8_t r, uint8_t g, uint8_t b);
void LCD_DrawTestGradient(void);
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void LCD_DrawImageRGB888(const uint8_t *img, uint16_t w, uint16_t h);


#endif
