/*
 * timecircuit_control.h
 *
 *  Created on: Nov 7, 2024
 *      Author: Professor Gizmo
 */

#ifndef TIME_CIRCUITS_TIMECIRCUIT_CONTROL_H_
#define TIME_CIRCUITS_TIMECIRCUIT_CONTROL_H_

#include "datetime_display.h"
#include "keypad3x4w.h"
//#include "main.h"


#define DESTINATION_DISPLAY_I2C_ADDRESS     0x71
#define PRESENT_DISPLAY_I2C_ADDRESS         0x72
#define DEPARTED_DISPLAY_I2C_ADDRESS        0x74

#define KEYPAD_WHITE_INDICATOR_PIN          GPIO_PIN_8
#define KEYPAD_WHITE_INDICATOR_GPIO_PORT    GPIOB

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

#define BUTTON_DEBOUNCE_TIME_MS     100
#define PRESENT_TIME_UPDATE_TIME_MS 30000

typedef struct TimeCircuit_Control_Config_Tag TimeCircuit_Control_Config_t;
typedef bool TimeCircuit_Control_Status_t;

static const uint8_t defaultDestinationTimeData[]   = {1,0,2,7,1,9,8,5,1,1,0,0};
static const uint8_t defaultPresentTimeData[]       = {0,7,0,5,2,0,1,5,2,3,5,9};
static const uint8_t defaultLastDepartedTimeData[]  = {1,1,1,2,1,9,5,5,2,2,0,4};

static const uint8_t glitchDestinationTimeData[]    = {0,1,0,1,1,8,8,5,1,2,0,0};
static const char glitchDisplayChars[] = {'0','0','9','0','0','0','9','0','0','0'};

//static const char glitchDisplayChars[2][10] = {
//    {'0','0','9','0','0','0','9','0','0','0'},
//    {'0','0','9','9','0','0','0','0','0','0'},
//};

TimeCircuit_Control_Config_t* timeCircuit_control_init(I2C_HandleTypeDef* const hi2c, RTC_HandleTypeDef* hrtc);
TimeCircuit_Control_Status_t  timeCircuit_control_update(TimeCircuit_Control_Config_t* const pConfig);
TimeCircuit_Control_Status_t  timeCircuit_control_setDefaultDisplays(TimeCircuit_Control_Config_t * const pConfig);

static Keypad3x4w_PinConfig_t timeCircuitKeypadPinParam[] =
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


#endif /* TIME_CIRCUITS_TIMECIRCUIT_CONTROL_H_ */
