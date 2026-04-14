/**
 * @file keypad.h
 * 
 * @date April 4, 2026
 */

#ifndef KEYPAD_H_
#define KEYPAD_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    char number_string[4];
    int number_int;
} Class_t;


void KEYPAD_IrqFromIsr();
void KEYPAD_ReadAnyPresses();
void KEYPAD_Init();


/**
 * @return whether the user entered a valid class number
 */
bool KEYPAD_PromptClassNumber(Class_t* class);

/*
* @return whether the user entered a valid answer A,B,C,D within 100ms
*/

bool KEYPAD_CheckForAnswer(uint8_t *index_out);

#endif //KEYPAD_H_
