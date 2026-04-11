#include "sender.h"
#include "main.h"
#include <stdint.h>

// Tell this file that huart3 exists somewhere else (in main.c)
extern UART_HandleTypeDef huart3;

void UART_SendMessage(char* message)
{
    // size of payload, added 1 for zero terminator
    uint16_t msg_size = (uint16_t)strlen((char*)message) + 1;
    // Transmit directly over UART in blocking mode.
    
    HAL_UART_Transmit(&huart3, message, msg_size, 100);
}
