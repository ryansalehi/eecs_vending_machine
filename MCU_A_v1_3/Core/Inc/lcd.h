#ifndef LCD_H
#define LCD_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <string.h>
#include "cmsis_os.h"
#include "stdio.h"
#include "cbuf.h"
#include "font5x7.h"

/**
 * High level threading functions
 */
void LCD_Init(void);
void LCD_Render(void);

/**
 * Public API for using the LCD from other threads.
 * Any draw requests must be surrounded by Begin/EndFrame calls.
 */
void LCD_BeginFrame();
void LCD_EndFrame();
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t r, uint8_t g, uint8_t b);
void LCD_DrawImageRGB888(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *img);
void LCD_DrawText(uint16_t x, uint16_t y, const char *str, uint16_t color565, uint8_t scale);

/**
 * Test/specific-use functions
 */
void LCD_FillScreen(uint8_t r, uint8_t g, uint8_t b);
void LCD_DrawTestGradient(void);

#endif
