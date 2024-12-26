/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
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
#include "stm32f4xx_hal.h"

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

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
#define SD_SPI_HANDLE   hspi1
#define SD_CS_GPIO_Port GPIOC
#define SD_CS_Pin       GPIO_PIN_5

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define KEYPAD_COL_2_Pin GPIO_PIN_0
#define KEYPAD_COL_2_GPIO_Port GPIOC
#define KEYPAD_ROW_1_Pin GPIO_PIN_1
#define KEYPAD_ROW_1_GPIO_Port GPIOC
#define KEYPAD_COL_1_Pin GPIO_PIN_2
#define KEYPAD_COL_1_GPIO_Port GPIOC
#define KEYPAD_ROW_4_Pin GPIO_PIN_3
#define KEYPAD_ROW_4_GPIO_Port GPIOC
#define CAN_ID_SWITCH_4_Pin GPIO_PIN_0
#define CAN_ID_SWITCH_4_GPIO_Port GPIOA
#define KEYPAD_ROW_2_Pin GPIO_PIN_1
#define KEYPAD_ROW_2_GPIO_Port GPIOA
#define WHITE_LED_Pin GPIO_PIN_3
#define WHITE_LED_GPIO_Port GPIOA
#define KEYPAD_COL_3_Pin GPIO_PIN_4
#define KEYPAD_COL_3_GPIO_Port GPIOA
#define KEYPAD_ROW_3_Pin GPIO_PIN_4
#define KEYPAD_ROW_3_GPIO_Port GPIOC
#define SD_CS_Pin GPIO_PIN_5
#define SD_CS_GPIO_Port GPIOC
#define SD_CD_Pin GPIO_PIN_0
#define SD_CD_GPIO_Port GPIOB
#define KEYPAD_ENTER_Pin GPIO_PIN_1
#define KEYPAD_ENTER_GPIO_Port GPIOB
#define SD_MODE_Pin GPIO_PIN_14
#define SD_MODE_GPIO_Port GPIOB
#define CAN_ID_SWITCH_1_Pin GPIO_PIN_6
#define CAN_ID_SWITCH_1_GPIO_Port GPIOC
#define CAN_ID_SWITCH_2_Pin GPIO_PIN_7
#define CAN_ID_SWITCH_2_GPIO_Port GPIOC
#define CAN_ID_SWITCH_3_Pin GPIO_PIN_8
#define CAN_ID_SWITCH_3_GPIO_Port GPIOC
#define TC_DISPLAY_SDA_Pin GPIO_PIN_9
#define TC_DISPLAY_SDA_GPIO_Port GPIOC
#define TC_DISPLAY_SCL_Pin GPIO_PIN_8
#define TC_DISPLAY_SCL_GPIO_Port GPIOA
#define MUTE_SWITCH_Pin GPIO_PIN_10
#define MUTE_SWITCH_GPIO_Port GPIOC
#define TIME_TRAVEL_SIM_Pin GPIO_PIN_11
#define TIME_TRAVEL_SIM_GPIO_Port GPIOC
#define TIME_TRAVEL_SIM_EXTI_IRQn EXTI15_10_IRQn
#define DIAGNOSTIC_RGB_LED_Pin GPIO_PIN_12
#define DIAGNOSTIC_RGB_LED_GPIO_Port GPIOC
#define IMU_INTERRUPT_Pin GPIO_PIN_2
#define IMU_INTERRUPT_GPIO_Port GPIOD
#define GLITCH_SWITCH_Pin GPIO_PIN_5
#define GLITCH_SWITCH_GPIO_Port GPIOB
#define IMU_SCL_Pin GPIO_PIN_6
#define IMU_SCL_GPIO_Port GPIOB
#define IMU_SDA_Pin GPIO_PIN_7
#define IMU_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
