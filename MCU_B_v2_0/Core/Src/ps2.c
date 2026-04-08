
#include "ps2.h"
#include "main.h"
#include <stddef.h>

static volatile uint8_t ps2_bit_count = 0;
static volatile uint8_t ps2_current_byte = 0;
static volatile uint8_t ps2_parity_bit = 0;
static volatile uint8_t ps2_last_byte = 0;
volatile uint8_t ps2_data_snapshot = 0;

/* timeout value for end-of-message detection */
# define PS2_MESSAGE_TIMEOUT_MS 25

static volatile bool ps2_read_successful = false;
static volatile bool ps2_read_failed = false;

static volatile uint8_t ps2_message[PS2_MESSAGE_MAX];
static volatile uint16_t ps2_message_length = 0;
static volatile bool ps2_message_ready = false;
static volatile uint32_t ps2_last_byte_tick = 0;

static uint8_t PS2_ReadDataPin(void);
static bool PS2_CheckOddParity(uint8_t data, uint8_t parity_bit);
static void PS2_ResetFrame(void);

static const uint8_t ps2_to_ascii[256] = {
    [0x16] = '1', [0x1E] = '2', [0x26] = '3', [0x25] = '4',
    [0x2E] = '5', [0x36] = '6', [0x3D] = '7', [0x3E] = '8',
    [0x46] = '9', [0x45] = '0',

    [0x1C] = 'a', [0x32] = 'b', [0x21] = 'c', [0x23] = 'd',
    [0x24] = 'e', [0x2B] = 'f', [0x34] = 'g', [0x33] = 'h',
    [0x43] = 'i', [0x3B] = 'j', [0x42] = 'k', [0x4B] = 'l',
    [0x3A] = 'm', [0x31] = 'n', [0x44] = 'o', [0x4D] = 'p',
    [0x15] = 'q', [0x2D] = 'r', [0x1B] = 's', [0x2C] = 't',
    [0x3C] = 'u', [0x2A] = 'v', [0x1D] = 'w', [0x22] = 'x',
    [0x35] = 'y', [0x1A] = 'z',

    [0x4C] = ';', [0x52] = '\'', [0x41] = ',', [0x49] = '.',
    [0x4A] = '/', [0x4E] = '-', [0x55] = '=', [0x54] = '[',
    [0x5B] = ']', [0x5D] = '\\', [0x0E] = '`', [0x29] = ' ',
    [0x5A] = '\r', [0x66] = '\b', [0x0D] = '\t',
};

char PS2_ScanCodeToAscii(uint8_t scan_code)
{
    return (char)ps2_to_ascii[scan_code];
}

//resets the receiver state
void PS2_Init(void)
{
    ps2_bit_count = 0;
    ps2_current_byte = 0;
    ps2_parity_bit = 0;
    ps2_last_byte = 0;

    ps2_read_successful = false;
    ps2_read_failed = false;

    ps2_message_length = 0;
    ps2_message_ready = false;
    ps2_last_byte_tick = 0;
}

//called once per falling clock edge from the interrupt callback
void PS2_ClockEdgeFromIsr(void)
{
	uint8_t bit = ps2_data_snapshot;

	switch (ps2_bit_count)
	{
		case 0:
			//start bit is always zero
			ps2_current_byte = 0;
			ps2_parity_bit = 0;

			//Commented out for debugging the first bit being 1
//            if (bit != 0U)
//            {
//            	ps2_read_successful = false;
//                ps2_read_failed = true;
//                PS2_ResetFrame();
//                return;
//            }
            break;

        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        	//8 bits of data, LSB first
        	ps2_current_byte |= (bit << (ps2_bit_count - 1U));
        	break;

        case 9:
        	ps2_parity_bit = bit;
        	break;

        case 10:
        	//Commented out to debug first bit being 1.
//        	//Stop bit must be 1.
//        	if ((bit == 1U) && PS2_CheckOddParity(ps2_current_byte, ps2_parity_bit))
//        	{
//        		ps2_last_byte = ps2_current_byte;
//        		ps2_read_successful = true;
//        		ps2_read_failed = false;
//
//        		if (ps2_message_length < PS2_MESSAGE_MAX)
//				{
//					ps2_message[ps2_message_length] = ps2_current_byte;
//					ps2_message_length++;
//					ps2_last_byte_tick = HAL_GetTick();
//					ps2_message_ready = false;
//				}
//				else
//				{
//					// buffer overflow
//					ps2_read_failed = true;
//				}
//        	}
//        	else
//        	{
//        		ps2_read_failed = true;
//        		ps2_read_successful = false;
//        	}
//
//        	PS2_ResetFrame();
//        	return;
        	ps2_last_byte = ps2_current_byte;
			ps2_read_successful = true;
			ps2_read_failed = false;

			if (ps2_message_length < PS2_MESSAGE_MAX)
			{
				ps2_message[ps2_message_length] = ps2_current_byte;
				ps2_message_length++;
				ps2_last_byte_tick = HAL_GetTick();
				ps2_message_ready = false;
			}

			PS2_ResetFrame();
			return;


        default:
        	//Should not happen
        	ps2_read_failed = true;
        	ps2_read_successful = false;
        	PS2_ResetFrame();
        	return;
	}

	ps2_bit_count++;
}

