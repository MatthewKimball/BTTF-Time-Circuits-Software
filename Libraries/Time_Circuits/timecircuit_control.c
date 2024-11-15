/*
 * timecircuit_control.c
 *
 *  Created on: Nov 7, 2024
 *      Author: Professor Gizmo
 */
#include <timecircuit_control.h>


struct TimeCircuit_Control_Config_Tag
{
  I2C_HandleTypeDef*          hi2c;
  RTC_HandleTypeDef*          hrtc;
  DateTime_Display_Config_t*  pDestinationTime;
  DateTime_Display_Config_t*  pPresentTime;
  DateTime_Display_Config_t*  pLastDepartedTime;
  Keypad3x4w_Config_t*        pTimeCircuitKeypad;

  RTC_TimeTypeDef             hRtcTime;
  RTC_DateTypeDef             hRtcDate;

  uint8_t                     keypadInput[12];
  uint8_t                     keypadInputValue;
  uint8_t                     keypadInputCount;

} TimeCircuit_Control_Config;

TimeCircuit_Control_Config_t* timeCircuit_control_init(I2C_HandleTypeDef* const hi2c, RTC_HandleTypeDef* hrtc)
{
  TimeCircuit_Control_Config_t* pConfig = malloc(sizeof(TimeCircuit_Control_Config_t));
  pConfig->hi2c = hi2c;
  pConfig->hrtc = hrtc;

  //Initialise the time circuit displays
  pConfig->pDestinationTime  = dateTime_display_init(hi2c, DESTINATION_DISPLAY_I2C_ADDRESS);
  pConfig-> pPresentTime     = dateTime_display_init(hi2c, PRESENT_DISPLAY_I2C_ADDRESS);
  pConfig->pLastDepartedTime = dateTime_display_init(hi2c, DEPARTED_DISPLAY_I2C_ADDRESS);

  //Initialise the time circuit keypad
  pConfig->pTimeCircuitKeypad = keypad3x4w_init(timeCircuitKeypadPinParam);

  return pConfig;
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

  isSuccess &= dateTime_setDisplayData(pConfig->pDestinationTime, defaultDestinationTimeData);
  isSuccess &= dateTime_setDisplayData(pConfig->pPresentTime, defaultPresentTimeData);
  isSuccess &= dateTime_setDisplayData(pConfig->pLastDepartedTime, defaultLastDepartedTimeData);

  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_control_updateDisplays(TimeCircuit_Control_Config_t* const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess = 1;

  isSuccess &= dateTime_updateDisplay(pConfig->pDestinationTime);
  isSuccess &= dateTime_updateDisplay(pConfig->pPresentTime);
  isSuccess &= dateTime_updateDisplay(pConfig->pLastDepartedTime);

  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_control_checkButtonActivation(const bool* const isbuttonActivated,
    bool* hasButtonActivated, uint32_t* previousTimeMS)
{
  TimeCircuit_Control_Status_t hasStateChanged = 0;
  uint32_t currentTimeMS = HAL_GetTick();

  //Verify that the button state hasn't changed
  if (*isbuttonActivated != *hasButtonActivated)
  {
    //Filter out false positive button activations
    if ((currentTimeMS - *previousTimeMS) > BUTTON_DEBOUNCE_TIME_MS)
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
        &previousTime);

  if ((hasButtonStateChanged == true) && (isButtonActivated == true))
  {
    pConfig->keypadInput[pConfig->keypadInputCount] = pConfig->keypadInputValue;
    pConfig->keypadInputCount++;
  }

  if (pConfig->keypadInputCount >= 12)
  {
    pConfig->keypadInputCount = 0;
  }

  return isSuccess;
}

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
  isSuccess &= HAL_RTC_GetTime(pConfig->hrtc, &pConfig->hRtcTime, RTC_FORMAT_BIN);

  if (pConfig->hRtcTime.Minutes != previousMinute)
  {
    //Retrieve RTC date time data
    isSuccess &= timeCircuit_control_getRtcDateTime(pConfig);

    //Set present date time to RTC date time
    isSuccess &= dateTime_setRtcDateTimeData(pConfig->pPresentTime, &pConfig->hRtcDate, &pConfig->hRtcTime);

    //Update present display
    isSuccess &= dateTime_updateDisplay(pConfig->pPresentTime);

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
        &previousTime);

  if (hasButtonStateChanged == true)
  {
    if (isButtonActivated != true )
    {
      //Clear displays
      isSuccess &= timeCircuit_control_clearDisplays(pConfig);

      //Copy last time departed time data to present time
      isSuccess &= dateTime_copyDateTime(pConfig->pLastDepartedTime, pConfig->pPresentTime);

      //Copy present time data to destination time
      isSuccess &= dateTime_copyDateTime(pConfig->pPresentTime, pConfig->pDestinationTime);

      //Update displays with new date times
      isSuccess &= timeCircuit_control_updateDisplays(pConfig);

      //Set the RTC with new present time
      isSuccess &= timeCircuit_control_setRtcDateTime(pConfig);
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
      &previousTime);

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
        isSuccess &= dateTime_updateDisplay(pConfig->pDestinationTime);
      }
      pConfig->keypadInputCount = 0;
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


  if ((!HAL_GPIO_ReadPin(GLITCH_SWITCH_GPIO_PORT, GLITCH_SWITCH_PIN)) == true )
  {
    //After random interval trigger
    isSuccess &= dateTime_clearDisplay(pConfig->pDestinationTime);
    HAL_Delay(500);
    dateTime_updateDisplayGlitch(pConfig->pDestinationTime, glitchDisplayChars);
    HAL_Delay(80);
    isSuccess &= dateTime_setDisplayData(pConfig->pDestinationTime, glitchDestinationTimeData);
    isSuccess &= dateTime_updateDisplay(pConfig->pDestinationTime);
    HAL_Delay(500);

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

  //Update time circuit displays colons
  isSuccess &= dateTime_toggleTimeColon(pConfig->pDestinationTime, pConfig->pPresentTime, pConfig->pLastDepartedTime);

  //Update Present Time from RTC
  isSuccess &= timeCircuit_control_updatePresentDateTime(pConfig);

  //Update Glitch
  isSuccess &= timeCircuit_control_updateGlitch(pConfig);

  return isSuccess;
}

