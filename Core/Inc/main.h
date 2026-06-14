/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define PUSH6_Pin GPIO_PIN_13
#define PUSH6_GPIO_Port GPIOC
#define LED_BUILTIN_Pin GPIO_PIN_6
#define LED_BUILTIN_GPIO_Port GPIOC
#define LATCH_Pin GPIO_PIN_15
#define LATCH_GPIO_Port GPIOA
#define SOFT_POWER_Pin GPIO_PIN_3
#define SOFT_POWER_GPIO_Port GPIOB
#define PUSH1_Pin GPIO_PIN_4
#define PUSH1_GPIO_Port GPIOB
#define PUSH2_Pin GPIO_PIN_5
#define PUSH2_GPIO_Port GPIOB
#define PUSH3_Pin GPIO_PIN_6
#define PUSH3_GPIO_Port GPIOB
#define PUSH4_Pin GPIO_PIN_7
#define PUSH4_GPIO_Port GPIOB
#define PUSH5_Pin GPIO_PIN_9
#define PUSH5_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define ARM_MATH_CM4

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
