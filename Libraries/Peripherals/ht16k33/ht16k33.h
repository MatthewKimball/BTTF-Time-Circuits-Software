/*
 * ht16k33.h
 *
 *  Created on: 24 Feb 2023
 *      Author: Matthew Kimball
 */

#ifndef HT16K33_H_
#define HT16K33_H_

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_I2C_TIMOUT_MS         5
#define DISPLAY_DATA_REG_ADDRESS  0x00
#define SYSTEM_SETUP_REG_ADDRESS  0x02
#define DISPLAY_SETUP_REG_ADDRESS 0x08
#define DIMMING_SET_REG_ADDRESS   0x0E

typedef enum
{
  Ht16k33_DimmingDutyCycle_1_16  = 0x00,
  Ht16k33_DimmingDutyCycle_2_16  = 0x01,
  Ht16k33_DimmingDutyCycle_3_16  = 0x02,
  Ht16k33_DimmingDutyCycle_4_16  = 0x03,
  Ht16k33_DimmingDutyCycle_5_16  = 0x04,
  Ht16k33_DimmingDutyCycle_6_16  = 0x05,
  Ht16k33_DimmingDutyCycle_7_16  = 0x06,
  Ht16k33_DimmingDutyCycle_8_16  = 0x07,
  Ht16k33_DimmingDutyCycle_9_16  = 0x08,
  Ht16k33_DimmingDutyCycle_10_16 = 0x09,
  Ht16k33_DimmingDutyCycle_11_16 = 0x0A,
  Ht16k33_DimmingDutyCycle_12_16 = 0x0B,
  Ht16k33_DimmingDutyCycle_13_16 = 0x0C,
  Ht16k33_DimmingDutyCycle_14_16 = 0x0D,
  Ht16k33_DimmingDutyCycle_15_16 = 0x0E,
  Ht16k33_DimmingDutyCycle_16_16 = 0x0F,

} Ht16k33_DimmingDutyCycle_e;

typedef enum
{
  Ht16k33_BlinkingFrequency_Off   = 0x00,
  Ht16k33_BlinkingFrequency_2Hz   = 0x01,
  Ht16k33_BlinkingFrequency_1Hz   = 0x02,
  Ht16k33_BlinkingFrequency_0_5Hz = 0x03,
} Ht16k33_BlinkingFreq_e;

typedef enum
{
  Ht16k33_SystemOscillator_Off = 0x00, //Standby Mode
  Ht16k33_SystemOscillator_On  = 0x01, //Normal Mode
} Ht16k33_SystemOscillator_e;

typedef enum
{
  Ht16k33_DisplayStatus_Off = 0x00,
  Ht16k33_DisplayStatus_On  = 0x01,
} Ht16k33_DisplayStatus_e;

typedef struct
{
  uint8_t s           : 1;
  uint8_t unreserved  : 3;
  uint8_t regAddrs    : 4;
} Ht16k33_SysSetupReg_t;

typedef struct
{
  uint8_t d          : 1;
  uint8_t b          : 2;
  uint8_t unreserved : 1;
  uint8_t regAddrs   : 4;
} Ht16k33_DisplaySetupReg_t;

typedef struct
{
  uint8_t p        : 4;
  uint8_t regAddrs : 4;
} Ht16k33_DimmingSetReg_t;

typedef struct
{
  uint8_t a        : 4;
  uint8_t regAddrs : 4;
} Ht16k33_DisplayData_t;

typedef struct  Ht16k33_Config_Tag Ht16k33_Config_t;
typedef bool    Ht16k33_Status_t;

Ht16k33_Config_t* ht16k33_init(I2C_HandleTypeDef* const hi2c, const uint8_t addrs);

Ht16k33_Status_t ht16k33_setSystemSetup(Ht16k33_Config_t* const pConfig, const Ht16k33_DisplayStatus_e displayStatus);
Ht16k33_Status_t ht16k33_setDisplaySetup(Ht16k33_Config_t* const pConfig, const Ht16k33_DisplayStatus_e dispStatus,
    const Ht16k33_BlinkingFreq_e frequency);
Ht16k33_Status_t ht16k33_setDimming (Ht16k33_Config_t* const pConfig, const Ht16k33_DimmingDutyCycle_e dutyCycle);
Ht16k33_Status_t ht16k33_updateDisplayData (Ht16k33_Config_t* const pConfig, const uint8_t ramAddrs,
    uint8_t * const dispDataBuffer, const uint8_t dispDataBufferSize);
#endif /* HT16K33_H_ */
