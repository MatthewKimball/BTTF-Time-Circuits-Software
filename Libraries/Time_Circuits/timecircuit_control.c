/*
 * timecircuit_control.c
 *
 *  Created on: Nov 7, 2024
 *      Author: Professor Gizmo
 */
#include <timecircuit_control.h>
#include "sound_effects.h"
#include "cmsis_os.h"
#include "gpio.h"
#include <stdint.h>
#include "imu.h"
//#include "ds3231_for_stm32_hal.h"
#include "ds3231.h"
#include <stdbool.h>

#define DESTINATION_DISPLAY_I2C_ADDRESS     0x71
#define PRESENT_DISPLAY_I2C_ADDRESS         0x72
#define DEPARTED_DISPLAY_I2C_ADDRESS        0x74

#define KEYPAD_WHITE_INDICATOR_PIN          GPIO_PIN_3
#define KEYPAD_WHITE_INDICATOR_GPIO_PORT    GPIOA

#define GLITCH_SWITCH_PIN                   GPIO_PIN_5
#define GLITCH_SWITCH_GPIO_PORT             GPIOB

#define TIME_TRAVEL_SWITCH_PIN              GPIO_PIN_11
#define TIME_TRAVEL_SWITCH_GPIO_PORT        GPIOC

#define KEYPAD_ENTER_SWITCH_PIN             GPIO_PIN_1
#define KEYPAD_ENTER_SWITCH_GPIO_PORT       GPIOB

#define KEYPAD_COLUMN_1_SWITCH_PIN          GPIO_PIN_2
#define KEYPAD_COLUMN_1_SWITCH_GPIO_PORT    GPIOC
#define KEYPAD_COLUMN_2_SWITCH_PIN          GPIO_PIN_0
#define KEYPAD_COLUMN_2_SWITCH_GPIO_PORT    GPIOC
#define KEYPAD_COLUMN_3_SWITCH_PIN          GPIO_PIN_4
#define KEYPAD_COLUMN_3_SWITCH_GPIO_PORT    GPIOA

#define KEYPAD_ROW_1_SWITCH_PIN             GPIO_PIN_1
#define KEYPAD_ROW_1_SWITCH_GPIO_PORT       GPIOC
#define KEYPAD_ROW_2_SWITCH_PIN             GPIO_PIN_1
#define KEYPAD_ROW_2_SWITCH_GPIO_PORT       GPIOA
#define KEYPAD_ROW_3_SWITCH_PIN             GPIO_PIN_4
#define KEYPAD_ROW_3_SWITCH_GPIO_PORT       GPIOC
#define KEYPAD_ROW_4_SWITCH_PIN             GPIO_PIN_3
#define KEYPAD_ROW_4_SWITCH_GPIO_PORT       GPIOC

#define MUTE_SWITCH_PIN                     GPIO_PIN_10
#define MUTE_SWITCH_GPIO_PORT               GPIOC

#define AMPLIFIER_SHUTDOWN_PIN              GPIO_PIN_14
#define AMPLIFIER_SHUTDOWN_GPIO_PORT        GPIOB

#define KEYPAD_DEBOUNCE_TIME_MS             50
#define ENTER_SWITCH_DEBOUNCE_TIME_MS       50
#define GLITCH_SWITCH_DEBOUNCE_TIME_MS      100
#define TIME_TRAVEL_SWITCH_DEBOUNCE_TIME_MS 100
#define PRESENT_TIME_UPDATE_TIME_MS         30000
#define DISPLAY_DELAY_MS                    500

#define MAXIMUM_DATETIME_INPUT_CHARS        12
#define NUMBER_OF_DATETIME_DISPLAYS         3

#define MAXIMUM_GLITCH_RANDOM_PERIOD_MS     60000

extern osMessageQueueId_t soundQueueHandle;
extern volatile bool gGlitchDoubleHit;

