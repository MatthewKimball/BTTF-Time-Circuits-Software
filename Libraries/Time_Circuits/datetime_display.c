/*
 * dateTime_display.c
 *
 *  Created on: Oct 27, 2024
 *      Author: Professor Gizmo
 */
#include <datetime_display.h>

struct DateTime_Display_Config_Tag
{
  I2C_HandleTypeDef*        hi2c;
  uint8_t                   i2cAddrs;
  Ht16k33_Config_t*         hDisplayDriver;
  DateTime_DisplayData_t    dateTimeData;

  uint16_t                  orignalYear;

} DateTime_Display_Config;


DateTime_Display_Config_t * dateTime_display_init(I2C_HandleTypeDef* const hi2c, const uint8_t addrs)
{
  DateTime_Display_Config_t* pConfig = malloc(sizeof(DateTime_Display_Config_t));
  pConfig->i2cAddrs = addrs;
  pConfig->hi2c = hi2c;
  pConfig->hDisplayDriver = ht16k33_init(hi2c, addrs);

  ht16k33_setSystemSetup(pConfig->hDisplayDriver, Ht16k33_SystemOscillator_On);
  ht16k33_setDisplaySetup(pConfig->hDisplayDriver, Ht16k33_DisplayStatus_On, Ht16k33_BlinkingFrequency_Off);

  return pConfig;
}

