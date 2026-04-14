#ifndef NFC2_H_
#define NFC2_H_

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    NFC_OK = 0,
    NFC_NO_CARD,
    NFC_TIMEOUT,
    NFC_ERROR
} NFC_Result_t;

bool NFC_Init(void);
NFC_Result_t NFC_ReadUID(uint8_t *uid, uint8_t *uid_len, uint32_t timeout_ms);
void NFC_IrqFromISR(void);

#endif //NFC2_H_
