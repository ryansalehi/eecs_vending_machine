#ifndef PS2_H
#define PS2_H

#include <stdint.h>
#include <stdbool.h>

//resets the receiver state
void PS2_Init(void);
bool PS2_Inited();

# define PS2_MESSAGE_MAX 500

extern volatile uint8_t ps2_data_snapshot;

//called once per falling clock edge from the interrupt callback
void PS2_ClockEdgeFromIsr(void);

//returns true if a valid byte was received
bool PS2_ReadSuccessful(void);

//returns true if the read failed
bool PS2_ReadFailed(void);

//gives you the decoded byte when it is ready
bool PS2_GetByte(uint8_t *byte);

void PS2_CheckMessageTimeout(void);

bool PS2_MessageReady(void);

uint16_t PS2_GetMessage(uint8_t *buffer, uint16_t buffer_size);

char PS2_ScanCodeToAscii(uint8_t scan_code);

void PS2_ClearMessage(void);
#endif /* PS2_H */
