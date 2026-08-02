/*
 * timecircuit_control.c
 *
 *  Created on: Nov 7, 2024
 *      Author: Professor Gizmo
 */
#include "timecircuit_control.h"
#include "datetime_display.h"
#include "keypad3x4w.h"
#include "storagedevice_control.h"
#include "sound_effects.h"
#include "imu.h"
#include "ds3231.h"
#include "cmsis_os.h"
#include "gpio.h"
#include "i2c.h"
#include <stdint.h>
#include <stdbool.h>
#include "301/CO_ODinterface.h"
#include "OD.h"

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

#define KEYPAD_DEBOUNCE_TIME_MS             50
#define ENTER_SWITCH_DEBOUNCE_TIME_MS       50
#define GLITCH_SWITCH_DEBOUNCE_TIME_MS      100
#define TIME_TRAVEL_SWITCH_DEBOUNCE_TIME_MS 100
#define PRESENT_TIME_UPDATE_TIME_MS         30000
#define DISPLAY_DELAY_MS                    500

#define MAXIMUM_DATETIME_INPUT_CHARS        12
#define NUMBER_OF_DATETIME_DISPLAYS         3


/* ---- Bit masks for Function Control (0x2200:05)---- */
#define FUNC_CTRL_CLEAR_ALL_DISPLAYS          (1U << 0)
#define FUNC_CTRL_UPDATE_ALL_DISPLAYS         (1U << 1)
#define FUNC_CTRL_SET_ALL_DISPLAYS            (1U << 2)
#define FUNC_CTRL_UPDATE_DESTINATION_DATE     (1U << 3)
#define FUNC_CTRL_UPDATE_RTC                  (1U << 4)
#define FUNC_CTRL_SAVE_DATES                  (1U << 5)

/* ---- Bit masks for Settings (0x2300:04) ---- */
#define SETBIT_GLITCH_ENABLE          (1u << 0)  // Bit0
#define SETBIT_MUTE_COLON_SOUND       (1u << 1)  // Bit1
#define SETBIT_MUTE_ALL               (1u << 2)  // Bit2
#define SETBIT_UPDATE_IMU_SETTINGS    (1u << 3)  // Bit3 - one-shot
#define SETBIT_UPDATE_GLITCH_SETTINGS (1u << 4)  // Bit4 - one-shot



extern osMessageQueueId_t soundQueueHandle;
extern volatile bool gColonPending;
extern volatile uint32_t gColonRequestTick;
extern StorageDevice_Config_t* gStorageConfig;
extern volatile bool gGlitchDoubleHit;
extern bool gSoundMuteSw;
extern bool gSoundRemoteMuteAll;
extern bool gSoundRemoteMuteColon;



const uint8_t   gDefaultDestinationTime[]     = {1,0,2,7,1,9,8,5,1,1,0,0};
const uint8_t   gDefaultPresentTime[]         = {0,7,0,5,2,0,1,5,2,3,5,9};
const uint8_t   gDefaultLastDepartedTime[]    = {1,1,1,2,1,9,5,5,2,2,0,4};
const uint8_t   gGlitchDestinationTime[]      = {0,1,0,1,1,8,8,5,1,2,0,0};
const char      gGlitchDisplayDate[]          = "JAN0118851200";
const char      gGlitchDisplayChars[]         = "   0090009000";
const char      gStoredDateTimeFileName[]     = "svDates.txt";
const uint32_t  gGlitchTimeDelay[]            = {500, 410, 90};

uint32_t gGlitchPeriodMs = 60000;

char keypadSound_One_filename[]   = "Dtmf-1.wav";
char keypadSound_Two_filename[]   = "Dtmf-2.wav";
char keypadSound_Three_filename[] = "Dtmf-3.wav";
char keypadSound_Four_filename[]  = "Dtmf-4.wav";
char keypadSound_Five_filename[]  = "Dtmf-5.wav";
char keypadSound_Six_filename[]   = "Dtmf-6.wav";
char keypadSound_Seven_filename[] = "Dtmf-7.wav";
char keypadSound_Eight_filename[] = "Dtmf-8.wav";
char keypadSound_Nine_filename[]  = "Dtmf-9.wav";
char keypadSound_Zero_filename[]  = "Dtmf-0.wav";

char lockedSound_filename[]  = "locked.wav";
char colonSound_filename[]  = "beep6.wav";
char gitchSound_filename[]  = "glitch2.wav";
char enterSound_filename[]  = "enter2.wav";


//Switches
bool gGlitchSw        = false;
bool gTimeTravelSw    = false;
bool gKeypadEnterSw   = false;

bool gGlitchRemoteEnable = false;



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

  SoundEffects_Config_t*      pSoundEffectConfig;

} TimeCircuit_Control_Config;

