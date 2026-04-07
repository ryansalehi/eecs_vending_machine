#include "motors.h"

void dispense(uint8_t motor_num, TIM_HandleTypeDef *htim3)
{

	switch(motor_num)
	{
		case 1:
		  HAL_GPIO_WritePin(enable_motor1_GPIO_Port, enable_motor1_Pin, GPIO_PIN_RESET);
		  HAL_GPIO_WritePin(enable_motor2_GPIO_Port, enable_motor2_Pin, GPIO_PIN_SET);
		  HAL_GPIO_WritePin(enable_motor3_GPIO_Port, enable_motor3_Pin, GPIO_PIN_SET);
		  HAL_GPIO_WritePin(enable_motor4_GPIO_Port, enable_motor4_Pin, GPIO_PIN_SET);
		  break;
		case 2:
		  HAL_GPIO_WritePin(enable_motor2_GPIO_Port, enable_motor2_Pin, GPIO_PIN_RESET);
		  HAL_GPIO_WritePin(enable_motor1_GPIO_Port, enable_motor1_Pin, GPIO_PIN_SET);
		  HAL_GPIO_WritePin(enable_motor3_GPIO_Port, enable_motor3_Pin, GPIO_PIN_SET);
		  HAL_GPIO_WritePin(enable_motor4_GPIO_Port, enable_motor4_Pin, GPIO_PIN_SET);
		  break;
		case 3:
		  HAL_GPIO_WritePin(enable_motor3_GPIO_Port, enable_motor3_Pin, GPIO_PIN_RESET);
		  HAL_GPIO_WritePin(enable_motor1_GPIO_Port, enable_motor1_Pin, GPIO_PIN_SET);
		  HAL_GPIO_WritePin(enable_motor2_GPIO_Port, enable_motor2_Pin, GPIO_PIN_SET);
		  HAL_GPIO_WritePin(enable_motor4_GPIO_Port, enable_motor4_Pin, GPIO_PIN_SET);
		  break;
		case 4:
		  HAL_GPIO_WritePin(enable_motor4_GPIO_Port, enable_motor4_Pin, GPIO_PIN_RESET);
		  HAL_GPIO_WritePin(enable_motor1_GPIO_Port, enable_motor1_Pin, GPIO_PIN_SET);
		  HAL_GPIO_WritePin(enable_motor2_GPIO_Port, enable_motor2_Pin, GPIO_PIN_SET);
		  HAL_GPIO_WritePin(enable_motor3_GPIO_Port, enable_motor3_Pin, GPIO_PIN_SET);
		  break;
		default:
			return;
	}

	  HAL_GPIO_WritePin(motor_direction_GPIO_Port, motor_direction_Pin, GPIO_PIN_RESET);
	  HAL_TIM_PWM_Start(htim3, TIM_CHANNEL_3);
	  HAL_Delay(2000);
	  HAL_TIM_PWM_Stop(htim3, TIM_CHANNEL_3);

	switch(motor_num)
	{
		case 1:
			HAL_GPIO_WritePin(enable_motor1_GPIO_Port, enable_motor1_Pin, GPIO_PIN_SET);
			break;
		case 2:
			HAL_GPIO_WritePin(enable_motor2_GPIO_Port, enable_motor2_Pin, GPIO_PIN_SET);
			break;
		case 3:
			HAL_GPIO_WritePin(enable_motor3_GPIO_Port, enable_motor3_Pin, GPIO_PIN_SET);
			break;
		case 4:
			HAL_GPIO_WritePin(enable_motor4_GPIO_Port, enable_motor4_Pin, GPIO_PIN_SET);
			break;
		default:
			return;
	}
}