const uint8_t   gDefaultDestinationTime[]     = {1,0,2,7,1,9,8,5,1,1,0,0};
const uint8_t   gDefaultPresentTime[]         = {0,7,0,5,2,0,1,5,2,3,5,9};
const uint8_t   gDefaultLastDepartedTime[]    = {1,1,1,2,1,9,5,5,2,2,0,4};
const uint8_t   gGlitchDestinationTime[]      = {0,1,0,1,1,8,8,5,1,2,0,0};
const char      gGlitchDisplayDate[]          = "JAN0118851200";
const char      gGlitchDisplayChars[]         = "   0090009000";
const char      gStoredDateTimeFileName[]     = "svDates.txt";
const uint32_t  gGlitchTimeDelay[]            = {500, 410, 90};

const Keypad3x4w_PinConfig_t gKeypadPinConfig[] =
{
  {
  /* COLs Pins Info */
  {KEYPAD_COLUMN_1_SWITCH_GPIO_PORT, KEYPAD_COLUMN_2_SWITCH_GPIO_PORT, KEYPAD_COLUMN_3_SWITCH_GPIO_PORT},
  {KEYPAD_COLUMN_1_SWITCH_PIN, KEYPAD_COLUMN_2_SWITCH_PIN, KEYPAD_COLUMN_3_SWITCH_PIN},
  /* ROWs Pins Info*/
  {KEYPAD_ROW_1_SWITCH_GPIO_PORT, KEYPAD_ROW_2_SWITCH_GPIO_PORT, KEYPAD_ROW_3_SWITCH_GPIO_PORT, KEYPAD_ROW_4_SWITCH_GPIO_PORT},
  {KEYPAD_ROW_1_SWITCH_PIN, KEYPAD_ROW_2_SWITCH_PIN, KEYPAD_ROW_3_SWITCH_PIN, KEYPAD_ROW_4_SWITCH_PIN}
  }
};

struct TimeCircuit_Control_Config_Tag
{
  I2C_HandleTypeDef*          hi2c_display;
  I2C_HandleTypeDef*          hi2c_rtc;
  RTC_HandleTypeDef*          hrtc;
  SPI_HandleTypeDef*          hspi;
  I2S_HandleTypeDef*          hi2s;

  DateTime_Display_Config_t*  pDestinationTime;
  DateTime_Display_Config_t*  pPresentTime;
  DateTime_Display_Config_t*  pLastDepartedTime;
  Keypad3x4w_Config_t*        pTimeCircuitKeypad;

  RTC_TimeTypeDef             hRtcTime;
  RTC_DateTypeDef             hRtcDate;

  uint8_t                     keypadInput[MAXIMUM_DATETIME_INPUT_CHARS];
  uint8_t                     keypadInputValue;
  uint8_t                     keypadInputCount;

  StorageDevice_Config_t*     pStorageDeviceConfig;
  SoundEffects_Config_t*      pSoundEffectConfig;

} TimeCircuit_Control_Config;


TimeCircuit_Control_Status_t timeCircuit_control_checkButtonActivation(const bool* const isbuttonActivated,
    bool* hasButtonActivated, uint32_t* previousTimeMS, uint32_t debounceTime);
TimeCircuit_Control_Status_t timeCircuit_control_clearDisplays(TimeCircuit_Control_Config_t* const pConfig);
TimeCircuit_Control_Status_t timeCircuit_control_setDefaultDateTimes(TimeCircuit_Control_Config_t* const pConfig);
TimeCircuit_Control_Status_t timeCircuit_control_getRTCMinute(TimeCircuit_Control_Config_t * const pConfig,
    uint8_t * currentMinutes);