typedef struct {
    TimeCircuit_Control_State_t currentState;
    uint8_t previousRemoteRequestState;
    uint32_t stateEntryTick;
    bool inputDateValid;
    bool receivedRemoteStateCommand;
} TimeTravelContext_t;


TimeCircuit_Control_Status_t timeCircuit_control_checkButtonActivation(const bool* const isbuttonActivated,
    bool* hasButtonActivated, uint32_t* previousTimeMS, uint32_t debounceTime);
TimeCircuit_Control_Status_t timeCircuit_control_clearDisplays(TimeCircuit_Control_Config_t* const pConfig);
TimeCircuit_Control_Status_t timeCircuit_control_setDefaultDateTimes(TimeCircuit_Control_Config_t* const pConfig);
TimeCircuit_Control_Status_t timeCircuit_control_getRTCMinute(TimeCircuit_Control_Config_t * const pConfig,
    uint8_t * currentMinutes);
 TimeCircuit_Control_Status_t timeCircuit_control_setRtcDateTime(TimeCircuit_Control_Config_t * const pConfig);
 TimeCircuit_Control_Status_t timeCircuit_control_getRtcDateTime(TimeCircuit_Control_Config_t * const pConfig);
 TimeCircuit_Control_Status_t timeCircuit_updateButtonStates (void);
 static void timeCircuit_processFunctionControl(TimeCircuit_Control_Config_t* const pConfig,
     TimeTravelContext_t *ctx);
 TimeCircuit_Control_Status_t timeCircuit_control_isRemoteDateValid(DateTime_Display_Config_t* pDateTimeDisplayData);
TimeCircuit_Control_Config_t* timeCircuit_control_init(I2C_HandleTypeDef* const hi2c_display, I2C_HandleTypeDef* const hi2c_rtc,
    RTC_HandleTypeDef* hrtc, I2S_HandleTypeDef* hi2s)

{
  TimeCircuit_Control_Config_t* pConfig = malloc(sizeof(TimeCircuit_Control_Config_t));
  pConfig->hi2c_display = hi2c_display;
  pConfig->hi2c_rtc = hi2c_rtc;
  pConfig->hrtc = hrtc;
  pConfig->hi2s = hi2s;


  //Initialise the time circuit displays
  pConfig->pDestinationTime  = dateTime_display_init(hi2c_display, DESTINATION_DISPLAY_I2C_ADDRESS);
  pConfig->pPresentTime      = dateTime_display_init(hi2c_display, PRESENT_DISPLAY_I2C_ADDRESS);
  pConfig->pLastDepartedTime = dateTime_display_init(hi2c_display, DEPARTED_DISPLAY_I2C_ADDRESS);


  //Initialise the time circuit keypad
  pConfig->pTimeCircuitKeypad = keypad3x4w_init(gKeypadPinConfig);

  //Set displays to last stored values or defaults
  timeCircuit_control_updateStartUpDateTimes(pConfig);


  // NOTE: external RTC init deliberately does NOT happen here. It's done by
  // timeCircuit_control_initRTC(), run from its own isolated FreeRTOS task
  // after the scheduler starts (see StartRtcInitTask). This function runs
  // synchronously before the scheduler even starts, so any hang here (a
  // stuck I2C bus, a bad connection) would take down the entire board with
  // no way to recover - keeping it out entirely means the RTC can fail in
  // any way whatsoever without affecting displays, keypad, sound, or CAN.

  //Update display with retrieved date times
  timeCircuit_control_updateDisplays(pConfig);

  return pConfig;
}

