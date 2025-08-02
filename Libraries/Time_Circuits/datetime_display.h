/*
 * datetime_display.h
 *
 *  Created on: Oct 27, 2024
 *      Author: Professor Gizmo
 */

#ifndef TIME_CIRCUITS_DATETIME_DISPLAY_H_
#define TIME_CIRCUITS_DATETIME_DISPLAY_H_

#include "stm32f4xx_hal.h"
#include "ht16k33.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define TOTAL_NUMBER_OF_ROWS          16
#define TOTAL_NUMBER_OF_COLUMNS       8

#define MERIDIEM_LED_SEGMENT_ADDRESS  0x09
#define COLON_LED_SEGMENT_ADDRESS     0x07

#define COLON_TIME_DELAY_MS           500

typedef enum
{
  DateTime_DisplayDataMeridiem_AM  = 1,
  DateTime_DisplayDataMeridiem_PM  = 2,
}DateTime_DisplayDataMeridiem_e;

typedef struct
{
  uint8_t                         Month;
  uint8_t                         Day;
  uint16_t                        Year;
  uint8_t                         Hour;
  uint8_t                         Minute;
  DateTime_DisplayDataMeridiem_e  Meridiem;
}DateTime_DisplayData_t;

typedef struct DateTime_Display_Config_Tag DateTime_Display_Config_t;
typedef bool DateTime_Display_Status_t;

DateTime_Display_Config_t*  dateTime_display_init(I2C_HandleTypeDef* const hi2c, const uint8_t addrs);
DateTime_Display_Status_t   dateTime_clearDisplay(DateTime_Display_Config_t* const pConfig);
DateTime_Display_Status_t   dateTime_updateDisplay(DateTime_Display_Config_t* const pConfig);
DateTime_Display_Status_t   dateTime_setDisplayData(DateTime_Display_Config_t* const pConfig,
    const uint8_t* const inputDateTime);
DateTime_Display_Status_t   dateTime_toggleTimeColon(DateTime_Display_Config_t* const pConfig,
    DateTime_Display_Config_t* const pConfig1, DateTime_Display_Config_t* const pConfig2);
DateTime_Display_Status_t   dateTime_copyDateTime(DateTime_Display_Config_t* const pDestConfig,
    DateTime_Display_Config_t* const pSourceConfig);
DateTime_Display_Status_t   dateTime_setRtcDateTimeData(DateTime_Display_Config_t* const pConfig,
    RTC_DateTypeDef* const pRtcDate, RTC_TimeTypeDef* const pRtcTime);
DateTime_Display_Status_t   dateTime_getRtcDateTimeData(DateTime_Display_Config_t* const pConfig,
    RTC_DateTypeDef* const pRtcDate, RTC_TimeTypeDef* const pRtcTime);
DateTime_Display_Status_t dateTime_updateDisplayGlitch(DateTime_Display_Config_t* const pConfig,
    const char* const pGlitchData);
DateTime_Display_Status_t dateTime_convertDateTimeToChar(DateTime_Display_Config_t* const pConfig,
    char* const writeBuf, uint8_t bufferSize, uint8_t* pBufferCount);
DateTime_Display_Status_t dateTime_setLed(DateTime_Display_Config_t* const pConfig, const uint8_t segmentNumber,
       const uint8_t ledState);
DateTime_Display_Status_t dateTime_clearDisplayExceptColons(DateTime_Display_Config_t* const pConfig);



#endif /* TIME_CIRCUITS_DATETIME_DISPLAY_H_ */
