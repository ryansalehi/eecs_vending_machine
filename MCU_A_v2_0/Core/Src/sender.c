#include "sender.h"
#include "main.h"

// Tell this file that huart3 exists somewhere else (in main.c)
extern UART_HandleTypeDef huart3;

void DoorComms_SendTestMessage(void)
{
    // The test payload
    uint8_t test_msg[] = "OPEN_LATCH\r\n";

    // Transmit directly over UART in blocking mode.
    // (Note: The 100ms timeout means the ISR will block for up to 100ms if the hardware hangs,
    // but for a quick test, this is perfectly fine).
    HAL_UART_Transmit(&huart3, test_msg, sizeof(test_msg) - 1, 100);
}