TimeCircuit_Control_Config_t* timeCircuit_control_init(I2C_HandleTypeDef* const hi2c_display, I2C_HandleTypeDef* const hi2c_rtc,
    RTC_HandleTypeDef* hrtc, SPI_HandleTypeDef* hspi, I2S_HandleTypeDef* hi2s)
{
  TimeCircuit_Control_Config_t* pConfig = malloc(sizeof(TimeCircuit_Control_Config_t));
  pConfig->hi2c_display = hi2c_display;
  pConfig->hi2c_rtc = hi2c_rtc;
  pConfig->hrtc = hrtc;
  pConfig->hspi = hspi;
  pConfig->hi2s = hi2s;


  //Initialise the time circuit displays
  pConfig->pDestinationTime  = dateTime_display_init(hi2c_display, DESTINATION_DISPLAY_I2C_ADDRESS);
  pConfig->pPresentTime      = dateTime_display_init(hi2c_display, PRESENT_DISPLAY_I2C_ADDRESS);
  pConfig->pLastDepartedTime = dateTime_display_init(hi2c_display, DEPARTED_DISPLAY_I2C_ADDRESS);


  //Initialise the time circuit keypad
  pConfig->pTimeCircuitKeypad = keypad3x4w_init(gKeypadPinConfig);

  //Play TC Start Up Sound
//  char filename[] = "locked.wav";
//  osMessageQueuePut(soundQueueHandle, &filename, 0, 0);

  //Set displays to last stored values or defaults
  timeCircuit_control_setDefaultDateTimes(pConfig);

  //Update display with retrieved date times
  timeCircuit_control_updateDisplays(pConfig);

  //Enable external RTC
  #if defined(SET_EXTERNAL_RTC)
  HAL_GPIO_WritePin( EXT_RTC_RST_GPIO_Port, EXT_RTC_RST_Pin, GPIO_PIN_SET);
  #endif

  //Update RTC with retrieved present date time
  timeCircuit_control_setRtcDateTime(pConfig);

  return pConfig;
}

TimeCircuit_Control_Status_t timeCircuit_control_deInit(TimeCircuit_Control_Config_t* const pConfig)
{
  storageDevice_demountDrive(pConfig->pStorageDeviceConfig);
  free (pConfig);
  return 1;
}

