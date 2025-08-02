/*
 * sound_effects.h
 *
 *  Created on: Dec 6, 2024
 *      Author: Professor Gizmo
 */

#ifndef TIME_CIRCUITS_SOUND_EFFECTS_H_
#define TIME_CIRCUITS_SOUND_EFFECTS_H_

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <storagedevice_control.h>

typedef struct SoundEffects_Config_Tag SoundEffects_Config_t;
typedef bool SoundEffects_Status_t;

typedef enum {
  UNKNOWN,
  HALF_COMPLETED,
  FULL_COMPLETED,
} CallBack_Result_t;

typedef enum
{
  SoundEffect_Activation_Disabled = 0,
  SoundEffect_Activation_Enabled  = 1,

}SoundEffect_Activation_e;

SoundEffects_Config_t* soundEffects_init(I2S_HandleTypeDef* hi2s, GPIO_TypeDef* pGpioPort, uint16_t gpioPin);
void soundEffects_deinit(SoundEffects_Config_t* pSoundEffectConfig);
SoundEffects_Status_t soundEffects_update(SoundEffects_Config_t* pSoundEffectConfig,
    StorageDevice_Config_t* pStorageDeviceConfig);
SoundEffects_Status_t soundEffects_playSound(SoundEffects_Config_t* pSoundEffectConfig,
    StorageDevice_Config_t* pStorageDeviceConfig, const char* const filename);
SoundEffects_Status_t soundEffects_initPlaySound(SoundEffects_Config_t* pSoundEffectConfig,
    StorageDevice_Config_t* pStorageDeviceConfig);
SoundEffects_Status_t soundEffects_stopSound(SoundEffects_Config_t* pSoundEffectConfig,
    StorageDevice_Config_t* pStorageDeviceConfig);

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s);
void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s);

#endif /* TIME_CIRCUITS_SOUND_EFFECTS_H_ */
