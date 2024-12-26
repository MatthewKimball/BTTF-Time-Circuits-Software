/*
 * dateTime_display.c
 *
 *  Created on: Oct 27, 2024
 *      Author: Professor Gizmo
 */
#include <datetime_display.h>

//7 Segment Wiring Order
static const uint8_t digitSegmentOrder[] = {
    8,  /* Day Digit 1    */
    9,  /* Day Digit 2    */
    6,  /* Year Digit 1   */
    7,  /* Year Digit 2   */
    4,  /* Year Digit 3   */
    5,  /* Year Digit 4   */
    2,  /* Hour Digit 1   */
    3,  /* Hour Digit 2   */
    0,  /* Minute Digit 1 */
    1,  /* Minute Digit 2 */
};

//14 Segment Alphanumerical Display
static const uint16_t fourteenSegmentChars[26] = {
  0b0000000011110111,  /* A */
  0b0001000110001111,  /* B */
  0b0000000000111001,  /* C */
  0b0001000100001111,  /* D */
  0b0000000011111001,  /* E */
  0b0000000011110001,  /* F */
  0b0000000010111101,  /* G */
  0b0000000011110110,  /* H */
  0b0001001000001001,  /* I */
  0b0000000000011110,  /* J */
  0b0010010001110000,  /* K */
  0b0000000000111000,  /* L */
  0b0000011000110110,  /* M */
  0b0000101000110110,  /* N */
  0b0000000000111111,  /* O */
  0b0000000011110011,  /* P */
  0b0010000000111111,  /* Q */
  0b0000100011110011,  /* R */
  0b0000000011101101,  /* S */
  0b0001000100000001,  /* T */
  0b0000000000111110,  /* U */
  0b0010010000110000,  /* V */
  0b0010100000110110,  /* W */
  0b0010110100000000,  /* X */
  0b0001000011100010,  /* Y */
  0b0000110000001001,  /* Z */
};

//7 Segment Numerical Display
static const uint8_t sevenSegmentChars[10] = {
  0x3F, /* 0 */
  0x06, /* 1 */
  0x5B, /* 2 */
  0x4F, /* 3 */
  0x66, /* 4 */
  0x6D, /* 5 */
  0x7D, /* 6 */
  0x07, /* 7 */
  0x7F, /* 8 */
  0x67, /* 9 */
};

//Month Display Characters
static const char monthDisplayChars[12][3] = {
  {'J','A','N'},    /* January */
  {'F','E','B'},    /* February */
  {'M','A','R'},    /* March */
  {'A','P','R'},    /* April */
  {'M','A','Y'},    /* May */
  {'J','U','N'},    /* June */
  {'J','U','L'},    /* July */
  {'A','U','G'},    /* August */
  {'S','E','P'},    /* September */
  {'O','C','T'},    /* October */
  {'N','O','V'},    /* November */
  {'D','E','C'},    /* December */
};

static const uint8_t monthDaysCount [12] = {
    31,    /* January */
    28,    /* February */
    31,    /* March */
    30,    /* April */
    31,    /* May */
    30,    /* June */
    31,    /* July */
    31,    /* August */
    30,    /* September */
    31,    /* October */
    30,    /* November */
    31,    /* December */
};

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

  memcpy(&digitSegBuffer, pGlitchData+3, sizeof(digitSegBuffer));


  //Update glitch alphanumeric display
  for (segmentCount = 0; segmentCount <= 2; segmentCount++)
  {
    if (pGlitchData[segmentCount] !=' ')
    {
      isSuccess |= dateTime_setAlphaSegments(pConfig, segmentCount, pGlitchData[2-segmentCount]);
    }
  }

  //Update glitch digit display
  for (segmentCount = 0; segmentCount < (sizeof(digitSegBuffer)-1); segmentCount++)
  {
    isSuccess |= dateTime_setDigitSegments(pConfig, digitSegmentOrder[segmentCount], digitSegBuffer[segmentCount]);
  }

  //Update Meridiem
  isSuccess |= dateTime_setLed(pConfig, MERIDIEM_LED_SEGMENT_ADDRESS, (DateTime_DisplayDataMeridiem_AM << 6));

  return isSuccess;
}

DateTime_Display_Status_t dateTime_convertDateTimeToChar(DateTime_Display_Config_t* const pConfig,
    char* const writeBuf, uint8_t bufferSize, uint8_t* pBufferCount)
{
  DateTime_Display_Status_t isSuccess = 0;
  uint8_t convertedHour;

  *pBufferCount +=  snprintf(writeBuf+*pBufferCount, bufferSize-*pBufferCount,
      "%02d", pConfig->dateTimeData.Month);
  *pBufferCount += snprintf(writeBuf+*pBufferCount, bufferSize-*pBufferCount,
      "%02d", pConfig->dateTimeData.Day);
  *pBufferCount += snprintf(writeBuf+*pBufferCount, bufferSize-*pBufferCount,
      "%04d", pConfig->dateTimeData.Year);
  switch (pConfig->dateTimeData.Meridiem)
  {
    case  DateTime_DisplayDataMeridiem_PM:
      convertedHour = (pConfig->dateTimeData.Hour == 12) ? (pConfig->dateTimeData.Hour) : (pConfig->dateTimeData.Hour + 12);
      break;
    case  DateTime_DisplayDataMeridiem_AM:
      convertedHour = (pConfig->dateTimeData.Hour == 12) ? (pConfig->dateTimeData.Hour + 12) : (pConfig->dateTimeData.Hour);
      break;
  }
  *pBufferCount += snprintf(writeBuf+*pBufferCount, bufferSize-*pBufferCount,
      "%02d", convertedHour);
  *pBufferCount += snprintf(writeBuf+*pBufferCount, bufferSize-*pBufferCount,
      "%02d", pConfig->dateTimeData.Minute);

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

