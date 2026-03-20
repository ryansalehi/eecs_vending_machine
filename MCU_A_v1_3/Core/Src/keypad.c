#include "keypad.h"
#include "lcd.h"
#include "stdint.h"
#include "stdbool.h"
static bool new_event = false;
static uint32_t num = 0;

void KEYPAD_IrqFromIsr()
{
	new_event = true;
}

void KEYPAD_ReadAnyPresses()
{
	if(new_event)
	{
		LCD_BeginFrame();
		// TODO: read from board over I2C
		char str[2] = {num + '0', '\0'};
		LCD_DrawText(150, 290, str, 155, 2);
		num++;
		new_event = false;
		LCD_EndFrame();
	}
}

