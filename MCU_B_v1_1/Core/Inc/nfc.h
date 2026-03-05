

#ifndef _nfc_h_
#define _nfc_h_

#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PN532_I2C_ADDR_7BIT (0x24)
#define PN532_I2C_ADDR_8BIT (PN532_I2C_ADDR_7BIT << 1)

typedef struct
{
    I2C_HandleTypeDef *hi2c;
    uint16_t i2c_addr;

    GPIO_TypeDef *irq_port;
    uint16_t irq_pin;

    GPIO_TypeDef *rst_port;
    uint16_t rst_pin;
} NFC_handle;

typedef struct {
    uint8_t uid[10];
    uint8_t uid_len;
} NFC_uid;

bool NFC_uid_equal(
    const uint8_t *a,
    uint8_t alen,
    const uint8_t *b,
    uint8_t blen
);

HAL_StatusTypeDef NFC_Init(NFC_handle *h);

/** Scan for a card and return UID if found. Timeout is RTOS ticks. */
HAL_StatusTypeDef NFC_ReadPassiveTargetID(
    NFC_handle *h,
    uint8_t *uid,
    uint8_t *uid_len,
    TickType_t timeout_ticks
);

/** Call this from HAL_GPIO_EXTI_Callback when the PN532 IRQ pin fires. */
void NFC_IrqFromIsr(NFC_handle *h);

#ifdef __cplusplus
}
#endif

#endif // _nfc_h_
