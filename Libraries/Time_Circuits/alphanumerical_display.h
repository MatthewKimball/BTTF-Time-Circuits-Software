/*
 * alphanumerical_display.h
 *
 *  Created on: 26 Feb 2023
 *      Author: User
 */

#ifndef TIME_CIRCUITS_ALPHANUMERICAL_DISPLAY_H_
#define TIME_CIRCUITS_ALPHANUMERICAL_DISPLAY_H_

#include "stm32f4xx_hal.h"
#include "ht16k33.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define TOTAL_NUMBER_OF_ROWS 16
#define TOTAL_NUMBER_OF_COLUMNS 8

typedef struct Alpha_Display_Config_Tag Alpha_Display_Config_t;
typedef bool Alpha_Display_Status_t;

Alpha_Display_Config_t* alpha_display_init(I2C_HandleTypeDef* const hi2c, const uint8_t addrs);

Alpha_Display_Status_t clearDisplay(Alpha_Display_Config_t* const pConfig);
Alpha_Display_Status_t charDisplay(Alpha_Display_Config_t* const pConfig, const uint8_t segmentNum,
    const char dispDigit);
Alpha_Display_Status_t digitDisplay(Alpha_Display_Config_t* const pConfig, const uint8_t segmentNum, const char dispDigit);
Alpha_Display_Status_t alphaDisplay(Alpha_Display_Config_t* const pConfig, const uint8_t segmentNum, const char dispAlpha);

#if 1
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

#endif
//7 Segment Numerical Display
static const uint8_t sevenSegmentChars[10] = {
  0x3F, /* 0 */
  0x06, /* 1 */
  0x5B, /* 2 */
  0x4F, /* 3 */
  0x66, /* 4 */
  0x6D, /* 5 */
  0x7C, /* 6 */
  0x07, /* 7 */
  0x7F, /* 8 */
  0x67, /* 9 */
};


#endif /* TIME_CIRCUITS_ALPHANUMERICAL_DISPLAY_H_ */
