/*
 * ht16k33.c
 *
 *  Created on: 24 Feb 2023
 *      Author: Matthew Kimball
 */

#include "ht16k33.h"

struct Ht16k33_Config_Tag
{
  I2C_HandleTypeDef*        hi2c;
  uint8_t                   i2cAddrs;
  Ht16k33_SysSetupReg_t     hSysSetupReg;
  Ht16k33_DisplaySetupReg_t hDisplayReg;
  Ht16k33_DimmingSetReg_t   hDimmingSetReg;
  Ht16k33_DisplayData_t     hDisplayAddrsPtrReg;

  //TODO (Matthew Kimball): Add keypad input functionality

} Ht16k33_Config;


Ht16k33_Config_t* ht16k33_init(I2C_HandleTypeDef* const hi2c, const uint8_t addrs)
{
  Ht16k33_Config_t* pConfig = malloc(sizeof(Ht16k33_Config_t));
  pConfig->i2cAddrs = addrs << 1;
  pConfig->hi2c = hi2c;

  pConfig->hSysSetupReg.regAddrs        = SYSTEM_SETUP_REG_ADDRESS;
  pConfig->hDisplayReg.regAddrs         = DISPLAY_SETUP_REG_ADDRESS;
  pConfig->hDimmingSetReg.regAddrs      = DIMMING_SET_REG_ADDRESS;
  pConfig->hDisplayAddrsPtrReg.regAddrs = DISPLAY_DATA_REG_ADDRESS;
  return pConfig;
}

Ht16k33_Status_t ht16k33_setSystemSetup(Ht16k33_Config_t* const pConfig, const Ht16k33_DisplayStatus_e displayStatus)
{
  pConfig->hSysSetupReg.s = displayStatus;

  uint8_t sysSetupReg = 0;
  sysSetupReg |= (pConfig->hSysSetupReg.s & 0x01) << 0;
  sysSetupReg |= (pConfig->hSysSetupReg.regAddrs & 0x0F) << 4;

  return
      HAL_I2C_Master_Transmit(pConfig->hi2c, pConfig->i2cAddrs, &sysSetupReg, sizeof(sysSetupReg),HAL_MAX_DELAY);
}

Ht16k33_Status_t ht16k33_setDisplaySetup(Ht16k33_Config_t* const pConfig, const Ht16k33_DisplayStatus_e dispStatus,
    const Ht16k33_BlinkingFreq_e frequency)
{
  pConfig->hDisplayReg.d = dispStatus;
  pConfig->hDisplayReg.b = frequency;

  uint8_t dispSetupReg = 0;

  dispSetupReg |= (pConfig->hDisplayReg.d & 0x01) << 0;
  dispSetupReg |= (pConfig->hDisplayReg.b & 0x03) << 1;
  dispSetupReg |= (pConfig->hDisplayReg.regAddrs & 0x0F) << 4;

  return
      HAL_I2C_Master_Transmit(pConfig->hi2c, pConfig->i2cAddrs, &dispSetupReg, sizeof(dispSetupReg),HAL_MAX_DELAY);
}

Ht16k33_Status_t ht16k33_setDimming (Ht16k33_Config_t* const pConfig, const Ht16k33_DimmingDutyCycle_e dutyCycle)
{
  pConfig->hDimmingSetReg.p = dutyCycle;

  uint8_t dimmingSetReg = 0;
  dimmingSetReg |= (pConfig->hDimmingSetReg.p & 0x0F) << 0;
  dimmingSetReg |= (pConfig->hDimmingSetReg.regAddrs & 0x0F) << 4;

  return
      HAL_I2C_Master_Transmit(pConfig->hi2c, pConfig->i2cAddrs, &dimmingSetReg, sizeof(dimmingSetReg),HAL_MAX_DELAY);
}

Ht16k33_Status_t ht16k33_updateDisplayData (Ht16k33_Config_t* const pConfig, const uint8_t ramAddrs,
    uint8_t* const dispDataBuffer, const uint8_t dispDataBufferSize)
{
  uint8_t* transmitBuffer     = malloc (dispDataBufferSize + 1);
  Ht16k33_Status_t isSuccess   = 0;
  uint8_t dispDataAddrsPtrReg = 0;

  pConfig->hDisplayAddrsPtrReg.a = ramAddrs;

  dispDataAddrsPtrReg |= (pConfig->hDisplayAddrsPtrReg.a & 0x0F) << 0;
  dispDataAddrsPtrReg |= (pConfig->hDisplayAddrsPtrReg.regAddrs & 0x0F) << 4;

  memcpy(transmitBuffer, &dispDataAddrsPtrReg, sizeof(dispDataAddrsPtrReg));
  memcpy(transmitBuffer + 1, dispDataBuffer, dispDataBufferSize);

  isSuccess = HAL_I2C_Master_Transmit(pConfig->hi2c, pConfig->i2cAddrs, transmitBuffer, (dispDataBufferSize + 1),
      HAL_MAX_DELAY);

  free(transmitBuffer);

  return isSuccess;
}

//TODO (Matthew Kimball): Add deconstructor
