#include "receiver.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include <string.h>
#include "main.h"
#include <stdint.h>
#include "statemachine.h"
#include <stdlib.h>

#define RX_BUFFER_SIZE 50

extern UART_HandleTypeDef huart3;
extern osThreadId_t UARTTaskHandle;

// ---  Buffer Architecture ---
static uint8_t rx_buffer[RX_BUFFER_SIZE];
static uint8_t rx_byte;
static uint16_t rx_index = 0;

// LOCK: 0 = Free for ISR to write, 1 = Task is busy processing
static volatile uint8_t buffer_locked = 0;


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
        // We own the buffer right now Process the message.
        if (strncmp((char *)rx_buffer, "MCARD:", 6) == 0)
        {
            char*name = rx_buffer + 6;
            SM_SetNewName(name);
        }
        if (strncmp((char *)rx_buffer, "LEVEL:", 6) == 0)
        {
            char*level = rx_buffer + 6;
            int lev = atoi(level);
            SM_SetLevel(lev);
        }

        // Clean up and unlock
        memset(rx_buffer, 0, RX_BUFFER_SIZE); // Zero out the data
        rx_index = 0;                         // Reset the typewriter carriage
        buffer_locked = 0;                    // UNLOCK: ISR is allowed to write again
    }
}

// 3. The Producer ISR
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        // Only accept data if the Task has unlocked the buffer!
        if (buffer_locked == 0)
        {
            if (rx_byte == '\n')
            {
                rx_buffer[rx_index] = '\0'; // Null-terminate

                buffer_locked = 1;          // LOCK THE BUFFER so we don't overwrite it

                // WAKE TASK
                if (UARTTaskHandle != NULL)
                {
                    vTaskNotifyGiveFromISR((TaskHandle_t)UARTTaskHandle, NULL);
                }
            }
            else
            {
                // Save the byte and move forward
                rx_buffer[rx_index] = rx_byte;
                rx_index++;

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
