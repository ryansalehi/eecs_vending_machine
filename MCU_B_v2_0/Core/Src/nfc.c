#include "nfc.h"
#include <string.h>
#include "cbuf.h"

/* PN532 framing constants */
#define PN532_PREAMBLE     0x00
#define PN532_STARTCODE1   0x00
#define PN532_STARTCODE2   0xFF
#define PN532_POSTAMBLE    0x00

#define PN532_HOSTTOPN532  0xD4
#define PN532_PN532TOHOST  0xD5

#define PN532_CMD_SAMCONFIGURATION     0x14
#define PN532_CMD_INLISTPASSIVETARGET  0x4A

/* Typical PN532 ready latency */
#define PN532_ACK_TIMEOUT_TICKS   pdMS_TO_TICKS(200)
#define PN532_RSP_TIMEOUT_TICKS   pdMS_TO_TICKS(500)
#define PN532_I2C_READY           0x01u
#define PN532_I2C_POLL_DELAY_MS   1u

NFC_uid uidsArray[10];
cbuf_t uids;
static volatile bool IRQ_raised = false;

/* Local helpers */
static uint8_t checksum8(const uint8_t *data, uint8_t len)
{
    uint16_t sum = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        sum += data[i];
    }
    return (uint8_t)(0x100 - (sum & 0xFF));
}

bool NFC_uid_equal(const uint8_t *a, uint8_t alen, const uint8_t *b, uint8_t blen)
{
    if (alen != blen) return false;
    return (memcmp(a, b, alen) == 0);
}

static TickType_t nfc_get_tick(void)
{
    if(xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
        return pdMS_TO_TICKS(HAL_GetTick());
    }

    return xTaskGetTickCount();
}

static HAL_StatusTypeDef nfc_wait_i2c_ready(NFC_handle *h, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while(HAL_I2C_GetState(h->hi2c) != HAL_I2C_STATE_READY)
    {
        if((HAL_GetTick() - start) >= timeout_ms)
        {
            return HAL_BUSY;
        }
    }

    return HAL_OK;
}

static HAL_StatusTypeDef nfc_write_frame(NFC_handle *h, const uint8_t *data, uint8_t len)
{
    /* Frame: 00 00 FF LEN LCS [DATA...] DCS 00
       Over I2C: leading 0x00 for "write data"
    */
    uint8_t buf[64];
    if((uint16_t)len + 9u > sizeof(buf))
    {
        return HAL_ERROR;
    }

    uint8_t idx = 0;
    buf[idx++] = 0x00; // I2C write
    buf[idx++] = PN532_PREAMBLE;
    buf[idx++] = PN532_STARTCODE1;
    buf[idx++] = PN532_STARTCODE2;
    buf[idx++] = len;
    buf[idx++] = (uint8_t)(0x100 - len); // length checksum (LEN + LCS = 0x00 mod 256)
    memcpy(&buf[idx], data, len);
    idx += len;
    buf[idx++] = checksum8(data, len); // data checksum
    buf[idx++] = PN532_POSTAMBLE;

    HAL_StatusTypeDef st = nfc_wait_i2c_ready(h, 10);
    if(st != HAL_OK)
    {
        return st;
    }

    return HAL_I2C_Master_Transmit(h->hi2c, h->i2c_addr, buf, idx, 100);
}

static HAL_StatusTypeDef nfc_read_bytes(NFC_handle *h, uint8_t *out, uint16_t n)
{
    HAL_StatusTypeDef wait_st = nfc_wait_i2c_ready(h, 10);
    if(wait_st != HAL_OK)
    {
        return wait_st;
    }

    HAL_StatusTypeDef st = HAL_I2C_Master_Receive(h->hi2c, h->i2c_addr, out, n, 1000);
    return st;
}

static bool nfc_irq_asserted(NFC_handle *h)
{
    if((h->irq_port == NULL) || (h->irq_pin == 0u))
    {
        return false;
    }

    return HAL_GPIO_ReadPin(h->irq_port, h->irq_pin) == GPIO_PIN_RESET;
}

static bool nfc_status_ready(NFC_handle *h)
{
    uint8_t status = 0;

    if(nfc_read_bytes(h, &status, 1) != HAL_OK)
    {
        return false;
    }

    return status == PN532_I2C_READY;
}

static HAL_StatusTypeDef nfc_wait_ready(NFC_handle *h, TickType_t timeout_ticks)
{
    uint32_t timeout_ms = (uint32_t)timeout_ticks * (uint32_t)portTICK_PERIOD_MS;
    if((timeout_ticks > 0u) && (timeout_ms == 0u))
    {
        timeout_ms = 1u;
    }
    uint32_t start = HAL_GetTick();

    do
    {
        if(IRQ_raised || nfc_irq_asserted(h))
        {
            IRQ_raised = false;
            return HAL_OK;
        }

        if(nfc_status_ready(h))
        {
            IRQ_raised = false;
            return HAL_OK;
        }

        HAL_Delay(PN532_I2C_POLL_DELAY_MS);
    } while ((HAL_GetTick() - start) < timeout_ms);

    return HAL_TIMEOUT;
}

static HAL_StatusTypeDef nfc_read_ack(NFC_handle *h)
{
    uint8_t r[10] = {0};
    HAL_StatusTypeDef st = nfc_read_bytes(h, r, sizeof(r));
    if(st != HAL_OK)
    {
        return st;
    }

    /* Sometimes there is a leading status byte 0x01 */
    if(r[0] == PN532_I2C_READY)
    {
        if(r[1]==0x00 && r[2]==0x00 && r[3]==0xFF && r[4]==0x00 && r[5]==0xFF && r[6]==0x00)
        {
            return HAL_OK;
        }
    }
    else
    {
        if(r[0]==0x00 && r[1]==0x00 && r[2]==0xFF && r[3]==0x00 && r[4]==0xFF && r[5]==0x00)
        {
            return HAL_OK;
        }
    }
    return HAL_ERROR;
}

