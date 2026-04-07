#include "receiver.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include <string.h>

#define RX_BUFFER_SIZE 50

extern UART_HandleTypeDef huart3;
extern osThreadId_t receiver_taskHandle;

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
        if (strncmp((char *)rx_buffer, "OPEN_LATCH\r", 11) == 0)
        {
            // TODO: OPEN THE LATCH!
        	// light up the LED for now as an indicator
        	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
			osDelay(3000);
			// Latch Closed
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
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
                if (receiver_taskHandle != NULL)
                {
                    vTaskNotifyGiveFromISR((TaskHandle_t)receiver_taskHandle, NULL);
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
