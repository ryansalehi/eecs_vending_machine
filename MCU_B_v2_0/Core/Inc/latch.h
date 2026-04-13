
#ifndef LATCH_H_
#define LATCH_H_

typedef enum
{
    door,
    tokendrop
} LATCHES;

void LATCH_Open(LATCHES latch);

#endif
