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

#define COLON_TIME_DELAY_MS 500

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

#endif /* TIME_CIRCUITS_DATETIME_DISPLAY_H_ */