static HAL_StatusTypeDef nfc_read_response(NFC_handle *h, uint8_t *out, uint8_t out_max, uint8_t *out_len)
{
    uint8_t r[64] = {0};
    HAL_StatusTypeDef st = nfc_read_bytes(h, r, sizeof(r));
    if(st != HAL_OK)
    {
        return st;
    }

    uint8_t i = 0;
    if(r[0] == PN532_I2C_READY)
    {
         /* skip status */
        i = 1;
    }

    if(r[i+0]!=0x00 || r[i+1]!=0x00 || r[i+2]!=0xFF)
    {
        return HAL_ERROR;
    }

    uint8_t len = r[i+3];
    uint8_t lcs = r[i+4];
    if((uint8_t)(len + lcs) != 0x00)
    {
        return HAL_ERROR;
    }
    if(len > out_max)
    {
        return HAL_ERROR;
    }

    uint8_t dcs = r[i + 5u + len];
    uint8_t postamble = r[i + 6u + len];
    if(dcs != checksum8(&r[i + 5], len))
    {
        return HAL_ERROR;
    }
    if(postamble != PN532_POSTAMBLE)
    {
        return HAL_ERROR;
    }

    memcpy(out, &r[i+5], len);
    *out_len = len;
    return HAL_OK;
}

static HAL_StatusTypeDef nfc_send_cmd_wait(
    NFC_handle *h,
    const uint8_t *cmd,
    uint8_t cmd_len,
    uint8_t *resp,
    uint8_t resp_max,
    uint8_t *resp_len,
    TickType_t overall_timeout
){
    TickType_t start = nfc_get_tick();

    IRQ_raised = false;

    // CMD
    HAL_StatusTypeDef st = nfc_write_frame(h, cmd, cmd_len);
    if(st != HAL_OK)
    {
        return st;
    }

    // ACK
    st = nfc_wait_ready(h, PN532_ACK_TIMEOUT_TICKS);
    if(st != HAL_OK)
    {
        return st;
    }
    st = nfc_read_ack(h);
    if(st != HAL_OK)
    {
        return st;
    }

    // RESPONSE
    TickType_t now = nfc_get_tick();
    TickType_t remaining = (now - start < overall_timeout) ? (overall_timeout - (now - start)) : 0;
    if(remaining == 0)
    {
        return HAL_TIMEOUT;
    }
    st = nfc_wait_ready(h, (remaining < PN532_RSP_TIMEOUT_TICKS) ? remaining : PN532_RSP_TIMEOUT_TICKS);
    if(st != HAL_OK)
    {
        return st;
    }
    return nfc_read_response(h, resp, resp_max, resp_len);
}

/* Public API */
void NFC_IrqFromIsr(NFC_handle *h)
{
    (void)h;
    IRQ_raised = true;
}

HAL_StatusTypeDef NFC_Init(NFC_handle *h)
{
    cbuf_init(
        &uids,
        uidsArray,
        sizeof(uidsArray) / sizeof(uidsArray[0]),
        sizeof(NFC_uid)
    );

    if(h == NULL || h->hi2c == NULL)
    {
        return HAL_ERROR;
    }

    /* SAMConfiguration */
    uint8_t cmd[]  = { PN532_HOSTTOPN532, PN532_CMD_SAMCONFIGURATION, 0x01, 0x14, 0x01 };
    uint8_t resp[32]; uint8_t resp_len = 0;

    HAL_StatusTypeDef st = nfc_send_cmd_wait(h, cmd, sizeof(cmd), resp, sizeof(resp), &resp_len, pdMS_TO_TICKS(2000));
    if(st != HAL_OK)
    {
        return st;
    }

    /* Response should be: D5 15 ... */
    if(resp_len < 2 || resp[0] != PN532_PN532TOHOST || resp[1] != (PN532_CMD_SAMCONFIGURATION + 1))
    {
        return HAL_ERROR;
    }
    return HAL_OK;
}

HAL_StatusTypeDef NFC_ReadPassiveTargetID(
    NFC_handle *h,
    uint8_t *uid,
    uint8_t *uid_len,
    TickType_t timeout_ticks
) {
    if(!h || !uid || !uid_len)
    {
        return HAL_ERROR;
    }

    uint8_t cmd[]  = { PN532_HOSTTOPN532, PN532_CMD_INLISTPASSIVETARGET, 0x01, 0x00 };
    uint8_t resp[32]; uint8_t resp_len = 0;

    HAL_StatusTypeDef st = nfc_send_cmd_wait(h, cmd, sizeof(cmd), resp, sizeof(resp), &resp_len, timeout_ticks);
    if(st != HAL_OK)
    {
        return st;
    }

    /* Response: D5 4B numTags ... */
    if (resp_len < 3 || resp[0] != PN532_PN532TOHOST || resp[1] != (PN532_CMD_INLISTPASSIVETARGET + 1)) {
        return HAL_ERROR;
    }

    uint8_t numTags = resp[2];
    if(numTags < 1)
    {
        return HAL_ERROR;
    }

    /* Parse UID for ISO14443A:
       [3]=Tg, [4..5]=SensRes, [6]=SelRes, [7]=NFCIDLen, [8..] UID bytes
    */
    if(resp_len < 9)
    {
        return HAL_ERROR;
    }
    uint8_t n = resp[7];
    if(n == 0 || n > 10 || (uint8_t)(8 + n) > resp_len)
    {
        return HAL_ERROR;
    }

    memcpy(uid, &resp[8], n);
    *uid_len = n;
    return HAL_OK;
}
