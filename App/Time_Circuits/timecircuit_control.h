/*
 * timecircuit_control.h
 *
 *  Created on: Nov 7, 2024
 *      Author: Professor Gizmo
 */

#ifndef TIMECIRCUIT_CONTROL_H_
#define TIMECIRCUIT_CONTROL_H_

#include <time.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

typedef struct  TimeCircuit_Control_Config_Tag TimeCircuit_Control_Config_t;
//typedef bool    TimeCircuit_Control_Status_t;

//***Choose one of the RTCs***//
// *******************************************************
//#define SET_INTERNAL_RTC // For internal RTC built into STM32 Microcontroller
#define SET_EXTERNAL_RTC // For external RTC DS3231
// *******************************************************


typedef enum {
    TIMECIRCUIT_CONTROL_OK = 0,
    TIMECIRCUIT_CONTROL_ERROR = 1,             // Generic error
    TIMECIRCUIT_CONTROL_INVALID_INPUT = 2,     // Bad keypad/CAN input
    TIMECIRCUIT_CONTROL_RTC_FAIL = 3,          // RTC read/write failed
    TIMECIRCUIT_CONTROL_CAN_COMM_ERROR = 4,    // CAN communication error
    TIMECIRCUIT_CONTROL_STATE_CONFLICT = 5,    // Invalid state transition
    TIMECIRCUIT_CONTROL_NOT_READY = 6,         // Waiting for valid config/input
    TIMECIRCUIT_CONTROL_BUSY = 7               // Currently processing something
} TimeCircuit_Control_Status_t;


typedef enum {
    TIME_CIRCUITS_IDLE     = 0,
    TIME_CIRCUITS_ARMED    = 1,
    TIME_CIRCUITS_TRAVEL   = 2,
    TIME_CIRCUITS_COMPLETE = 3
} TimeCircuit_Control_State_t;

TimeCircuit_Control_Config_t* timeCircuit_control_init(I2C_HandleTypeDef* const hi2c_display,
    I2C_HandleTypeDef* const hi2c_rtc, RTC_HandleTypeDef* hrtc, I2S_HandleTypeDef* hi2s);
TimeCircuit_Control_Status_t  timeCircuit_control_initRTC(TimeCircuit_Control_Config_t* const pConfig);
TimeCircuit_Control_Status_t  timeCircuit_control_update(TimeCircuit_Control_Config_t* const pConfig);
TimeCircuit_Control_Status_t  timeCircuit_control_setDefaultDisplays(TimeCircuit_Control_Config_t * const pConfig);
TimeCircuit_Control_Status_t  timeCircuit_control_updateStartUpDateTimes(TimeCircuit_Control_Config_t * const pConfig);
TimeCircuit_Control_Status_t  timeCircuit_control_updateDisplays(TimeCircuit_Control_Config_t * const pConfig);
TimeCircuit_Control_Status_t  timeCircuit_control_setRtcDateTime(TimeCircuit_Control_Config_t * const pConfig);
TimeCircuit_Control_Status_t timeCircuit_control_deInit(TimeCircuit_Control_Config_t* const pConfig);


TimeCircuit_Control_Status_t timeCircuit__toggleTimeColon(TimeCircuit_Control_Config_t* const pConfig);
TimeCircuit_Control_Status_t timeCircuit__setColonState(TimeCircuit_Control_Config_t* const pConfig, uint8_t colonOn);



#endif /* TIMECIRCUIT_CONTROL_H_ */
