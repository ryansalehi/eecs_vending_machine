#include "keypad.h"
#include "lcd.h"
#include "stdint.h"
#include "stdbool.h"
#include "cbuf.h"

#define KEYPAD_ADDR (0x34 << 1)

// Registers
#define KEYPAD_CFG (0x01)
#define KEYPAD_INT_STAT (0x02)
#define KEYPAD_EVENT_COUNT (0x03)
#define KEYPAD_EVENT_A (0x04)

volatile static bool key_press = false;
static uint32_t num = 0;

extern I2C_HandleTypeDef hi2c1;

static cbuf_t keypad_events;

typedef struct {
	uint8_t keycode;
	uint8_t pressed;
} Event_t;

Event_t events[10];

void KEYPAD_Write(uint8_t reg, uint8_t data)
{
    HAL_I2C_Mem_Write(&hi2c1, KEYPAD_ADDR, reg, 1, &data, 1, HAL_MAX_DELAY);
}

uint8_t KEYPAD_Read(uint8_t reg)
{
    uint8_t data;
    HAL_I2C_Mem_Read(&hi2c1, KEYPAD_ADDR, reg, 1, &data, 1, HAL_MAX_DELAY);
    return data;
}

void KEYPAD_Init()
{
	
	KEYPAD_Write(KEYPAD_CFG, 0x01);

	KEYPAD_Write(0x1D, 0x0F); // rows
	KEYPAD_Write(0x1E, 0x0F); // cols

	KEYPAD_Write(KEYPAD_INT_STAT, 0xFF);

	cbuf_init(&keypad_events, events, sizeof(events)/sizeof(Event_t), sizeof(Event_t));
}

void KEYPAD_IrqFromIsr()
{
	key_press = true;
}

void KEYPAD_ReadAnyPresses()
{
	if(key_press)
	{
		uint8_t count = KEYPAD_Read(KEYPAD_EVENT_COUNT) & 0x0F; // mask away upper bits

		while (count--)
		{
			uint8_t key = KEYPAD_Read(KEYPAD_EVENT_A);

			Event_t new_event;

			new_event.keycode = key & 0x7F;
            new_event.pressed = (key & 0x80) >> 7;

			// do stuff
			cbuf_push(&keypad_events, &new_event);

		}
		
		KEYPAD_Write(KEYPAD_INT_STAT, 0xFF);
		key_press = false;
	}
}