TimeCircuit_Control_Status_t timeCircuit_control_clearDisplays(TimeCircuit_Control_Config_t* const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess = 1;

  isSuccess &= dateTime_clearDisplay(pConfig->pDestinationTime);
  isSuccess &= dateTime_clearDisplay(pConfig->pPresentTime);
  isSuccess &= dateTime_clearDisplay(pConfig->pLastDepartedTime);

  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_control_setDefaultDateTimes(TimeCircuit_Control_Config_t* const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess = 1;

  isSuccess &= dateTime_setDisplayData(pConfig->pDestinationTime, gDefaultDestinationTime);
  isSuccess &= dateTime_setDisplayData(pConfig->pPresentTime, gDefaultPresentTime);
  isSuccess &= dateTime_setDisplayData(pConfig->pLastDepartedTime, gDefaultLastDepartedTime);

  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_control_updateDisplays(TimeCircuit_Control_Config_t* const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess = false;

  isSuccess = dateTime_updateDisplay(pConfig->pDestinationTime);
  isSuccess &= dateTime_updateDisplay(pConfig->pPresentTime);
  isSuccess &= dateTime_updateDisplay(pConfig->pLastDepartedTime);

  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_control_checkButtonActivation(const bool* const isbuttonActivated,
    bool* hasButtonActivated, uint32_t* previousTimeMS, uint32_t debounceTime)
{
  TimeCircuit_Control_Status_t hasStateChanged = 0;
  uint32_t currentTimeMS = HAL_GetTick();

  //Verify that the button state hasn't changed
  if (*isbuttonActivated != *hasButtonActivated)
  {
    //Filter out false positive button activations
    if ((currentTimeMS - *previousTimeMS) > debounceTime)
    {
      *previousTimeMS = currentTimeMS;
      hasStateChanged = true;
      *hasButtonActivated = *isbuttonActivated;
    }
  }
  return hasStateChanged;
}

TimeCircuit_Control_Status_t timeCircuit_control_readInputDateTime(TimeCircuit_Control_Config_t * const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess = 1;
  bool isButtonActivated                  = false;
  bool hasButtonStateChanged              = false;

  static bool hasButtonActivated          = false;
  static uint32_t previousTime            = 0;


  isButtonActivated  = keypad3x4w_readKeypad(pConfig->pTimeCircuitKeypad, &pConfig->keypadInputValue);
  hasButtonStateChanged = timeCircuit_control_checkButtonActivation(&isButtonActivated, &hasButtonActivated,
        &previousTime, KEYPAD_DEBOUNCE_TIME_MS);

  if ((hasButtonStateChanged == true) && (isButtonActivated == true))
  {
    pConfig->keypadInput[pConfig->keypadInputCount] = pConfig->keypadInputValue;
    pConfig->keypadInputCount++;

    switch(pConfig->keypadInputValue)
    {
      case 0:
        char filename[] = "Dtmf-0.wav";
        osMessageQueuePut(soundQueueHandle, &filename, 0, 0);
        break;
      case 1:
        char filename1[] = "Dtmf-1.wav";
        osMessageQueuePut(soundQueueHandle, &filename1, 0, 0);
        break;
      case 2:
        char filename2[] = "Dtmf-2.wav";
        osMessageQueuePut(soundQueueHandle, &filename2, 0, 0);
        break;
      case 3:
        char filename3[] = "Dtmf-3.wav";
        osMessageQueuePut(soundQueueHandle, &filename3, 0, 0);
        break;
      case 4:
        char filename4[] = "Dtmf-4.wav";
        osMessageQueuePut(soundQueueHandle, &filename4, 0, 0);
        break;
      case 5:
        char filename5[] = "Dtmf-5.wav";
        osMessageQueuePut(soundQueueHandle, &filename5, 0, 0);
        break;
      case 6:
        char filename6[] = "Dtmf-6.wav";
        osMessageQueuePut(soundQueueHandle, &filename6, 0, 0);
        break;
      case 7:
        char filename7[] = "Dtmf-7.wav";
        osMessageQueuePut(soundQueueHandle, &filename7, 0, 0);
        break;
      case 8:
        char filename8[] = "Dtmf-8.wav";
        osMessageQueuePut(soundQueueHandle, &filename8, 0, 0);
        break;
      case 9:
        char filename9[] = "Dtmf-9.wav";
        osMessageQueuePut(soundQueueHandle, &filename9, 0, 0);
        break;

    }
  }

  if (pConfig->keypadInputCount >= 12)
  {
    pConfig->keypadInputCount = 0;
  }

  return isSuccess;
}
#if defined(SET_INTERNAL_RTC)
TimeCircuit_Control_Status_t timeCircuit_control_getRtcDateTime(TimeCircuit_Control_Config_t * const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess = 1;

  //Retrieve RTC Date Time Data
  isSuccess &= HAL_RTC_GetTime(pConfig->hrtc, &pConfig->hRtcTime, RTC_FORMAT_BIN);
  isSuccess &= HAL_RTC_GetDate(pConfig->hrtc, &pConfig->hRtcDate, RTC_FORMAT_BIN);

  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_control_setRtcDateTime(TimeCircuit_Control_Config_t * const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess = 1;

  //Retrieve default RTC date time data
  isSuccess &= timeCircuit_control_getRtcDateTime(pConfig);

  //Get present date time for RTC date time
  isSuccess &= dateTime_getRtcDateTimeData(pConfig->pPresentTime, &pConfig->hRtcDate, &pConfig->hRtcTime);

  //Set RTC with present time data
  isSuccess &= HAL_RTC_SetTime(pConfig->hrtc, &pConfig->hRtcTime, RTC_FORMAT_BIN);
  isSuccess &= HAL_RTC_SetDate(pConfig->hrtc, &pConfig->hRtcDate, RTC_FORMAT_BIN);

  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_control_getRTCMinute(TimeCircuit_Control_Config_t * const pConfig,
    uint8_t * currentMinutes)
{
  TimeCircuit_Control_Status_t isSuccess = 1;

  isSuccess &= HAL_RTC_GetTime(pConfig->hrtc, &pConfig->hRtcTime, RTC_FORMAT_BIN);
  *currentMinutes = pConfig->hRtcTime.Minutes;

  return isSuccess;
}


#elif defined(SET_EXTERNAL_RTC)
TimeCircuit_Control_Status_t timeCircuit_control_getRtcDateTime(TimeCircuit_Control_Config_t * const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess = 1;

  //Retrieve RTC Date Time Data
  isSuccess &= DS3231_GetDateTime(pConfig->hi2c_rtc,
                                       &pConfig->hRtcTime,
                                       &pConfig->hRtcDate);
//  pConfig->hRtcTime.Hours   = DS3231_GetHour();
//  pConfig->hRtcTime.Minutes = DS3231_GetMinute();
//  pConfig->hRtcTime.Seconds = DS3231_GetSecond();
//
//  pConfig->hRtcDate.Date    = DS3231_GetDate();
//  pConfig->hRtcDate.Month   = DS3231_GetMonth();
//  pConfig->hRtcDate.Year    = DS3231_GetYear();

  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_control_setRtcDateTime(TimeCircuit_Control_Config_t * const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess = 1;

  //Retrieve default RTC date time data
  isSuccess &= timeCircuit_control_getRtcDateTime(pConfig);

  //Get present date time for RTC date time
  isSuccess &= dateTime_getRtcDateTimeData(pConfig->pPresentTime, &pConfig->hRtcDate, &pConfig->hRtcTime);

  //Set RTC with present date time data
  isSuccess &= DS3231_SetDateTime(pConfig->hi2c_rtc,
      &pConfig->hRtcTime,
      &pConfig->hRtcDate);
//  DS3231_SetFullTime(pConfig->hRtcTime.Hours,
//                           pConfig->hRtcTime.Minutes,
//                           pConfig->hRtcTime.Seconds);
//
//  DS3231_SetFullDate(pConfig->hRtcDate.Date,
//                         pConfig->hRtcDate.Month,
//                         1,
//                         0);

  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_control_getRTCMinute(TimeCircuit_Control_Config_t * const pConfig,
    uint8_t * currentMinutes)
{
  TimeCircuit_Control_Status_t isSuccess = 1;

  isSuccess &= DS3231_GetDateTime(pConfig->hi2c_rtc,
                                       &pConfig->hRtcTime,
                                       &pConfig->hRtcDate);

  *currentMinutes = pConfig->hRtcTime.Minutes;

  //*currentMinutes = DS3231_GetMinute();

  return isSuccess;
}

#endif



TimeCircuit_Control_Status_t timeCircuit_control_saveDateTimes(TimeCircuit_Control_Config_t * const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess = false;

  char    writeBuf[(MAXIMUM_DATETIME_INPUT_CHARS * 3) + 1];
  uint8_t bufferCount = 0;
  DateTime_Display_Config_t* pDateTimeDisplays[] = {pConfig->pDestinationTime, pConfig->pPresentTime, pConfig->pLastDepartedTime};

    //Prepare buffer with datetimes
  for (uint8_t displayCount = 0; displayCount < 3; displayCount++)
  {
    dateTime_convertDateTimeToChar(pDateTimeDisplays[displayCount], writeBuf, sizeof(writeBuf), &bufferCount);
  }

  //Write datetime data to SD card
  // isSuccess = storageDevice_writeFile(pConfig->pStorageDeviceConfig, writeBuf, sizeof(writeBuf), gStoredDateTimeFileName);

  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_control_updateStartUpDateTimes(TimeCircuit_Control_Config_t * const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess = false;

  //char* pReadBuf = malloc(MAXIMUM_DATETIME_INPUT_CHARS * 3);
  char pReadBuf[(MAXIMUM_DATETIME_INPUT_CHARS * 3) + 1];
  uint8_t pStartUpDateTime[MAXIMUM_DATETIME_INPUT_CHARS];

  DateTime_Display_Config_t* pDateTimeDisplays[] = {pConfig->pDestinationTime, pConfig->pPresentTime, pConfig->pLastDepartedTime};


  //Read datetime data from SD card
 // isSuccess = storageDevice_readFile(pConfig->pStorageDeviceConfig, pReadBuf, sizeof(pReadBuf), gStoredDateTimeFileName);

  //Check read was successful, if not set to default values
  if(isSuccess)
  {
    for (uint8_t displayCount = 0; displayCount < 3; displayCount++)
    {
      for (uint8_t characterCount = 0; characterCount < MAXIMUM_DATETIME_INPUT_CHARS; characterCount++)
      {
        pStartUpDateTime[characterCount] = pReadBuf[characterCount + (displayCount * MAXIMUM_DATETIME_INPUT_CHARS)] - '0';
      }
      isSuccess &= dateTime_setDisplayData((pDateTimeDisplays[displayCount]), pStartUpDateTime);
    }
      if (isSuccess == false)
      {
      isSuccess &= timeCircuit_control_updateDisplays(pConfig);
      }
  }

  //Set default values if SD Card values not read or invalid
  if (isSuccess == false)
  {
    isSuccess &= timeCircuit_control_setDefaultDateTimes(pConfig);
  }


  //free(pReadBuf);
  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_control_setDefaultDisplays(TimeCircuit_Control_Config_t * const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess = 1;

  //Reset to displays to default
  isSuccess &= timeCircuit_control_clearDisplays(pConfig);
  isSuccess &= timeCircuit_control_setDefaultDateTimes(pConfig);
  isSuccess &= timeCircuit_control_updateDisplays(pConfig);
  pConfig->keypadInputCount = 0;

  isSuccess &= timeCircuit_control_setRtcDateTime(pConfig);

  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_control_updatePresentDateTime(TimeCircuit_Control_Config_t * const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess = 1;
  static uint8_t previousMinute = 0;
  uint8_t currentMinute = 0;

  isSuccess &=  timeCircuit_control_getRTCMinute(pConfig, &currentMinute);

  if (currentMinute != previousMinute)
  {
    //Retrieve RTC date time data
    isSuccess &= timeCircuit_control_getRtcDateTime(pConfig);

    //Set present date time to RTC date time
    isSuccess &= dateTime_setRtcDateTimeData(pConfig->pPresentTime, &pConfig->hRtcDate, &pConfig->hRtcTime);

    //Update present display
    isSuccess &= dateTime_updateDisplay(pConfig->pPresentTime);

    //Store new date time
    isSuccess &= timeCircuit_control_saveDateTimes(pConfig);

    previousMinute = pConfig->hRtcTime.Minutes;
  }

  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_control_updateTimeTravelDateTimes(TimeCircuit_Control_Config_t* const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess  = true;
  bool isButtonActivated                  = false;
  bool hasButtonStateChanged              = false;

  static bool hasButtonActivated          = false;
  static uint32_t previousTime            = 0;

  isButtonActivated  = HAL_GPIO_ReadPin(TIME_TRAVEL_SWITCH_GPIO_PORT, TIME_TRAVEL_SWITCH_PIN);
  hasButtonStateChanged = timeCircuit_control_checkButtonActivation(&isButtonActivated, &hasButtonActivated,
        &previousTime, TIME_TRAVEL_SWITCH_DEBOUNCE_TIME_MS);

  if (hasButtonStateChanged == true)
  {
    if (isButtonActivated != true )
    {
      //Clear displays
      isSuccess &= timeCircuit_control_clearDisplays(pConfig);

      //Play Sound
      char filename9[] = "locked.wav";
      osMessageQueuePut(soundQueueHandle, &filename9, 0, 0);

      //Delay Display Update
      HAL_Delay(DISPLAY_DELAY_MS);

      //Copy last time departed time data to present time
      isSuccess &= dateTime_copyDateTime(pConfig->pLastDepartedTime, pConfig->pPresentTime);

      //Copy present time data to destination time
      isSuccess &= dateTime_copyDateTime(pConfig->pPresentTime, pConfig->pDestinationTime);

      //Update displays with new date times
      isSuccess &= timeCircuit_control_updateDisplays(pConfig);

      //Set the RTC with new present time
      isSuccess &= timeCircuit_control_setRtcDateTime(pConfig);

      //Store new date time
      isSuccess &= timeCircuit_control_saveDateTimes(pConfig);
    }
  }
  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_control_updateDestinationDateTime(TimeCircuit_Control_Config_t* const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess  = true;
  bool isButtonActivated                  = false;
  bool hasButtonStateChanged              = false;

  static bool hasButtonActivated          = false;
  static uint32_t previousTime            = 0;

  isButtonActivated  = !HAL_GPIO_ReadPin(KEYPAD_ENTER_SWITCH_GPIO_PORT, KEYPAD_ENTER_SWITCH_PIN);
  hasButtonStateChanged = timeCircuit_control_checkButtonActivation(&isButtonActivated, &hasButtonActivated,
      &previousTime, ENTER_SWITCH_DEBOUNCE_TIME_MS);

  if (hasButtonStateChanged == true)
  {
    if (isButtonActivated == true )
    {
      //Activate Keypad White Indicator
      HAL_GPIO_WritePin(KEYPAD_WHITE_INDICATOR_GPIO_PORT, KEYPAD_WHITE_INDICATOR_PIN, GPIO_PIN_SET);
      //Clear destination date time
      isSuccess &= dateTime_clearDisplay(pConfig->pDestinationTime);
      //Update date time if a valid entry has submitted
      if (dateTime_setDisplayData(pConfig->pDestinationTime,pConfig->keypadInput))
      {
        //Play sound
        char filename[] = "enter.wav";
        osMessageQueuePut(soundQueueHandle, &filename, 0, 0);

        //Delay Display Update
        HAL_Delay(DISPLAY_DELAY_MS);
        isSuccess &= dateTime_updateDisplay(pConfig->pDestinationTime);
      }
      pConfig->keypadInputCount = 0;

      //Save new date times
      isSuccess &= timeCircuit_control_saveDateTimes(pConfig);
    }
    else
    {
      //Deactivate Keypad White Indicator
      HAL_GPIO_WritePin(KEYPAD_WHITE_INDICATOR_GPIO_PORT, KEYPAD_WHITE_INDICATOR_PIN, GPIO_PIN_RESET);
    }

  }
  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_control_updateGlitch(TimeCircuit_Control_Config_t* const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess  = true;
  static uint32_t previousTime            = 0;
  static uint32_t randomFaultTime         = 0;
  static uint8_t  stateCount              = 0;

  bool bIsButtonActivated                 = false;
  bool bHasButtonStateChanged              = false;

  static bool hasButtonActivated          = false;
  static uint32_t previousTimeMS          = 0;
  static uint32_t previousFaultTime       = 0;

  bIsButtonActivated  = !HAL_GPIO_ReadPin(GLITCH_SWITCH_GPIO_PORT, GLITCH_SWITCH_PIN);
  bHasButtonStateChanged = timeCircuit_control_checkButtonActivation(&bIsButtonActivated, &hasButtonActivated,
      &previousTimeMS, GLITCH_SWITCH_DEBOUNCE_TIME_MS);

  //Checks if glitch switch has been enabled
  if (bHasButtonStateChanged == true)
  {
    if (bIsButtonActivated == true )
    {
      randomFaultTime = rand() % MAXIMUM_GLITCH_RANDOM_PERIOD_MS;
      previousFaultTime = HAL_GetTick();
    }
    else
    {
      isSuccess &= dateTime_clearDisplay(pConfig->pDestinationTime);
      isSuccess = dateTime_updateDisplay(pConfig->pDestinationTime);

      return true;
    }
  }

  //Update display with glitch
  if (((HAL_GetTick() - previousTime) > gGlitchTimeDelay[stateCount]) &&
      ((HAL_GetTick() - previousFaultTime) > randomFaultTime) &&
      (bIsButtonActivated == true) && (gGlitchDoubleHit  == false))
  {
    switch (stateCount)
    {
      case 0:
        isSuccess &= dateTime_clearDisplay(pConfig->pDestinationTime);
        stateCount++;
        break;
      case 1:
        isSuccess &= dateTime_updateDisplayGlitch(pConfig->pDestinationTime, gGlitchDisplayChars);
        stateCount++;
        break;
      case 2:
        isSuccess &= dateTime_updateDisplayGlitch(pConfig->pDestinationTime,gGlitchDisplayDate);
        stateCount = 0;
//        char filename[] = "glitch2.wav";
//        osMessageQueuePut(soundQueueHandle, &filename, 0, 0);
        break;

      default:
        break;
     }
    previousTime = HAL_GetTick();
  }

  // Clear glitch
  if (gGlitchDoubleHit == true)
  {
    //Clear glitching display
    isSuccess &= dateTime_clearDisplay(pConfig->pDestinationTime);
    HAL_Delay(500);
    isSuccess &= dateTime_updateDisplayGlitch(pConfig->pDestinationTime, gGlitchDisplayChars);
    HAL_Delay(100);
    isSuccess = dateTime_updateDisplay(pConfig->pDestinationTime);

    //Clear glitch flag
    gGlitchDoubleHit = false;

    //Generate new random time
    randomFaultTime = rand() % MAXIMUM_GLITCH_RANDOM_PERIOD_MS;

    //Reset glitch timer
    previousFaultTime = HAL_GetTick();


  }

  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit__setColonState(TimeCircuit_Control_Config_t* const pConfig, uint8_t colonOn)
{
  DateTime_Display_Status_t isSuccess   = 0;

  isSuccess |= dateTime_setLed(pConfig->pDestinationTime,   COLON_LED_SEGMENT_ADDRESS, (colonOn<<6));
  isSuccess |= dateTime_setLed(pConfig->pLastDepartedTime,  COLON_LED_SEGMENT_ADDRESS, (colonOn<<6));
  isSuccess |= dateTime_setLed(pConfig->pPresentTime,       COLON_LED_SEGMENT_ADDRESS, (colonOn<<6));

  return isSuccess;

}

TimeCircuit_Control_Status_t timeCircuit__toggleTimeColon(TimeCircuit_Control_Config_t* const pConfig)
{
  static uint8_t toogleStatus = 0;
  static uint32_t previousTime = 0;
  DateTime_Display_Status_t isSuccess   = 0;

  if ((HAL_GetTick()-previousTime) >= COLON_TIME_DELAY_MS)
  {
    toogleStatus = ((toogleStatus) == 0) ? 3 : 0;
    previousTime = HAL_GetTick();
    isSuccess |= dateTime_setLed(pConfig->pDestinationTime,   COLON_LED_SEGMENT_ADDRESS, (toogleStatus<<6));
    isSuccess |= dateTime_setLed(pConfig->pLastDepartedTime,  COLON_LED_SEGMENT_ADDRESS, (toogleStatus<<6));
    isSuccess |= dateTime_setLed(pConfig->pPresentTime,       COLON_LED_SEGMENT_ADDRESS, (toogleStatus<<6));


  }

  return isSuccess;
}



TimeCircuit_Control_Status_t timeCircuit_control_update(TimeCircuit_Control_Config_t * const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess = 1;

  //Update date times after time travel simulation event
  isSuccess &= timeCircuit_control_updateTimeTravelDateTimes(pConfig);

  //Read user input date time
  isSuccess &= timeCircuit_control_readInputDateTime(pConfig);

  //Update Destination Time from user input
  isSuccess &= timeCircuit_control_updateDestinationDateTime(pConfig);


  //Update Present Time from RTC
  isSuccess &= timeCircuit_control_updatePresentDateTime(pConfig);

  //Update Glitch
  isSuccess &= timeCircuit_control_updateGlitch(pConfig);


  return isSuccess;
}

