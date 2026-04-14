#include "nfc2.h"

#include "FreeRTOS.h"
#include "task.h"


//#include "stm32f4xx_hal.h"

#include <string.h>

/* CubeMX should generate hi2c1 in i2c.c */
extern I2C_HandleTypeDef hi2c1;

/* ---------- Pin / bus config ---------- */

#define PN532_I2C_ADDR          (0x24u << 1)   /* STM32 HAL wants shifted 7-bit address */
#define PN532_IRQ_PORT          GPIOC
#define PN532_IRQ_PIN           GPIO_PIN_13

/* ---------- PN532 protocol constants ---------- */

#define PN532_PREAMBLE          0x00u
#define PN532_STARTCODE1        0x00u
#define PN532_STARTCODE2        0xFFu
#define PN532_POSTAMBLE         0x00u

#define PN532_HOST_TO_PN532     0xD4u
#define PN532_PN532_TO_HOST     0xD5u

#define PN532_I2C_READY         0x01u

#define PN532_CMD_GET_FW        0x02u
#define PN532_CMD_SETPARAM      0x12u
#define PN532_CMD_SAMCONFIG     0x14u
#define PN532_CMD_INLIST        0x4Au

#define PN532_RESP_GET_FW       0x03u
#define PN532_RESP_SETPARAM     0x13u
#define PN532_RESP_SAMCONFIG    0x15u
#define PN532_RESP_INLIST       0x4Bu

/* Fixed PN532 ACK frame (without I2C status byte) */
static const uint8_t s_ack_frame[6] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};

/* Only one task should own the NFC device. */
static TaskHandle_t s_waiting_task = NULL;

/* ---------- Low-level helpers ---------- */

static bool pn532_i2c_write(const uint8_t *buf, uint16_t len)
{
    return (HAL_I2C_Master_Transmit(&hi2c1, PN532_I2C_ADDR, (uint8_t *)buf, len, 100) == HAL_OK);
}

static bool pn532_i2c_read(uint8_t *buf, uint16_t len)
{
    return (HAL_I2C_Master_Receive(&hi2c1, PN532_I2C_ADDR, buf, len, 100) == HAL_OK);
}

static bool pn532_wait_irq(uint32_t timeout_ms)
{
    /* Scheduler running: block on task notification from EXTI ISR. */
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        s_waiting_task = xTaskGetCurrentTaskHandle();

        /* Clear any stale notification count. */
        (void)ulTaskNotifyTake(pdTRUE, 0);

        /* Already asserted? */
        if (HAL_GPIO_ReadPin(PN532_IRQ_PORT, PN532_IRQ_PIN) == GPIO_PIN_RESET)
        {
            return true;
        }

        return (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(timeout_ms)) > 0);
    }

    /* No scheduler yet: simple GPIO poll. */
    uint32_t start = HAL_GetTick();
    while (HAL_GPIO_ReadPin(PN532_IRQ_PORT, PN532_IRQ_PIN) == GPIO_PIN_SET)
    {
        if ((HAL_GetTick() - start) >= timeout_ms)
        {
            return false;
        }
    }

    return true;
}

static bool pn532_write_frame(const uint8_t *cmd, uint8_t cmd_len)
{
    /* 8 bytes framing + small command payload */
    uint8_t frame[32];
    uint8_t len = (uint8_t)(cmd_len + 1u); /* + TFI */
    uint8_t sum = PN532_HOST_TO_PN532;

    frame[0] = PN532_PREAMBLE;
    frame[1] = PN532_STARTCODE1;
    frame[2] = PN532_STARTCODE2;
    frame[3] = len;
    frame[4] = (uint8_t)(~len + 1u);
    frame[5] = PN532_HOST_TO_PN532;

    for (uint8_t i = 0; i < cmd_len; i++)
    {
        frame[6 + i] = cmd[i];
        sum = (uint8_t)(sum + cmd[i]);
    }

    frame[6 + cmd_len] = (uint8_t)(~sum + 1u); /* DCS */
    frame[7 + cmd_len] = PN532_POSTAMBLE;

    return pn532_i2c_write(frame, (uint16_t)(8u + cmd_len));
}

