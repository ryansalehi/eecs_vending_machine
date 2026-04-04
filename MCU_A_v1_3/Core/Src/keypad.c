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
#define KEYPAD_GPIO_DIR1  (0x23)

volatile static bool key_press = false;
extern I2C_HandleTypeDef hi2c1;

static cbuf_t keypad_events;

typedef struct {
	uint8_t keycode;
	uint8_t pressed;
	char decoded;
} Event_t;

Event_t events[10];

char keymap[4][4] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};

static void KEYPAD_Write(uint8_t reg, uint8_t data)
{
    HAL_I2C_Mem_Write(&hi2c1, KEYPAD_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
}

static uint8_t KEYPAD_Read(uint8_t reg)
{
    uint8_t data;
    HAL_I2C_Mem_Read(&hi2c1, KEYPAD_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
    return data;
}

static void KEYPAD_DecodeReading(Event_t* event)
{
	uint8_t row = event->keycode / 10;
	uint8_t col = event->keycode % 10 - 1;
	if(row > 3 || col > 3)
	{
		event->decoded = 'X'; // invalid reading
	}
	else
	{
		event->decoded = keymap[row][col];
	}
}

void KEYPAD_Init()
{
	// Reset device
	KEYPAD_Write(KEYPAD_CFG, 0x80);
    osDelay(10);

	// Enable keypad / GPIO
	// bit 0: auto increment
	// bit 4: key event interrupt enable
	KEYPAD_Write(KEYPAD_CFG, 0x11);

	// configure rows/cols
	KEYPAD_Write(KEYPAD_GPIO_DIR1, 0x0F); // all are inputs
	KEYPAD_Write(0x1D, 0x0F); // add rows to matrix
	KEYPAD_Write(0x1E, 0x0F); // add cols to matrix
	KEYPAD_Write(0x1F, 0x0F); // GPI_EM1 enable key events
    KEYPAD_Write(0x20, 0x0F); // GPI_EM2 enable key events

	// clear interrupts
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
		key_press = false;
		uint8_t count = KEYPAD_Read(KEYPAD_EVENT_COUNT) & 0x0F; // count is only the bottom 4 bits 

		while (count--)
		{
			uint8_t key = KEYPAD_Read(KEYPAD_EVENT_A);

			Event_t new_event;
			new_event.keycode = key & 0x7F;
            new_event.pressed = (key & 0x80) >> 7; // MSB: 0 if button released, 1 if button pressed
			KEYPAD_DecodeReading(&new_event);

			// only log button presses (ignore release events)
			if(new_event.pressed == 1)
			{
				cbuf_push_overwrite(&keypad_events, &new_event);
			}
		}
		
		KEYPAD_Write(KEYPAD_INT_STAT, 0xFF);
	}
}

