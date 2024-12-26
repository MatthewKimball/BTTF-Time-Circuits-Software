/*
 * timecircuit_control.h
 *
 *  Created on: Nov 7, 2024
 *      Author: Professor Gizmo
 */

#ifndef TIMECIRCUIT_CONTROL_H_
#define TIMECIRCUIT_CONTROL_H_

#include "datetime_display.h"
#include "keypad3x4w.h"
#include "storagedevice_control.h"
#include <time.h>

typedef struct  TimeCircuit_Control_Config_Tag TimeCircuit_Control_Config_t;
typedef bool    TimeCircuit_Control_Status_t;

TimeCircuit_Control_Config_t* timeCircuit_control_init(I2C_HandleTypeDef* const hi2c, RTC_HandleTypeDef* hrtc,
    SPI_HandleTypeDef* hspi, I2S_HandleTypeDef* hi2s);
TimeCircuit_Control_Status_t  timeCircuit_control_update(TimeCircuit_Control_Config_t* const pConfig);
TimeCircuit_Control_Status_t  timeCircuit_control_setDefaultDisplays(TimeCircuit_Control_Config_t * const pConfig);
TimeCircuit_Control_Status_t  timeCircuit_control_updateStartUpDateTimes(TimeCircuit_Control_Config_t * const pConfig);
TimeCircuit_Control_Status_t  timeCircuit_control_updateDisplays(TimeCircuit_Control_Config_t * const pConfig);
TimeCircuit_Control_Status_t  timeCircuit_control_setRtcDateTime(TimeCircuit_Control_Config_t * const pConfig);
TimeCircuit_Control_Status_t timeCircuit_control_deInit(TimeCircuit_Control_Config_t* const pConfig);

#endif /* TIMECIRCUIT_CONTROL_H_ */
