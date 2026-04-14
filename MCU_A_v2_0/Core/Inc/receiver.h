#ifndef RECEIVER_H
#define RECEIVER_H

#include <stdint.h>

void UART_Init();
uint32_t UART_GetLastHeartbeat();

/* * 1. Call this in main.c in USER CODE BEGIN 2
 * This arms the UART hardware to catch the very first byte.
 */
void Receiver_StartHardwareListening(void);


/* * 2. Call this inside the infinite loop of your CubeMX-generated task
 * in freertos.c. This contains the actual FreeRTOS sleep/wake logic.
 */
void Receiver_Process(void);

#endif /* RECEIVER_H */
