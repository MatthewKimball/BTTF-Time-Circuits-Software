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
#include <stdbool.h>

extern uint32_t gPlayedSize, gRecordingSize;
extern bool gIsPlaying;
extern StorageDevice_Config_t* gStorageConfig;

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

SoundEffects_Config_t* soundEffects_init(I2S_HandleTypeDef* hi2s, GPIO_TypeDef* pGpioPort, uint16_t gpioPin)
{
  SoundEffects_Config_t* pConfig = malloc(sizeof(SoundEffects_Config_t));
  pConfig->hi2s = hi2s;
  pConfig->pMuteSwitchGpioPort = pGpioPort;
  pConfig->pMuteSwitchGpioPin  = gpioPin;
  return pConfig;
}

void soundEffects_deinit(SoundEffects_Config_t* pSoundEffectConfig)
{
  free(pSoundEffectConfig);
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

  gFileReadSize = 0;
  gRecordingSize = 0;
  gPlayedSize = 0;

  storageDevice_readWavDataSize(pStorageDeviceConfig, filename, &gRecordingSize, &gFileReadSize);
  storageDevice_readFileData(pStorageDeviceConfig, gSamples, 2000, &gFileReadSize);

  gIsPlaying = true;

  uint16_t transferSize = (gRecordingSize < 1000) ? gRecordingSize : 1000;
  HAL_I2S_Transmit_DMA(pSoundEffectConfig->hi2s, gSamples, transferSize);

  return 1;
}

SoundEffects_Status_t soundEffects_stopSound(SoundEffects_Config_t* pSoundEffectConfig,
    StorageDevice_Config_t* pStorageDeviceConfig)
{

    HAL_I2S_DMAStop(pSoundEffectConfig->hi2s);
    storageDevice_closeFile(pStorageDeviceConfig);

  return 1;
}

SoundEffects_Status_t soundEffects_update(SoundEffects_Config_t* pSoundEffectConfig,
    StorageDevice_Config_t * pStorageDeviceConfig)
{
  if (gPlayedSize >= gRecordingSize)
  {
    soundEffects_stopSound(pSoundEffectConfig, pStorageDeviceConfig);
    gIsPlaying = false;
    return 1;
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

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    extern bool gIsPlaying;
    extern uint32_t gPlayedSize, gRecordingSize;

    gPlayedSize += 1000;

    if (gPlayedSize >= gRecordingSize)
    {
        gIsPlaying = false;
    }

    gCallbackResult = FULL_COMPLETED;
}

void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    extern CallBack_Result_t gCallbackResult;
    gCallbackResult = HALF_COMPLETED;
}