DateTime_Display_Status_t dateTime_setDateTimeMonth(DateTime_Display_Config_t* const pConfig,
    const uint8_t* const inputDateTime)
{
  //Extract and check input month
  pConfig->dateTimeData.Month = (inputDateTime[0]*10) + (inputDateTime[1]);
  if ((pConfig->dateTimeData.Month >= 1) && (pConfig->dateTimeData.Month <= 12))
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

DateTime_Display_Status_t dateTime_setDateTimeDay(DateTime_Display_Config_t* const pConfig,
    const uint8_t* const inputDateTime)
{
  bool leapYear = false;

  //Extract input day and check for days per month
  pConfig->dateTimeData.Day = (inputDateTime[2]*10) + (inputDateTime[3]);

  //Check for leap year
  if ((((pConfig->dateTimeData.Year % 4) == 0) && ((pConfig->dateTimeData.Year % 100) != 0)) ||
      ((pConfig->dateTimeData.Year % 400) == 0))
  {
    leapYear = true;
  }

  if (pConfig->dateTimeData.Day <= (monthDaysCount[pConfig->dateTimeData.Month-1] + leapYear) &&
      (pConfig->dateTimeData.Day > 0))
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

DateTime_Display_Status_t dateTime_setDateTimeYear(DateTime_Display_Config_t* const pConfig,
    const uint8_t* const inputDateTime)
{
  //Extract input year
  pConfig->dateTimeData.Year = (inputDateTime[4]*1000)+(inputDateTime[5]*100)
      + (inputDateTime[6]*10) + (inputDateTime[7]);

  return 1;
}

DateTime_Display_Status_t dateTime_setDateTimeHour(DateTime_Display_Config_t* const pConfig,
    const uint8_t* const inputDateTime)
{
  DateTime_Display_Status_t isSuccess = 1;

  //Extract and check input hour
  pConfig->dateTimeData.Hour = (inputDateTime[8]*10) + (inputDateTime[9]);

  if ((pConfig->dateTimeData.Hour >= 0) && (pConfig->dateTimeData.Hour <= 24))
  {
    //Convert to 12 hour time and set meridiem
    if ((pConfig->dateTimeData.Hour > 12))
    {
      pConfig->dateTimeData.Hour -= 12;
      pConfig->dateTimeData.Meridiem = (pConfig->dateTimeData.Hour == 12) ?
          DateTime_DisplayDataMeridiem_AM : DateTime_DisplayDataMeridiem_PM;
    }
    else
    {
      pConfig->dateTimeData.Meridiem = (pConfig->dateTimeData.Hour == 12) ?
          DateTime_DisplayDataMeridiem_PM : DateTime_DisplayDataMeridiem_AM;
      pConfig->dateTimeData.Hour = (pConfig->dateTimeData.Hour == 0) ? 12 : pConfig->dateTimeData.Hour;
    }
  }
  else
  {
    isSuccess = 0;
  }

  return isSuccess;
}

DateTime_Display_Status_t dateTime_setDateTimeMinute(DateTime_Display_Config_t* const pConfig,
    const uint8_t* const inputDateTime)
{
  //Extract and check input month
  pConfig->dateTimeData.Minute = (inputDateTime[10]*10) + (inputDateTime[11]);
  if ((pConfig->dateTimeData.Minute >= 0) && (pConfig->dateTimeData.Minute < 60))
  {
    return 1;
  }
  else
  {
    return 0;

  }
}

DateTime_Display_Status_t dateTime_setDisplayData(DateTime_Display_Config_t* const pConfig,
    const uint8_t* const inputDateTime)
{
  DateTime_Display_Status_t isSuccess = 1;

  isSuccess &= dateTime_setDateTimeMonth(pConfig, inputDateTime);
  isSuccess &= dateTime_setDateTimeYear(pConfig, inputDateTime);
  isSuccess &= dateTime_setDateTimeDay(pConfig, inputDateTime);
  isSuccess &= dateTime_setDateTimeHour(pConfig, inputDateTime);
  isSuccess &= dateTime_setDateTimeMinute(pConfig, inputDateTime);

  return isSuccess;
}

DateTime_Display_Status_t   dateTime_getRtcDateTimeData(DateTime_Display_Config_t* const pConfig,
    RTC_DateTypeDef* const pRtcDate, RTC_TimeTypeDef* const pRtcTime)
{
  DateTime_Display_Status_t isSuccess = 1;

  pRtcDate->Date        = pConfig->dateTimeData.Day;
  pRtcDate->Month       = pConfig->dateTimeData.Month;
  pRtcDate->Year        = 0;                            //Set to zero because maximum value from RTC is 99 years
  pRtcTime->Hours       = pConfig->dateTimeData.Hour;
  pRtcTime->Minutes     = pConfig->dateTimeData.Minute;
  pRtcTime->TimeFormat  = pConfig->dateTimeData.Meridiem - 1; //RTC AM = 0, RTC PM = 1;
  pRtcTime->Seconds     = 0;
  pRtcTime->SubSeconds  = 0;
  pConfig->orignalYear  = pConfig->dateTimeData.Year; //Hacky way to fix RTC year issue
  return isSuccess;
}

DateTime_Display_Status_t   dateTime_setRtcDateTimeData(DateTime_Display_Config_t* const pConfig,
    RTC_DateTypeDef* const pRtcDate, RTC_TimeTypeDef* const pRtcTime)
{
  DateTime_Display_Status_t isSuccess = 1;

  pConfig->dateTimeData.Day       = pRtcDate->Date;
  pConfig->dateTimeData.Month     = pRtcDate->Month;
  pConfig->dateTimeData.Year      = pConfig->orignalYear + pRtcDate->Year;  //Add years because maximum value from RTC is 99 years
  pConfig->dateTimeData.Hour      = pRtcTime->Hours;
  pConfig->dateTimeData.Minute    = pRtcTime->Minutes;
  pConfig->dateTimeData.Meridiem  = pRtcTime->TimeFormat + 1; //RTC AM = 0, RTC PM = 1;

  return isSuccess;
}

DateTime_Display_Status_t dateTime_copyDateTime(DateTime_Display_Config_t* const pDestConfig,
    DateTime_Display_Config_t* const pSourceConfig)
{
  DateTime_Display_Status_t isSuccess = 1;

  memcpy(&pDestConfig->dateTimeData, &pSourceConfig->dateTimeData, sizeof(pDestConfig->dateTimeData));

  return isSuccess;
}

DateTime_Display_Status_t dateTime_clearDisplay(DateTime_Display_Config_t* const pConfig)
{
  uint8_t* clearBuffer = malloc (TOTAL_NUMBER_OF_ROWS);
  DateTime_Display_Status_t isSuccess   = 0;

  for (int buffCount = 0; buffCount < TOTAL_NUMBER_OF_ROWS; buffCount++)
  {
    clearBuffer[buffCount] = 0x00;
  }

  isSuccess = ht16k33_updateDisplayData (pConfig->hDisplayDriver,DISPLAY_DATA_REG_ADDRESS, clearBuffer,
      TOTAL_NUMBER_OF_ROWS);

  if (isSuccess)
  {
    isSuccess |= ht16k33_setDisplaySetup(pConfig->hDisplayDriver, Ht16k33_DisplayStatus_On,
        Ht16k33_BlinkingFrequency_Off);
  }

  free(clearBuffer);

  return isSuccess;
}
DateTime_Display_Status_t dateTime_setLed(DateTime_Display_Config_t* const pConfig, const uint8_t segmentNumber,
       const uint8_t ledState)
{
     DateTime_Display_Status_t isSuccess   = 0;
     uint8_t SegmentData = 0;
     uint8_t RequestData = segmentNumber;

     isSuccess |= HAL_I2C_Master_Transmit(pConfig->hi2c, (pConfig->i2cAddrs)<<1, &RequestData,  1, HAL_MAX_DELAY);
     isSuccess |= HAL_I2C_Master_Receive(pConfig->hi2c, ((pConfig->i2cAddrs)<<1)|0x01, &SegmentData, 1, HAL_MAX_DELAY);
     SegmentData &= 0b00111111;
     SegmentData |= ledState;
     isSuccess |= ht16k33_updateDisplayData (pConfig->hDisplayDriver, RequestData, &SegmentData, 1);

     return isSuccess;
}

DateTime_Display_Status_t dateTime_toggleTimeColon(DateTime_Display_Config_t* const pConfig,
    DateTime_Display_Config_t* const pConfig1, DateTime_Display_Config_t* const pConfig2)
{
  static uint8_t toogleStatus = 0;
  static uint32_t previousTime = 0;
  DateTime_Display_Status_t isSuccess   = 0;

  if ((HAL_GetTick()-previousTime) >= COLON_TIME_DELAY_MS)
  {
    toogleStatus = ((toogleStatus) == 0) ? 3 : 0;
    previousTime = HAL_GetTick();
    isSuccess |= dateTime_setLed(pConfig, COLON_LED_SEGMENT_ADDRESS, (toogleStatus<<6));
    isSuccess |= dateTime_setLed(pConfig1, COLON_LED_SEGMENT_ADDRESS, (toogleStatus<<6));
    isSuccess |= dateTime_setLed(pConfig2, COLON_LED_SEGMENT_ADDRESS, (toogleStatus<<6));
  }

  return isSuccess;
}

//Hacky way of fixing wiring issue of LED numerical digits
DateTime_Display_Status_t dateTime_setDigitSegments(DateTime_Display_Config_t* const pConfig, const uint8_t segmentNum,
    const char dispDigit)
{
  uint8_t* displayBuffer = malloc (2);
  uint8_t test[] = {0,0};
  uint8_t boo[] = {0x00};

  if (segmentNum % 2)
  {
    boo[0] = segmentNum - 0x01;
    HAL_I2C_Master_Transmit(pConfig->hi2c, (pConfig->i2cAddrs)<<1, boo,  sizeof(boo), HAL_MAX_DELAY);
    HAL_I2C_Master_Receive(pConfig->hi2c, ((pConfig->i2cAddrs)<<1)|0x01, test, 1, HAL_MAX_DELAY);
    test[0] = 0x7F & test[0];
    test[0] |= (sevenSegmentChars[(dispDigit-'0')] & 0x01 )<< 7;
    test[1] |= sevenSegmentChars[(dispDigit-'0')] >> 1;
    ht16k33_updateDisplayData (pConfig->hDisplayDriver, boo[0], test, 2);
  }
  else
  {
    boo[0] = segmentNum;
    HAL_I2C_Master_Transmit(pConfig->hi2c, (pConfig->i2cAddrs)<<1, boo,  1, HAL_MAX_DELAY);
    HAL_I2C_Master_Receive(pConfig->hi2c, ((pConfig->i2cAddrs)<<1)|0x01, test, 1, HAL_MAX_DELAY);
    test[0] = 0x80 & test[0];
    displayBuffer[0] = test[0] | sevenSegmentChars[(dispDigit-'0')];
    ht16k33_updateDisplayData (pConfig->hDisplayDriver, segmentNum, displayBuffer, 1);
  }

  free(displayBuffer);
  return 1;
}

DateTime_Display_Status_t dateTime_setAlphaSegments(DateTime_Display_Config_t* const pConfig, const uint8_t segmentNum,
    const char dispAlpha)
{
  uint8_t displayBuffer[2] = {0,0};

  displayBuffer[0] = (uint8_t)((fourteenSegmentChars[(uint8_t)(dispAlpha-0x41)]>>0) & 0xFF);
  displayBuffer[1] = (uint8_t)((fourteenSegmentChars[(uint8_t)(dispAlpha-0x41)]>>8) & 0xFF);
  ht16k33_updateDisplayData (pConfig->hDisplayDriver, (10 + (2*segmentNum)), displayBuffer, 2);

  return 1;
}

DateTime_Display_Status_t dateTime_updateDisplayGlitch(DateTime_Display_Config_t* const pConfig,
    const char* const pGlitchData)
{
  DateTime_Display_Status_t isSuccess           = 0;
  uint8_t                   segmentCount        = 0;
  char                      digitSegBuffer[11];

  memcpy(&digitSegBuffer, pGlitchData, sizeof(digitSegBuffer));

  //Update Glitch Digits
  for (segmentCount = 0; segmentCount < (sizeof(digitSegBuffer)-1); segmentCount++)
  {
    isSuccess |= dateTime_setDigitSegments(pConfig, digitSegmentOrder[segmentCount], digitSegBuffer[segmentCount]);
  }

  //Update Meridiem
  isSuccess |= dateTime_setLed(pConfig, MERIDIEM_LED_SEGMENT_ADDRESS, (DateTime_DisplayDataMeridiem_AM << 6));

  return isSuccess;
}

DateTime_Display_Status_t dateTime_updateDisplay(DateTime_Display_Config_t* const pConfig)
{
  DateTime_Display_Status_t isSuccess           = 0;
  uint8_t                   digitSegBufferCount = 0;
  uint8_t                   segmentCount        = 0;
  char                      digitSegBuffer[11];

  //Update Month Display Characters
  for (segmentCount = 0; segmentCount <= 2; segmentCount++)
  {
    isSuccess |= dateTime_setAlphaSegments(pConfig, segmentCount,
        monthDisplayChars[(pConfig->dateTimeData.Month)-1][2-segmentCount]);
  }

  //Update Day, Year and Time
  digitSegBufferCount = snprintf(digitSegBuffer, sizeof(digitSegBuffer), "%02d", pConfig->dateTimeData.Day);
  digitSegBufferCount += snprintf(digitSegBuffer+digitSegBufferCount, sizeof(digitSegBuffer)-digitSegBufferCount,
      "%04d", pConfig->dateTimeData.Year);
  digitSegBufferCount += snprintf(digitSegBuffer+digitSegBufferCount, sizeof(digitSegBuffer)-digitSegBufferCount,
      "%02d", pConfig->dateTimeData.Hour);
  digitSegBufferCount += snprintf(digitSegBuffer+digitSegBufferCount, sizeof(digitSegBuffer)-digitSegBufferCount,
      "%02d", pConfig->dateTimeData.Minute);

  for (segmentCount = 0; segmentCount < (sizeof(digitSegBuffer)-1); segmentCount++)
  {
    isSuccess |= dateTime_setDigitSegments(pConfig, digitSegmentOrder[segmentCount], digitSegBuffer[segmentCount]);
  }

  //Update Meridiem
  isSuccess |= dateTime_setLed(pConfig, MERIDIEM_LED_SEGMENT_ADDRESS, (pConfig->dateTimeData.Meridiem << 6));

  return isSuccess;
}
