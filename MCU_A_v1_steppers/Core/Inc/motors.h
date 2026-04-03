#ifndef MOTORS_H
#define MOTORS_H

#include "main.h"

// pass integer value 1, 2, 3, or 4 to specify
// which motor you want to spin
// does nothing if other values are passed

//also pass in pointer to timer handle.
//you'll see it defined in main.c as
/* Private variables
//TIM_HandleTypeDef htim3;*/
void dispense(uint8_t motor_num, TIM_HandleTypeDef *htim3);

#endif
