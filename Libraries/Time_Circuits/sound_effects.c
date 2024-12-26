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
uint16_t gSamples[8000];

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

  storageDevice_readWavDataSize(pStorageDeviceConfig, "enter_v1.wav", &gRecordingSize, &gFileReadSize);
  storageDevice_readFileData(pStorageDeviceConfig, gSamples, 16000, &gFileReadSize);

  return 1;
}

SoundEffects_Status_t soundEffects_playSound(SoundEffects_Config_t* pSoundEffectConfig,
    StorageDevice_Config_t* pStorageDeviceConfig)
{
  gPlayedSize              = 0;
  HAL_I2S_Transmit_DMA(pSoundEffectConfig->hi2s,(uint16_t *) gSamples, 8000);


  return 1;
}

SoundEffects_Status_t soundEffects_update(SoundEffects_Config_t* pSoundEffectConfig,
    StorageDevice_Config_t * pStorageDeviceConfig)
{

  while(gPlayedSize < gRecordingSize)
  {

    if(gCallbackResult == HALF_COMPLETED)
    {
      storageDevice_readFileData(pStorageDeviceConfig, gSamples, 8000, &gFileReadSize);

      gCallbackResult = UNKNOWN;
    }

    if(gCallbackResult == FULL_COMPLETED)
    {
      storageDevice_readFileData(pStorageDeviceConfig, &gSamples[4000], 8000, &gFileReadSize);

      gCallbackResult = UNKNOWN;
    }

    if(gPlayedSize >= gRecordingSize)
    {
      HAL_I2S_DMAStop(pSoundEffectConfig->hi2s);
    }
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
  gPlayedSize     +=  8000;
}