//returns true if a valid byte was received
bool PS2_ReadSuccessful(void)
{
	return ps2_read_successful;
}

//returns true if the read failed
bool PS2_ReadFailed(void)
{
	return ps2_read_failed;
}

//gives you the decoded byte when it is ready
bool PS2_GetByte(uint8_t *byte)
{
	if ((byte == NULL) || (ps2_read_successful == false))
	{
		return false;
	}

	*byte = ps2_last_byte;
	ps2_read_successful = false;
	return true;
}

// allows us to receive multi-byte messages that arrive within the timeout period (25 ms)
void PS2_CheckMessageTimeout(void)
{
    if ((ps2_message_length > 0U) &&
        (ps2_message_ready == false) &&
        ((HAL_GetTick() - ps2_last_byte_tick) > PS2_MESSAGE_TIMEOUT_MS))
    {
        ps2_message_ready = true;
    }
}

//set by PS2_CheckMessageTimeout after 25 ms has elapsed letting us know we are safe to
//decode the full multi-byte message.
bool PS2_MessageReady(void)
{
    return ps2_message_ready;
}

//returns the size of the message as a integer number of bytes
uint16_t PS2_GetMessage(uint8_t *buffer, uint16_t buffer_size)
{
    uint16_t i;
    uint16_t copy_length;

    if ((buffer == NULL) || (buffer_size == 0U) || (ps2_message_ready == false))
    {
        return 0U;
    }

    // protecting the following code from race conditions
    __disable_irq();

    // defensive programming, but PS2_MESSAGE_MAX should be passed into the buffer_size argument
    // preventing this from happening
    copy_length = ps2_message_length;
    if (copy_length > buffer_size)
    {
        copy_length = buffer_size;
    }

    //copying over the message into the buffer passed into the function
    for (i = 0; i < copy_length; i++)
    {
        buffer[i] = ps2_message[i];
    }

    ps2_message_length = 0;
    ps2_message_ready = false;
    ps2_last_byte_tick = 0;

    __enable_irq();

    return copy_length;
}

void PS2_ClearMessage(void)
{
	// protecting the following from race conditions
    __disable_irq();
    ps2_message_length = 0;
    ps2_message_ready = false;
    ps2_last_byte_tick = 0;
    __enable_irq();
}


static uint8_t PS2_ReadDataPin(void)
{
    return (uint8_t)HAL_GPIO_ReadPin(PS_2_Data_GPIO_Port, PS_2_Data_Pin);
}

static bool PS2_CheckOddParity(uint8_t data, uint8_t parity_bit)
{
	uint8_t ones = 0;

	for (uint8_t i = 0; i < 8U; i++)
	{
		ones += (data >> i) & 0x01U;
	}

	ones += (parity_bit & 0x01U);

	//PS/2 uses odd parity, so total number of 1s should be odd
	return ((ones & 0x01U) == 1U);
}

static void PS2_ResetFrame(void)
{
    ps2_bit_count = 0;
    ps2_current_byte = 0;
    ps2_parity_bit = 0;
}
