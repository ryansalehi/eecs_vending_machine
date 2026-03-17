#ifndef PS2_H
#define PS2_H

#include <stdint.h>
#include <stdbool.h>

//resets the receiver state
void PS2_Init(void);

//called once per falling clock edge from the interrupt callback
void PS2_ClockEdgeFromIsr(void);

//returns true if a valid byte was received
bool PS2_ReadSuccessful(void);

//returns true if the read failed
bool PS2_ReadFailed(void);

//gives you the decoded byte when it is ready
bool PS2_GetByte(uint8_t *byte);
#endif /* PS2_H */
