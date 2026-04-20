#include "sender.h"
#include "main.h"
#include <stdint.h>
#include <string.h>
#include "mbedtls/aes.h"

static mbedtls_aes_context aes;
static unsigned char key[16];      // 128-bit key (use 24 or 32 for AES-192/256)
static unsigned char input[32];    // your 32-byte plaintext
static unsigned char output[32];   // encrypted result
static unsigned char iv[16];       // initialization vector (for CBC)

// Tell this file that huart3 exists somewhere else (in main.c)
extern UART_HandleTypeDef huart3;

void UART_Init()
{
    for(int i = 0; i < sizeof(key); i++)
    {
        key[i] = i + 1;
        iv[i] = i + 1;
    }
}

void UART_SendMessage(char* message)
{
    // size of payload, added 1 for zero terminator
    uint16_t msg_size = 32;
    if(msg_size > 32)
    {
        return;
    }

    mbedtls_aes_init(&aes);
    memset(input, 0, sizeof(input));
    memset(output, 0, sizeof(output));
    memcpy(input, message, msg_size);
    unsigned char ivcopy[16];
    memcpy(ivcopy,iv,16);

    mbedtls_aes_setkey_enc(&aes, key, 128);
    mbedtls_aes_crypt_cbc(
        &aes,
        MBEDTLS_AES_ENCRYPT,
        32,
        ivcopy,
        input,
        output
    );

    HAL_UART_Transmit(&huart3, output, 32, 100);
    mbedtls_aes_free(&aes);
}