static NFC_Result_t pn532_read_ack(uint32_t timeout_ms)
{
    uint8_t rx[1 + 6]; /* status + ACK */

    if (!pn532_wait_irq(timeout_ms))
    {
        return NFC_TIMEOUT;
    }

    if (!pn532_i2c_read(rx, sizeof(rx)))
    {
        return NFC_ERROR;
    }

    if (rx[0] != PN532_I2C_READY)
    {
        return NFC_ERROR;
    }

    if (memcmp(&rx[1], s_ack_frame, sizeof(s_ack_frame)) != 0)
    {
        return NFC_ERROR;
    }

    return NFC_OK;
}

/*
 * Reads one full information frame plus I2C status byte.
 *
 * rx_total_len must be large enough for the expected response transaction.
 * For this minimal driver:
 *   SAMConfiguration response: 10 bytes total
 *   SetParameters response:    10 bytes total
 *   GetFirmwareVersion:        14 bytes total
 *   InListPassiveTarget:       26 bytes total (max UID-only frame, no ATS)
 */
static NFC_Result_t pn532_read_response(uint8_t expected_resp_code,
                                        uint8_t *out,
                                        uint8_t *out_len,
                                        uint16_t rx_total_len,
                                        uint32_t timeout_ms)
{
    uint8_t rx[32];

    if (rx_total_len > sizeof(rx))
    {
        return NFC_ERROR;
    }

    if (!pn532_wait_irq(timeout_ms))
    {
        return NFC_TIMEOUT;
    }

    if (!pn532_i2c_read(rx, rx_total_len))
    {
        return NFC_ERROR;
    }

    if (rx[0] != PN532_I2C_READY)
    {
        return NFC_ERROR;
    }

    /* Normal frame starts at rx[1] */
    if ((rx[1] != 0x00u) || (rx[2] != 0x00u) || (rx[3] != 0xFFu))
    {
        return NFC_ERROR;
    }

    uint8_t len = rx[4];
    uint8_t lcs = rx[5];

    if ((uint8_t)(len + lcs) != 0x00u)
    {
        return NFC_ERROR;
    }

    /* Need at least TFI + response code */
    if (len < 2u)
    {
        return NFC_ERROR;
    }

    /* Check TFI / response code */
    if (rx[6] != PN532_PN532_TO_HOST)
    {
        return NFC_ERROR;
    }

    if (rx[7] != expected_resp_code)
    {
        return NFC_ERROR;
    }

    /* Data checksum over [TFI..last payload byte] + DCS == 0 */
    uint8_t sum = 0;
    for (uint16_t i = 6; i < (uint16_t)(6u + len); i++)
    {
        sum = (uint8_t)(sum + rx[i]);
    }
    sum = (uint8_t)(sum + rx[6u + len]); /* DCS */

    if (sum != 0x00u)
    {
        return NFC_ERROR;
    }

    if (rx[7u + len] != PN532_POSTAMBLE)
    {
        return NFC_ERROR;
    }

    /* Copy only command-specific payload: bytes after D5 + RespCode */
    uint8_t payload_len = (uint8_t)(len - 2u);
    if (out && payload_len)
    {
        memcpy(out, &rx[8], payload_len);
    }

    if (out_len)
    {
        *out_len = payload_len;
    }

    return NFC_OK;
}

static NFC_Result_t pn532_command(const uint8_t *cmd,
                                  uint8_t cmd_len,
                                  uint8_t expected_resp_code,
                                  uint8_t *resp,
                                  uint8_t *resp_len,
                                  uint16_t resp_rx_total_len,
                                  uint32_t timeout_ms)
{
    NFC_Result_t r;

    if (!pn532_write_frame(cmd, cmd_len))
    {
        return NFC_ERROR;
    }

    r = pn532_read_ack(timeout_ms);
    if (r != NFC_OK)
    {
        return r;
    }

    r = pn532_read_response(expected_resp_code, resp, resp_len, resp_rx_total_len, timeout_ms);
    return r;
}

