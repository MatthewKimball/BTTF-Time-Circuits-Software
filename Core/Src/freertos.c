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
#include <string.h>
#include "sound_effects.h"
#include "timecircuit_control.h"
#include "imu.h"
#include "CO_app_STM32.h"
#include "OD.h"

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
extern CAN_HandleTypeDef hcan1;
extern TIM_HandleTypeDef htim7;
extern void MX_CAN1_Init(void);

// Global flag for sound playing
bool gIsPlaying = false;

// Colon beep is low-priority background "tick" - it must never delay or
// queue up ahead of interactive sounds (keypad, enter, glitch, locked). It's
// requested via this flag instead of the shared queue so multiple pending
// requests coalesce into a single pending beep instead of backing up.
volatile bool gColonPending = false;
volatile uint32_t gColonRequestTick = 0;
#define COLON_STALE_MS 500 // one colon blink half-period - older than this, skip it

// Set by StartRtcInitTask once timeCircuit_control_initRTC() returns, success
// or failure - lets StartMainTask give the RTC a short bounded head start
// before the displays first light up, so present time doesn't visibly jump
// right after power-on (see the wait in StartMainTask). Bounded so a
// hung/missing RTC still can never block boot - see StartRtcInitTask.
volatile bool gRtcInitDone = false;
#define RTC_INIT_WAIT_TIMEOUT_MS 500

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
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for ColonTask */
osThreadId_t ColonTaskHandle;
const osThreadAttr_t ColonTask_attributes = {
  .name = "ColonTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for CANopenTask */
osThreadId_t CANopenTaskHandle;
const osThreadAttr_t CANopenTask_attributes = {
  .name = "CANopenTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for RtcInitTask - isolated so a stuck/hanging RTC I2C bus can
 * never block MainTask, SoundTask, ColonTask or CANopenTask from running. */
osThreadId_t RtcInitTaskHandle;
const osThreadAttr_t RtcInitTask_attributes = {
  .name = "RtcInitTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartMainTask(void *argument);
void StartSoundTask(void *argument);
void StartColonTask(void *argument);
void StartCANopen(void *argument);
void StartRtcInitTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  soundQueueHandle = osMessageQueueNew(8, sizeof(char[32]), NULL);
  configASSERT(soundQueueHandle != NULL);

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

  /* creation of CANopenTask */
  CANopenTaskHandle = osThreadNew(StartCANopen, NULL, &CANopenTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* creation of RtcInitTask */
  RtcInitTaskHandle = osThreadNew(StartRtcInitTask, NULL, &RtcInitTask_attributes);
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
  // Plays the startup sound and draws the displays (already left blank by
  // timeCircuit_control_init()) together - see
  // timeCircuit_control_playStartupSequence(). Deliberately runs before
  // imu_bno055_init() below: that call blocks for ~2.1s total (a mandatory
  // 1500ms power-on settle delay plus BNO055 mode-switch delays), which
  // used to sit in front of the startup sequence and made the displays
  // stay dark for that whole time on every boot. Nothing at boot depends
  // on the IMU being ready immediately, so it's fine for its init to
  // finish a couple seconds into the sound/display sequence instead - the
  // main loop below still only starts once it's done either way.
  //
  // Give RtcInitTask a short bounded head start first: a normal DS3231 read
  // (bus recovery + init) finishes in a handful of ms, but without waiting
  // at all, playStartupSequence() below usually wins the race and draws the
  // present display with yesterday's last-saved SD time, which then jumps
  // to the real time a couple of main-loop ticks later. Capped at
  // RTC_INIT_WAIT_TIMEOUT_MS so a hung/missing RTC still can never delay
  // boot beyond that - it just falls through and draws the SD-stored value
  // as before, same as if this wait wasn't here at all.
  uint32_t rtcWaitStartTick = HAL_GetTick();
  while (!gRtcInitDone && (HAL_GetTick() - rtcWaitStartTick) < RTC_INIT_WAIT_TIMEOUT_MS)
  {
    osDelay(2);
  }

  timeCircuit_control_playStartupSequence(gTimeCircuitConfig);

  imu_bno055_init();

  /* Infinite loop */
  for(;;)
  {
    timeCircuit_control_update(gTimeCircuitConfig);
    imu_bno055_service();
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
  extern char colonSound_filename[];
  char currentSound[32];

  for(;;)
  {
    bool haveSound = false;
    bool playingColon = false;

    if (osMessageQueueGet(soundQueueHandle, &currentSound, NULL, 0) == osOK) {
      haveSound = true;
    } else if (gColonPending) {
      gColonPending = false;
      if ((HAL_GetTick() - gColonRequestTick) <= COLON_STALE_MS) {
        strncpy(currentSound, colonSound_filename, sizeof(currentSound) - 1);
        currentSound[sizeof(currentSound) - 1] = '\0';
        haveSound = true;
        playingColon = true;
      }
      // else: too much time has passed since this beep was due (SoundTask
      // was busy with something else) - drop it rather than play it late.
    }

    if (haveSound)
    {
      soundEffects_playSound(gSoundEffectConfig, gStorageConfig, currentSound);
      while (gIsPlaying)
      {
        // Only the low-priority colon beep gets preempted by a real request
        // arriving mid-playback. Two real sounds queued back-to-back (e.g.
        // fast keypad typing) must NOT interrupt each other - each plays to
        // completion in order, they just never wait behind a colon beep.
        if (playingColon)
        {
          char interrupting[32];
          if (osMessageQueueGet(soundQueueHandle, &interrupting, NULL, 0) == osOK)
          {
            soundEffects_playSound(gSoundEffectConfig, gStorageConfig, interrupting);
            playingColon = false;
            continue;
          }
        }
        soundEffects_update(gSoundEffectConfig, gStorageConfig);
        osDelay(1);
      }
    }
    else
    {
      osDelay(1);
    }
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


    osDelay(500);

}
  /* USER CODE END StartColonTask */
}

/* USER CODE BEGIN Header_StartCANopen */
/**
* @brief Function implementing the CANopenTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartCANopen */
void StartCANopen(void *argument)
{
  /* USER CODE BEGIN StartCANopen */

  CANopenNodeSTM32 canOpenNodeSTM32;
  canOpenNodeSTM32.CANHandle = &hcan1;
  canOpenNodeSTM32.HWInitFunction = MX_CAN1_Init;
  canOpenNodeSTM32.timerHandle = &htim7;
  canOpenNodeSTM32.desiredNodeID = 21;
  canOpenNodeSTM32.baudrate = 1000;
  canopen_app_init(&canOpenNodeSTM32);

  HAL_TIM_Base_Start_IT(&htim7);
  HAL_CAN_Start(&hcan1);
  HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
  /* Infinite loop */
  for(;;)
  {

    canopen_app_process();

    vTaskDelay(pdMS_TO_TICKS(1));
  }
  /* USER CODE END StartCANopen */
}

/**
* @brief One-shot task: initializes the external RTC after the scheduler is
* already running. If the RTC I2C bus is stuck or the hardware is faulty,
* this task alone stalls - every other task (keypad/display, sound, colon,
* CAN) continues completely unaffected, guaranteeing the RTC can never again
* prevent the rest of the board from booting and working normally.
* @param argument: Not used
* @retval None
*/
void StartRtcInitTask(void *argument)
{
  timeCircuit_control_initRTC(gTimeCircuitConfig);

  // Signal StartMainTask's boot-time wait (see above) that the RTC attempt
  // is over, whether it succeeded or not - either way there's nothing more
  // for that wait to usefully wait for.
  gRtcInitDone = true;

  // One-shot - nothing left to do, so just idle forever rather than exit
  // (avoids relying on osThreadExit/OS task cleanup semantics).
  for(;;)
  {
    osDelay(1000);
  }
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

