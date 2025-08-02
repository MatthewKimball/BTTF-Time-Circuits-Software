/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include "sound_effects.h"
#include "timecircuit_control.h"
#include "imu.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
// Global handles for tasks and queue
osThreadId_t soundTaskHandle;
osThreadId_t colonTaskHandle;
osThreadId_t mainTaskHandle;
osMessageQueueId_t soundQueueHandle;


// External configuration handles (should be defined elsewhere)
extern SoundEffects_Config_t* gSoundEffectConfig;
extern StorageDevice_Config_t* gStorageConfig;
extern TimeCircuit_Control_Config_t* gTimeCircuitConfig;

// Global flag for sound playing
bool gIsPlaying = false;

/* USER CODE END Variables */
/* Definitions for MainTask */
osThreadId_t MainTaskHandle;
const osThreadAttr_t MainTask_attributes = {
  .name = "MainTask",
  .stack_size = 768 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for SoundTask */
osThreadId_t SoundTaskHandle;
const osThreadAttr_t SoundTask_attributes = {
  .name = "SoundTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for ColonTask */
osThreadId_t ColonTaskHandle;
const osThreadAttr_t ColonTask_attributes = {
  .name = "ColonTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartMainTask(void *argument);
void StartSoundTask(void *argument);
void StartColonTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of MainTask */
  MainTaskHandle = osThreadNew(StartMainTask, NULL, &MainTask_attributes);

  /* creation of SoundTask */
  SoundTaskHandle = osThreadNew(StartSoundTask, NULL, &SoundTask_attributes);

  /* creation of ColonTask */
  ColonTaskHandle = osThreadNew(StartColonTask, NULL, &ColonTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartMainTask */
/**
  * @brief  Function implementing the MainTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartMainTask */
void StartMainTask(void *argument)
{
  /* USER CODE BEGIN StartMainTask */
  imu_bno055_init();
  /* Infinite loop */
  for(;;)
  {
    timeCircuit_control_update(gTimeCircuitConfig);
    osDelay(20); // Run control update every 20ms (adjust as needed)

  }
  /* USER CODE END StartMainTask */
}

/* USER CODE BEGIN Header_StartSoundTask */
/**
* @brief Function implementing the SoundTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartSoundTask */
void StartSoundTask(void *argument)
{
  /* USER CODE BEGIN StartSoundTask */
  /* Infinite loop */
  for(;;)
  {
//    if (osMessageQueueGet(soundQueueHandle, &currentSound, NULL, osWaitForever) == osOK)
//    {
//      soundEffects_playSound(gSoundEffectConfig, gStorageConfig, currentSound);
//      while (gIsPlaying)
        osDelay(10); // Wait until sound is finished
//    }

  }
  /* USER CODE END StartSoundTask */
}

/* USER CODE BEGIN Header_StartColonTask */
/**
* @brief Function implementing the ColonTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartColonTask */
void StartColonTask(void *argument)
{
  /* USER CODE BEGIN StartColonTask */
  /* Infinite loop */
  for(;;)
  {
    timeCircuit__toggleTimeColon(gTimeCircuitConfig);

    // Optional: enqueue beep
//    if (toogleStatus == 3) {
//      const char* tone = "beep.wav";
//      osMessageQueuePut(soundQueueHandle, tone, 0, 0);
  //  }

    osDelay(500);

}
  /* USER CODE END StartColonTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