/* ---------- Public API ---------- */

bool NFC_Init(void)
{
    uint8_t resp[16];
    uint8_t resp_len = 0;

    /*
     * 1) Put PN532 into normal mode and enable IRQ output:
     *    D4 14 01 00 01
     */
    const uint8_t sam_config[] = {PN532_CMD_SAMCONFIG, 0x01u, 0x00u, 0x01u};
    if (pn532_command(sam_config,
                      sizeof(sam_config),
                      PN532_RESP_SAMCONFIG,
                      resp,
                      &resp_len,
                      10u,
                      200u) != NFC_OK)
    {
        return false;
    }

    /*
     * 2) Optional sanity check:
     *    D4 02 -> D5 03 IC Ver Rev Support
     */
    const uint8_t get_fw[] = {PN532_CMD_GET_FW};
    if (pn532_command(get_fw,
                      sizeof(get_fw),
                      PN532_RESP_GET_FW,
                      resp,
                      &resp_len,
                      14u,
                      200u) != NFC_OK)
    {
        return false;
    }

    if ((resp_len < 4u) || (resp[0] != 0x32u))
    {
        return false;
    }

    /*
     * 3) Clear optional higher-level behavior we do not need here.
     *    SetParameters(0x00) disables automatic RATS,
     *    which keeps InListPassiveTarget responses short and UID-focused.
     */
    const uint8_t set_params[] = {PN532_CMD_SETPARAM, 0x00u};
    if (pn532_command(set_params,
                      sizeof(set_params),
                      PN532_RESP_SETPARAM,
                      resp,
                      &resp_len,
                      10u,
                      200u) != NFC_OK)
    {
        return false;
    }

    return true;
}

NFC_Result_t NFC_ReadUID(uint8_t *uid, uint8_t *uid_len, uint32_t timeout_ms)
{
    uint8_t resp[24];
    uint8_t resp_len = 0;

    if ((uid == NULL) || (uid_len == NULL))
    {
        return NFC_ERROR;
    }

    *uid_len = 0u;

    /*
     * InListPassiveTarget:
     *   MaxTg = 1
     *   BrTy  = 0x00 => 106 kbps Type A
     */
    const uint8_t in_list[] = {PN532_CMD_INLIST, 0x01u, 0x00u};

    NFC_Result_t r = pn532_command(in_list,
                                   sizeof(in_list),
                                   PN532_RESP_INLIST,
                                   resp,
                                   &resp_len,
                                   26u,         /* status + max UID-only Type A frame */
                                   timeout_ms);

    if (r != NFC_OK)
    {
        return r;
    }

    /*
     * Response payload layout for 106 kbps Type A:
     *   resp[0]  = NbTg
     *   resp[1]  = Tg
     *   resp[2]  = SENS_RES[0]
     *   resp[3]  = SENS_RES[1]
     *   resp[4]  = SEL_RES
     *   resp[5]  = NFCIDLength
     *   resp[6+] = NFCID1[]
     */
    if (resp_len < 1u)
    {
        return NFC_ERROR;
    }

    if (resp[0] == 0u)
    {
        return NFC_NO_CARD;
    }

    if (resp_len < 6u)
    {
        return NFC_ERROR;
    }

    uint8_t n = resp[5];
    if ((n == 0u) || (n > 10u))
    {
        return NFC_ERROR;
    }

    if ((uint8_t)(6u + n) > resp_len)
    {
        return NFC_ERROR;
    }

    memcpy(uid, &resp[6], n);
    *uid_len = n;

    return NFC_OK;
}

void NFC_IrqFromISR(void)
{
    BaseType_t hpw = pdFALSE;

    if (s_waiting_task != NULL)
    {
        vTaskNotifyGiveFromISR(s_waiting_task, &hpw);
        portYIELD_FROM_ISR(hpw);
    }
}
