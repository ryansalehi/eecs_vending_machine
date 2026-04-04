#include "keypad.h"
#include "lcd.h"
#include "stdint.h"
#include "stdbool.h"
#include "ctype.h"
#include "stdlib.h"
#include "cbuf.h"

#define KEYPAD_ADDR (0x34 << 1)

// Registers
#define KEYPAD_CFG (0x01)
#define KEYPAD_INT_STAT (0x02)
#define KEYPAD_EVENT_COUNT (0x03)
#define KEYPAD_EVENT_A (0x04)
#define KEYPAD_GPIO_DIR1  (0x23)

extern I2C_HandleTypeDef hi2c1;

volatile static bool key_press = false;
static bool reading_active = false;
static cbuf_t keypad_events;
static osSemaphoreId_t keypadReadSemaphore; // Used for signalling between reading and processing
static osMutexId_t keypadCbufMutex; // Prevents reading and processing at the same time

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
	keypadReadSemaphore = osSemaphoreNew(1, 0, NULL);
    configASSERT(keypadReadSemaphore != NULL);
    keypadCbufMutex = osMutexNew(NULL);
}

void KEYPAD_IrqFromIsr()
{
	key_press = true;
}

void KEYPAD_ReadAnyPresses()
{
	if(key_press && reading_active)
	{
		osMutexAcquire(keypadCbufMutex, portMAX_DELAY);
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
				osSemaphoreRelease(keypadReadSemaphore);
			}
		}
		
		KEYPAD_Write(KEYPAD_INT_STAT, 0xFF);
		osMutexRelease(keypadCbufMutex);
	}
}

bool KEYPAD_PromptClassNumber(Class_t* out)
{
	if(!out)
	{
		return false;
	}
	memset(out->number_string, 0, sizeof(out->number_string));
	out->number_int = 0;

	// Clear out the circular buffer
	osMutexAcquire(keypadCbufMutex, portMAX_DELAY);
	cbuf_clear(&keypad_events);
	osMutexRelease(keypadCbufMutex);

	LCD_BeginFrame();
	LCD_FillRect(0, 70, 480, 200, 228, 228, 228);
	LCD_DrawText(15, 140, "enter your class number", 155, 3);
	LCD_EndFrame();

	char running_class_number[11] = {};
	int running_idx = 0;

	while(true)
	{
		reading_active = true;
		volatile osStatus_t state = osSemaphoreAcquire(keypadReadSemaphore, portMAX_DELAY);
		if(osOK == state)
		{
			osMutexAcquire(keypadCbufMutex, portMAX_DELAY);
			Event_t most_recent;
			if(!cbuf_peek_back(&keypad_events,&most_recent))
			{
				// Keep reading if buffer is empty (this should never happen)
				osMutexRelease(keypadCbufMutex);
				continue;
			}
			if(most_recent.decoded == '*')
			{
				// * means delete
				Event_t delete_event;
				cbuf_pop_back(&keypad_events,&delete_event); // delete the *
				cbuf_pop_back(&keypad_events,&delete_event); // delete the previous number
			}
			else if(most_recent.decoded == '#')
			{
				// # means "I'm done entering the number"
				Event_t delete_event;
				cbuf_pop_back(&keypad_events,&delete_event); // delete the #
				if(cbuf_size(&keypad_events) != 3)
				{
					// Class numbers must be 3 chars
					reading_active = false;
					osMutexRelease(keypadCbufMutex);
					return false;
				}
				for(size_t i = 0; i < 3; ++i)
				{
					Event_t class_num;
					if(!cbuf_pop(&keypad_events, &class_num))
					{
						// failed to read class digit
						reading_active = false;
						osMutexRelease(keypadCbufMutex);
						return false;
					}
					if(!isdigit(class_num.decoded))
					{
						// must be a digit
						reading_active = false;
						osMutexRelease(keypadCbufMutex);
						return false;
					}
					out->number_string[i] = class_num.decoded;
				}

				// Reading was successful at this point
				out->number_string[3] = '\0'; //ensure null-terminated
				out->number_int = atoi(out->number_string);
				reading_active = false;
				osMutexRelease(keypadCbufMutex);
				return true;
			}
			else
			{
				running_class_number[running_idx++] = most_recent.decoded;
				LCD_BeginFrame();
				LCD_FillRect(0, 70, 480, 200, 228, 228, 228);
				LCD_DrawText(15, 140, "entering:", 155, 3);
				LCD_DrawText(300, 140, running_class_number, 155, 3);
				LCD_EndFrame();
			}
			osMutexRelease(keypadCbufMutex);
		}
	}
}