TimeCircuit_Control_Status_t timeCircuit_control_initRTC(TimeCircuit_Control_Config_t * const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess = false;

  #if defined(SET_EXTERNAL_RTC)
  HAL_GPIO_WritePin( EXT_RTC_RST_GPIO_Port, EXT_RTC_RST_Pin, GPIO_PIN_SET);
  // Runs in its own isolated task (see StartRtcInitTask), so even if this
  // recovery attempt itself hangs or has a bug, only this task stalls -
  // the rest of the system is unaffected either way.
  I2C2_BusRecovery();
  if(DS3231_Init(pConfig->hi2c_rtc)  == HAL_OK)
  {
    //Retrieve year data
    dateTime_getRtcDateTimeData(pConfig->pPresentTime, &pConfig->hRtcDate, &pConfig->hRtcTime);

    //Retrieve RTC date time data
    timeCircuit_control_getRtcDateTime(pConfig);

    //Set present date time to RTC date time
    dateTime_setRtcDateTimeData(pConfig->pPresentTime, &pConfig->hRtcDate, &pConfig->hRtcTime);

    //Refresh just the present-time display now that real RTC data is available
    dateTime_updateDisplay(pConfig->pPresentTime);

    isSuccess = true;
  }
  #endif

  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_control_deInit(TimeCircuit_Control_Config_t* const pConfig)
{
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

  // Keep the keypad input buffer in sync with what's now on the destination
  // display, so pressing the destination-time Enter key without typing
  // anything new re-validates this default instead of a stale/empty buffer.
  for (uint8_t characterCount = 0; characterCount < MAXIMUM_DATETIME_INPUT_CHARS; characterCount++)
  {
    pConfig->keypadInput[characterCount] = gDefaultDestinationTime[characterCount];
  }
  pConfig->keypadInputCount = MAXIMUM_DATETIME_INPUT_CHARS;

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

  bool isButtonActivated                  = false;
  bool hasButtonStateChanged              = false;

  static bool hasButtonActivated          = false;
  static uint32_t previousTime            = 0;

  TimeCircuit_Control_Status_t state = TIMECIRCUIT_CONTROL_NOT_READY;


  isButtonActivated  = keypad3x4w_readKeypad(pConfig->pTimeCircuitKeypad, &pConfig->keypadInputValue);
  hasButtonStateChanged = timeCircuit_control_checkButtonActivation(&isButtonActivated, &hasButtonActivated,
        &previousTime, KEYPAD_DEBOUNCE_TIME_MS);

  if ((hasButtonStateChanged == true) && (isButtonActivated == true))
  {
    //Reset Count
    if (pConfig->keypadInputCount >= 12)
    {
      pConfig->keypadInputCount = 0;
    }

    pConfig->keypadInput[pConfig->keypadInputCount] = pConfig->keypadInputValue;
    pConfig->keypadInputCount++;

    switch(pConfig->keypadInputValue)
    {
      case 0:
        osMessageQueuePut(soundQueueHandle, &keypadSound_Zero_filename, 0, 0);
        break;
      case 1:
        osMessageQueuePut(soundQueueHandle, &keypadSound_One_filename, 0, 0);
        break;
      case 2:
        osMessageQueuePut(soundQueueHandle, &keypadSound_Two_filename, 0, 0);
        break;
      case 3:
        osMessageQueuePut(soundQueueHandle, &keypadSound_Three_filename, 0, 0);
        break;
      case 4:
        osMessageQueuePut(soundQueueHandle, &keypadSound_Four_filename, 0, 0);
        break;
      case 5:
        osMessageQueuePut(soundQueueHandle, &keypadSound_Five_filename, 0, 0);
        break;
      case 6:
        osMessageQueuePut(soundQueueHandle, &keypadSound_Six_filename, 0, 0);
        break;
      case 7:
        osMessageQueuePut(soundQueueHandle, &keypadSound_Seven_filename, 0, 0);
        break;
      case 8:
        osMessageQueuePut(soundQueueHandle, &keypadSound_Eight_filename, 0, 0);
        break;
      case 9:
        osMessageQueuePut(soundQueueHandle, &keypadSound_Nine_filename, 0, 0);
        break;

    }
  }


  if (pConfig->keypadInputCount == 12)
  {
    state = TIMECIRCUIT_CONTROL_OK;
  }
  else
  {
    state = TIMECIRCUIT_CONTROL_NOT_READY;
  }

  return state;
}


TimeCircuit_Control_Status_t timeCircuit_control_getRtcDateTime(TimeCircuit_Control_Config_t * const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess = 1;

  //Retrieve RTC Date Time Data
  #if defined(SET_INTERNAL_RTC)
    isSuccess &= HAL_RTC_GetTime(pConfig->hrtc, &pConfig->hRtcTime, RTC_FORMAT_BIN);
    isSuccess &= HAL_RTC_GetDate(pConfig->hrtc, &pConfig->hRtcDate, RTC_FORMAT_BIN);
  #elif defined(SET_EXTERNAL_RTC)
    isSuccess &= DS3231_GetDateTime(pConfig->hi2c_rtc, &pConfig->hRtcTime, &pConfig->hRtcDate);
  #endif

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
  #if defined(SET_INTERNAL_RTC)
    isSuccess &= HAL_RTC_SetTime(pConfig->hrtc, &pConfig->hRtcTime, RTC_FORMAT_BIN);
    isSuccess &= HAL_RTC_SetDate(pConfig->hrtc, &pConfig->hRtcDate, RTC_FORMAT_BIN);
  #elif defined(SET_EXTERNAL_RTC)
    isSuccess &= DS3231_SetDateTime(pConfig->hi2c_rtc, &pConfig->hRtcTime, &pConfig->hRtcDate);
  #endif

  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_control_getRTCMinute(TimeCircuit_Control_Config_t * const pConfig,
    uint8_t * currentMinutes)
{
  TimeCircuit_Control_Status_t isSuccess = 1;

  #if defined(SET_INTERNAL_RTC)
  isSuccess &= HAL_RTC_GetTime(pConfig->hrtc, &pConfig->hRtcTime, RTC_FORMAT_BIN);
  #elif defined(SET_EXTERNAL_RTC)
    HAL_StatusTypeDef rtcStatus = DS3231_GetDateTime(pConfig->hi2c_rtc, &pConfig->hRtcTime, &pConfig->hRtcDate);
    isSuccess &= (rtcStatus == HAL_OK);

    if (rtcStatus != HAL_OK) {
      // Bus may be wedged (e.g. battery-backed RTC left holding it after a
      // power cycle). Recover and retry, but rate-limited so a genuinely
      // disconnected/dead RTC doesn't get bit-banged every single cycle.
      static uint32_t lastRecoveryTick = 0;
      if ((HAL_GetTick() - lastRecoveryTick) > 5000) {
        lastRecoveryTick = HAL_GetTick();
        I2C2_BusRecovery();
        isSuccess = (DS3231_GetDateTime(pConfig->hi2c_rtc, &pConfig->hRtcTime, &pConfig->hRtcDate) == HAL_OK);
      }
    }
  #endif

  *currentMinutes = pConfig->hRtcTime.Minutes;

  return isSuccess;
}


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
  isSuccess = storageDevice_writeFile(gStorageConfig, writeBuf, sizeof(writeBuf), gStoredDateTimeFileName);

  isSuccess &= storageDevice_closeFile(gStorageConfig);

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
  isSuccess = storageDevice_readFile(gStorageConfig, pReadBuf, sizeof(pReadBuf), gStoredDateTimeFileName);
  isSuccess &= storageDevice_closeFile(gStorageConfig);

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

      // Mirror the restored destination time into the keypad input buffer.
      // Without this, the buffer sits empty from boot until the user types
      // a full new entry, so pressing the destination-time Enter key first
      // (to just confirm the value already on the display) re-validates an
      // empty buffer and blanks the display instead.
      if (displayCount == 0)
      {
        for (uint8_t characterCount = 0; characterCount < MAXIMUM_DATETIME_INPUT_CHARS; characterCount++)
        {
          pConfig->keypadInput[characterCount] = pStartUpDateTime[characterCount];
        }
        pConfig->keypadInputCount = MAXIMUM_DATETIME_INPUT_CHARS;
      }
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

TimeCircuit_Control_Status_t timeCircuit_control_executeTimeTravelEvent(TimeCircuit_Control_Config_t* const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess  = true;

      //Clear displays
      isSuccess &= timeCircuit_control_clearDisplays(pConfig);

      //Play Sound
      osMessageQueuePut(soundQueueHandle, &lockedSound_filename, 0, 0);

      //Delay Display Update
      osDelay(DISPLAY_DELAY_MS);

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

  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_control_readTimeTravelSwitch(TimeCircuit_Control_Config_t* const pConfig)
{
  TimeCircuit_Control_Status_t  status = TIMECIRCUIT_CONTROL_NOT_READY;
  bool isButtonActivated                  = false;
  bool hasButtonStateChanged              = false;

  static bool hasButtonActivated          = false;
  static uint32_t previousTime            = 0;

  isButtonActivated  = HAL_GPIO_ReadPin(TIME_TRAVEL_SWITCH_GPIO_PORT, TIME_TRAVEL_SWITCH_PIN);
  hasButtonStateChanged = timeCircuit_control_checkButtonActivation(&isButtonActivated, &hasButtonActivated,
        &previousTime, TIME_TRAVEL_SWITCH_DEBOUNCE_TIME_MS);

  if (hasButtonStateChanged == true)
  {
    gTimeTravelSw = !isButtonActivated;

    if (isButtonActivated == true )
    {
      status = TIMECIRCUIT_CONTROL_OK;
    }
  }
  return status;
}

TimeCircuit_Control_Status_t timeCircuit_control_updateDestinationDateTime(TimeCircuit_Control_Config_t* const pConfig)
{
  TimeCircuit_Control_Status_t  status = TIMECIRCUIT_CONTROL_NOT_READY;
  bool isButtonActivated                  = false;
  bool hasButtonStateChanged              = false;

  static bool hasButtonActivated          = false;
  static uint32_t previousTime            = 0;


  isButtonActivated  = !HAL_GPIO_ReadPin(KEYPAD_ENTER_SWITCH_GPIO_PORT, KEYPAD_ENTER_SWITCH_PIN);
  hasButtonStateChanged = timeCircuit_control_checkButtonActivation(&isButtonActivated, &hasButtonActivated,
      &previousTime, ENTER_SWITCH_DEBOUNCE_TIME_MS);

  if (hasButtonStateChanged == true)
  {
    gKeypadEnterSw = isButtonActivated;

    if (isButtonActivated == true )
    {
      //Activate Keypad White Indicator
      HAL_GPIO_WritePin(KEYPAD_WHITE_INDICATOR_GPIO_PORT, KEYPAD_WHITE_INDICATOR_PIN, GPIO_PIN_SET);
      //Clear destination date time
      dateTime_clearDisplay(pConfig->pDestinationTime);
      //Reset keypad input data buffer
      pConfig->keypadInputCount = 0;

      //Update date time if a valid entry has submitted
      if (dateTime_setDisplayData(pConfig->pDestinationTime,pConfig->keypadInput))
      {
        //Play sound

        osMessageQueuePut(soundQueueHandle, &enterSound_filename, 0, 0);

        //Delay Display Update
        osDelay(DISPLAY_DELAY_MS);
        dateTime_updateDisplay(pConfig->pDestinationTime);

        //Save new date times
        timeCircuit_control_saveDateTimes(pConfig);

        status = TIMECIRCUIT_CONTROL_OK;
      }
      else
      {
        status = TIMECIRCUIT_CONTROL_INVALID_INPUT;
      }

    }
    else
    {
      //Deactivate Keypad White Indicator
      HAL_GPIO_WritePin(KEYPAD_WHITE_INDICATOR_GPIO_PORT, KEYPAD_WHITE_INDICATOR_PIN, GPIO_PIN_RESET);
    }

  }
  return status;
}

TimeCircuit_Control_Status_t timeCircuit_control_updateGlitch(TimeCircuit_Control_Config_t* const pConfig)
{
  TimeCircuit_Control_Status_t isSuccess  = true;

  /* --- local statics --- */
  static uint32_t previousTime      = 0;
  static uint32_t randomFaultTime   = 0;
  static uint8_t  stateCount        = 0;

  static bool     hasButtonActivated = false;
  static uint32_t previousTimeMS     = 0;
  static uint32_t previousFaultTime  = 0;

  /* --- read & debounce local switch --- */
  bool bIsButtonActivated  = !HAL_GPIO_ReadPin(GLITCH_SWITCH_GPIO_PORT, GLITCH_SWITCH_PIN);
  bool bHasButtonStateChanged =
      timeCircuit_control_checkButtonActivation(&bIsButtonActivated,
                                                &hasButtonActivated,
                                                &previousTimeMS,
                                                GLITCH_SWITCH_DEBOUNCE_TIME_MS);

  /* --- build effective glitch enable: local OR remote --- */
  bool remote = gGlitchRemoteEnable;
  bool effective = (bIsButtonActivated || remote);

  /* edge detect on the effective state */
  static bool prevEffective = false;
  bool effectiveChanged = bHasButtonStateChanged || (effective != prevEffective);

  if (effectiveChanged) {
    prevEffective = effective;
    gGlitchSw = effective;               // keep your global in sync

    if (effective) {
      /* (re)arm: seed new random delay and timestamp */
      if (gGlitchPeriodMs == 0) gGlitchPeriodMs = 1;  // avoid %0
      randomFaultTime  = rand() % gGlitchPeriodMs;
      previousFaultTime = HAL_GetTick();
    } else {
      /* turned OFF: restore destination display. No clear beforehand -
       * dateTime_updateDisplay() unconditionally repaints every alpha,
       * digit, and meridiem segment already, so clearing first only
       * produced a visible blank-then-redraw flicker with nothing to show
       * for it. */
      isSuccess  = dateTime_updateDisplay(pConfig->pDestinationTime);
      return isSuccess;
    }
  }

  /* --- run glitch state machine when enabled --- */
  if ( effective &&
       ((HAL_GetTick() - previousTime)      >  gGlitchTimeDelay[stateCount]) &&
       ((HAL_GetTick() - previousFaultTime) >  randomFaultTime) &&
       (gGlitchDoubleHit  == false) )
  {
    switch (stateCount) {
      case 0:
        isSuccess &= dateTime_clearDisplayExceptColons(pConfig->pDestinationTime);
        stateCount++;
        break;

      case 1:
        isSuccess &= dateTime_updateDisplayGlitch(pConfig->pDestinationTime, gGlitchDisplayChars);
        stateCount++;
        break;

      case 2:
        isSuccess &= dateTime_updateDisplayGlitch(pConfig->pDestinationTime, gGlitchDisplayDate);
        stateCount = 0;

        osMessageQueuePut(soundQueueHandle, &gitchSound_filename, 0, 0);
        break;
    }
    previousTime = HAL_GetTick();
  }

  /* --- double-hit clears then re-arms --- */
  if (effective && gGlitchDoubleHit) {
    isSuccess &= dateTime_clearDisplayExceptColons(pConfig->pDestinationTime);
    osDelay(500);
    isSuccess &= dateTime_updateDisplayGlitch(pConfig->pDestinationTime, gGlitchDisplayChars);
    osDelay(100);
    osMessageQueuePut(soundQueueHandle, &lockedSound_filename, 0, 0);

    isSuccess  = dateTime_updateDisplay(pConfig->pDestinationTime);

    gGlitchDoubleHit = false;
    if (gGlitchPeriodMs == 0) gGlitchPeriodMs = 1;
    randomFaultTime   = rand() % gGlitchPeriodMs;
    previousFaultTime = HAL_GetTick();
  }

  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_updateButtonStates(void)
{
  uint8_t state = 0;
  DateTime_Display_Status_t isSuccess   = 0;

  state |= (gGlitchSw        ? 1U << 0 : 0U); // Bit0 = Glitch
  state |= (gKeypadEnterSw   ? 1U << 1 : 0U); // Bit1 = Keypad Enter
  state |= (gSoundMuteSw     ? 1U << 2 : 0U); // Bit2 = Mute
  state |= (gTimeTravelSw    ? 1U << 3 : 0U); // Bit3 = Time Travel Simulation

  // Bits 4-7 are reserved and left as 0

  OD_RAM.x2100_buttonsState = state;  // Update OD variable
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

    //Play sound
    if ((toogleStatus == 3) && !gSoundRemoteMuteColon && !gSoundMuteSw) {
      gColonPending = true;
      gColonRequestTick = HAL_GetTick();
    }

  }

  return isSuccess;
}

TimeCircuit_Control_Status_t timeCircuit_setRemoteDisplayDates(TimeCircuit_Control_Config_t* const pConfig)
{
  DateTime_Display_Status_t isSuccess   = 0;

  dateTime_setRemoteDateTime ((const OD_DateTimeRec_t *)&OD_RAM.x2000_destinationTime, pConfig->pDestinationTime);
  dateTime_setRemoteDateTime ((const OD_DateTimeRec_t *)&OD_RAM.x2001_presentTime, pConfig->pPresentTime);
  dateTime_setRemoteDateTime ((const OD_DateTimeRec_t *)&OD_RAM.x2002_lastDepartedTime, pConfig->pLastDepartedTime);

  return isSuccess;
}

static void timeCircuit_processFunctionControl(TimeCircuit_Control_Config_t* const pConfig,
    TimeTravelContext_t *ctx) {
    uint8_t fc = OD_RAM.x2200_functionControl;

    if (fc & FUNC_CTRL_CLEAR_ALL_DISPLAYS) {
      timeCircuit_control_clearDisplays(pConfig);
        OD_RAM.x2200_functionControl &= ~FUNC_CTRL_CLEAR_ALL_DISPLAYS; // clear bit after action
    }

    if (fc & FUNC_CTRL_UPDATE_ALL_DISPLAYS) {
      timeCircuit_control_clearDisplays(pConfig);

       if ((timeCircuit_control_isRemoteDateValid(pConfig->pDestinationTime) == TIMECIRCUIT_CONTROL_OK) &&
           (timeCircuit_control_isRemoteDateValid(pConfig->pPresentTime) == TIMECIRCUIT_CONTROL_OK) &&
           (timeCircuit_control_isRemoteDateValid(pConfig->pLastDepartedTime) == TIMECIRCUIT_CONTROL_OK))
       {
         osDelay(DISPLAY_DELAY_MS);
          timeCircuit_control_updateDisplays(pConfig);
          timeCircuit_control_setRtcDateTime(pConfig);
          timeCircuit_control_saveDateTimes(pConfig);
         ctx->inputDateValid = true;
       }
       else
       {
         ctx->inputDateValid = false;
       }

        OD_RAM.x2200_functionControl &= ~FUNC_CTRL_UPDATE_ALL_DISPLAYS;
    }

    if (fc & FUNC_CTRL_UPDATE_DESTINATION_DATE) {
      dateTime_clearDisplay(pConfig->pDestinationTime);
      dateTime_setRemoteDateTime ((const OD_DateTimeRec_t *)&OD_RAM.x2000_destinationTime, pConfig->pDestinationTime);

      if (timeCircuit_control_isRemoteDateValid(pConfig->pDestinationTime) == TIMECIRCUIT_CONTROL_OK)
      {
        osDelay(DISPLAY_DELAY_MS);
        dateTime_updateDisplay(pConfig->pDestinationTime);
        ctx->inputDateValid = true;
      }
      else
      {
        ctx->inputDateValid = false;
      }

        OD_RAM.x2200_functionControl &= ~FUNC_CTRL_UPDATE_DESTINATION_DATE;
    }

    if (fc & FUNC_CTRL_UPDATE_RTC) {
      timeCircuit_control_setRtcDateTime(pConfig);
        OD_RAM.x2200_functionControl &= ~FUNC_CTRL_UPDATE_RTC;
    }

    if (fc & FUNC_CTRL_SAVE_DATES) {
      timeCircuit_control_saveDateTimes(pConfig);
        OD_RAM.x2200_functionControl &= ~FUNC_CTRL_SAVE_DATES;
    }

    if (fc & FUNC_CTRL_SET_ALL_DISPLAYS) {
      timeCircuit_setRemoteDisplayDates(pConfig);
        OD_RAM.x2200_functionControl &= ~FUNC_CTRL_SET_ALL_DISPLAYS;
    }

}

TimeCircuit_Control_Status_t timeCircuit_control_isRemoteDateValid(DateTime_Display_Config_t* pDateTimeDisplayData)
{
  TimeCircuit_Control_Status_t state = TIMECIRCUIT_CONTROL_INVALID_INPUT;

  char    writeBuf[MAXIMUM_DATETIME_INPUT_CHARS + 1];
  uint8_t datetimeBuffer[MAXIMUM_DATETIME_INPUT_CHARS];
  uint8_t bufferCount = 0;

  dateTime_convertDateTimeToChar(pDateTimeDisplayData, writeBuf, sizeof(writeBuf), &bufferCount);

  for (size_t i = 0; i < MAXIMUM_DATETIME_INPUT_CHARS; i++) {
    datetimeBuffer[i] = (uint8_t)(writeBuf[i] - '0');
  }

  if (dateTime_setDisplayData(pDateTimeDisplayData,datetimeBuffer))
  {
    state = TIMECIRCUIT_CONTROL_OK;
  }

  return state;

}


static TimeCircuit_Control_State_t sanitizeRemoteRequest(
        TimeCircuit_Control_State_t req,
        const TimeTravelContext_t *ctx)
{
    switch (req) {
    case TIME_CIRCUITS_IDLE:
        // Always allow an abort-to-idle
        return TIME_CIRCUITS_IDLE;

    case TIME_CIRCUITS_ARMED:
        // Only arm if the input date is currently valid
        return ctx->inputDateValid ? TIME_CIRCUITS_ARMED : ctx->currentState;

    case TIME_CIRCUITS_TRAVEL:
        // Only allow travel if we're armed AND the date is still valid
        return (ctx->currentState == TIME_CIRCUITS_ARMED && ctx->inputDateValid)
               ? TIME_CIRCUITS_TRAVEL : ctx->currentState;

    case TIME_CIRCUITS_COMPLETE:
        // Only accept complete if we're in travel
        return (ctx->currentState == TIME_CIRCUITS_TRAVEL)
               ? TIME_CIRCUITS_COMPLETE : ctx->currentState;

    default:
        return ctx->currentState;
    }
}

TimeCircuit_Control_Status_t timeCircuit_control_ProcessSettingBits(void)
{
    static uint8_t prevStateBits = 0xFF;  // force first-run update

    /* Read current bits (uint8_t read is atomic on Cortex-M) */
    uint8_t bits = OD_RAM.x2300_settingParameters.settingBits;

    /* ---- Stateful bits: only act on changes ---- */
    if (prevStateBits == 0xFF || ((prevStateBits ^ bits) & SETBIT_GLITCH_ENABLE)) {
      gGlitchRemoteEnable = (bits & SETBIT_GLITCH_ENABLE) != 0;
    }
    if (prevStateBits == 0xFF || ((prevStateBits ^ bits) & SETBIT_MUTE_COLON_SOUND)) {
      gSoundRemoteMuteColon = (bits & SETBIT_MUTE_COLON_SOUND) != 0;
    }
    if (prevStateBits == 0xFF || ((prevStateBits ^ bits) & SETBIT_MUTE_ALL)) {
      gSoundRemoteMuteAll = (bits & SETBIT_MUTE_ALL) != 0;
    }

    /* ---- One-shot command bits ---- */
    if (bits & SETBIT_UPDATE_IMU_SETTINGS) {
        /* Pull parameters from the record (0x2300:02, :03) */
        uint8_t thr = OD_RAM.x2300_settingParameters.IMU_MotionDetectionThreshold;
        uint8_t dur = OD_RAM.x2300_settingParameters.IMU_MotionDetectionDuration;

        imu_bno055_updateAnyMotionSettings(thr, dur);

        /* Clear the command bit so it doesn’t retrigger next cycle */
        bits &= (uint8_t)~SETBIT_UPDATE_IMU_SETTINGS;
    }

    if (bits & SETBIT_UPDATE_GLITCH_SETTINGS) {
        /* Pull parameter from 0x2300:01 */
        uint32_t period = OD_RAM.x2300_settingParameters.glitchPeriod;  // ms
        gGlitchPeriodMs = period;

        /* Clear the command bit */
        bits &= (uint8_t)~SETBIT_UPDATE_GLITCH_SETTINGS;
    }

    /* If we cleared any command bits, write the new value back to OD */
    if (bits != OD_RAM.x2300_settingParameters.settingBits) {
        OD_RAM.x2300_settingParameters.settingBits = bits;
    }

    /* Remember for next edge-detect */
    prevStateBits = bits;

    return 1;
}


TimeCircuit_Control_Status_t timeCircuit_control_update(TimeCircuit_Control_Config_t * const pConfig)
{
  static TimeTravelContext_t ctx =
  {
      .currentState = TIME_CIRCUITS_IDLE,
      .previousRemoteRequestState = TIME_CIRCUITS_IDLE,
      .stateEntryTick = 0,
      .inputDateValid = false,
      .receivedRemoteStateCommand = false
  };

  // --- Always-on updates ---
  timeCircuit_control_updatePresentDateTime(pConfig);  // RTC must always run
  timeCircuit_control_updateGlitch(pConfig);           // Optional always-on glitch
  timeCircuit_control_readTimeTravelSwitch(pConfig);   // Read Time Travel Switch

  // --- Keypad and Input ---
  if (ctx.currentState != TIME_CIRCUITS_TRAVEL)
  {
    timeCircuit_control_readInputDateTime(pConfig);

    switch (timeCircuit_control_updateDestinationDateTime(pConfig))
    {
      case TIMECIRCUIT_CONTROL_OK:
        ctx.inputDateValid = true;
        break;
      case TIMECIRCUIT_CONTROL_INVALID_INPUT:
        ctx.inputDateValid = false;
        break;
      default:
          break;
    }
  }

  // --- Check if remote command has changed the state ---
  TimeCircuit_Control_State_t req;

  req = (TimeCircuit_Control_State_t)OD_RAM.x2102_requestedTimeCircuitsState;


  if (req != ctx.previousRemoteRequestState) {
      ctx.previousRemoteRequestState = req;

      TimeCircuit_Control_State_t allowed = sanitizeRemoteRequest(req, &ctx);
      if (allowed != ctx.currentState) {
          ctx.currentState = allowed;
          ctx.stateEntryTick = HAL_GetTick();
      }

      /* Optional: make the command register one-shot by clearing it once accepted */
      if (allowed == req) {

          OD_RAM.x2102_requestedTimeCircuitsState = TIME_CIRCUITS_IDLE;

      }
  }

  // --- State machine handling ---
  switch (ctx.currentState)
  {
      case TIME_CIRCUITS_IDLE:
          if (ctx.inputDateValid)
          {
              ctx.currentState = TIME_CIRCUITS_ARMED;
              ctx.stateEntryTick = HAL_GetTick();
          }
          break;

      case TIME_CIRCUITS_ARMED:
          if (!ctx.inputDateValid)
          {
            ctx.currentState = TIME_CIRCUITS_IDLE; // Disarmed due to invalid new entry
              break;
          }

          if ( gTimeTravelSw == true)
          {
            ctx.currentState = TIME_CIRCUITS_TRAVEL;
              ctx.stateEntryTick = HAL_GetTick();
          }
          break;

      case TIME_CIRCUITS_TRAVEL:
          timeCircuit_control_executeTimeTravelEvent(pConfig); // Travel!

          ctx.currentState = TIME_CIRCUITS_COMPLETE;
          ctx.stateEntryTick = HAL_GetTick();
          break;

      case TIME_CIRCUITS_COMPLETE:
          if ((HAL_GetTick() - ctx.stateEntryTick) > 3000) { // 3 sec timeout
            ctx.currentState = TIME_CIRCUITS_IDLE;
          }
          break;

      default:
          ctx.currentState = TIME_CIRCUITS_IDLE;
          break;
  }

  // --- Update CANOpen Object Dictionary Parameters ---

  OD_RAM.x2101_timeCircuitsState = ctx.currentState;
  timeCircuit_updateButtonStates();

  // --- Execute CANOpen Object Dictionary Commands ---
  timeCircuit_processFunctionControl(pConfig, &ctx);

  //Update Sound Effect Control Settings
   soundEffects_disableAmplifier();
   soundEffects_readMuteSwitch();


  return TIMECIRCUIT_CONTROL_OK;
}

