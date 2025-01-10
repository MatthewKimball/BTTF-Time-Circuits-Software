/*
 * sound_effects.c
 *
 *  Created on: Dec 6, 2024
 *      Author: Professor Gizmo
 */

#include "sound_effects.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


struct SoundEffects_Config_Tag
{
  I2S_HandleTypeDef*        hi2s;
  GPIO_TypeDef*             pMuteSwitchGpioPort;
  uint16_t                  pMuteSwitchGpioPin;
  GPIO_TypeDef*             pAmpEnableGpioPort;
  uint16_t                  pAmpEnableGpioPin;
  SoundEffect_Activation_e  activationState;
} SoundEffects_Config;

CallBack_Result_t gCallbackResult = UNKNOWN;
uint32_t gFileReadSize            = 0;
uint32_t gRecordingSize           = 0;
uint32_t gPlayedSize              = 0;
uint16_t gSamples[1000];

SoundEffects_Config_t * soundEffects_init(I2S_HandleTypeDef* hi2s, GPIO_TypeDef* const pGpioPort, const uint16_t* const pGpioPin)
{
  SoundEffects_Config_t* pConfig = malloc(sizeof(SoundEffects_Config_t));
  pConfig->hi2s = hi2s;
  pConfig->pMuteSwitchGpioPort = pGpioPort;
  pConfig->pMuteSwitchGpioPin = *pGpioPin;

  return pConfig;
}

SoundEffects_Status_t soundEffects_activate(SoundEffects_Config_t* pSoundEffectConfig)
{

  HAL_GPIO_WritePin(pSoundEffectConfig->pMuteSwitchGpioPort, pSoundEffectConfig->pMuteSwitchGpioPin, pSoundEffectConfig->activationState);

  return 1;
}

SoundEffects_Status_t soundEffects_initPlaySound(SoundEffects_Config_t* pSoundEffectConfig,
    StorageDevice_Config_t* pStorageDeviceConfig)
{

  storageDevice_readWavDataSize(pStorageDeviceConfig, "enter.wav", &gRecordingSize, &gFileReadSize);

  return 1;
}

SoundEffects_Status_t soundEffects_playSound(SoundEffects_Config_t* pSoundEffectConfig,
    StorageDevice_Config_t* pStorageDeviceConfig, const char* const filename)
{

  if (gPlayedSize < gRecordingSize)
  {
    HAL_I2S_DMAStop(pSoundEffectConfig->hi2s);
    storageDevice_closeFile(pStorageDeviceConfig);
  }

  gFileReadSize            = 0;
  gRecordingSize           = 0;
  gPlayedSize              = 0;

  storageDevice_readWavDataSize(pStorageDeviceConfig, filename, &gRecordingSize, &gFileReadSize);
  storageDevice_readFileData(pStorageDeviceConfig, gSamples, 2000, &gFileReadSize);
  HAL_I2S_Transmit_DMA(pSoundEffectConfig->hi2s,(uint16_t *) gSamples, 1000);
  //soundEffects_update(pSoundEffectConfig, pStorageDeviceConfig);
  //storageDevice_closeFile(pStorageDeviceConfig);

  return 1;
}

SoundEffects_Status_t soundEffects_update(SoundEffects_Config_t* pSoundEffectConfig,
    StorageDevice_Config_t * pStorageDeviceConfig)
{
  if (gPlayedSize >= gRecordingSize)
  {
    HAL_I2S_DMAStop(pSoundEffectConfig->hi2s);
    storageDevice_closeFile(pStorageDeviceConfig);
  }

  if(gCallbackResult == HALF_COMPLETED)
  {

    storageDevice_readFileData(pStorageDeviceConfig, gSamples, 1000, &gFileReadSize);

    gCallbackResult = UNKNOWN;
  }

  if(gCallbackResult == FULL_COMPLETED)
  {
    storageDevice_readFileData(pStorageDeviceConfig, &gSamples[500], 1000, &gFileReadSize);

    gCallbackResult = UNKNOWN;
  }

return 1;
}



void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
  gCallbackResult = HALF_COMPLETED;
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
  gCallbackResult =   FULL_COMPLETED;
  gPlayedSize     +=  1000;

}
