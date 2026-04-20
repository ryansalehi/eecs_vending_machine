#include "receiver.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include <string.h>
#include "main.h"
#include <stdint.h>
#include "statemachine.h"
#include <stdlib.h>
#include <stdbool.h>
#include "mbedtls/aes.h"

#define RX_BUFFER_SIZE 50

extern UART_HandleTypeDef huart3;
extern osThreadId_t UARTTaskHandle;

static mbedtls_aes_context aes;
static unsigned char key[16];      // 128-bit key (use 24 or 32 for AES-192/256)
static unsigned char output[32];   // encrypted result
static unsigned char iv[16];       // initialization vector (for CBC)



// ---  Buffer Architecture ---
static uint8_t rx_buffer[RX_BUFFER_SIZE];
static uint8_t rx_byte;
static uint16_t rx_index = 0;

// LOCK: 0 = Free for ISR to write, 1 = Task is busy processing
static volatile uint8_t buffer_locked = 0;

void UART_Init()
{
    for(int i = 0; i < sizeof(key); i++)
    {
        key[i] = i + 1;
        iv[i] = i + 1;
    }
}

// 1. Hardware Primer
void Receiver_StartHardwareListening(void)
{
    HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
}

// 2. The Consumer Task
void Receiver_Process(void)
{
    uint32_t notificationValue;

    // Sleep until the ISR wakes us up
    notificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    if (notificationValue > 0)
    {
        mbedtls_aes_init(&aes);
        unsigned char ivcopy[16];
        memcpy(ivcopy,iv,16);
        mbedtls_aes_setkey_dec(&aes, key, 128);
        mbedtls_aes_crypt_cbc(
            &aes,
            MBEDTLS_AES_DECRYPT,
            32,
            ivcopy,
            rx_buffer,
            output
        );

        // We own the buffer right now Process the message.
        if (strncmp((char*)output, "MC:", 3) == 0)
        {
            char*name = (char*)output + 3;
            SM_SetNewName(name);
            char*level = (char*)output + 30;
            int lev = atoi(level);
            SM_SetLevel(lev);
        }
        else if (strncmp((char*)output, "TOKEN_VALID", 11) == 0){
            SM_SetToken();
        }
        else if (strncmp((char*)output, "TOKEN_INVLD", 11) == 0){
            SM_SetInvalidToken();
        }
        mbedtls_aes_free(&aes);
    }
    // Clean up and unlock
	memset(rx_buffer, 0, RX_BUFFER_SIZE); // Zero out the data
	rx_index = 0;                         // Reset the typewriter carriage
	buffer_locked = 0;                    // UNLOCK: ISR is allowed to write again
}

// 3. The Producer ISR
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
    	whoami = rx_index;
        // Only accept data if the Task has unlocked the buffer!
        if (buffer_locked == 0)
        {
        	// Save the byte and move forward
			rx_buffer[rx_index] = rx_byte;
			rx_index++;

            if (rx_index == 32)
            {

                buffer_locked = 1;          // LOCK THE BUFFER so we don't overwrite it

                rx_index = 0;

                // WAKE TASK
                vTaskNotifyGiveFromISR((TaskHandle_t)UARTTaskHandle, NULL);
                whoami = 0;
            }
            else
            {

                // Prevent overflowing the array length
                if (rx_index >= RX_BUFFER_SIZE - 1) rx_index = 0;
            }
        }

        // If buffer_locked == 1, we drop the byte
        //TODO: better handling logic

        // Always re-arm the interrupt
        HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
    }

}
