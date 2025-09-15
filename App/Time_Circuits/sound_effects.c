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
#include <string.h>

#define MUTE_SWITCH_PIN                     GPIO_PIN_10
#define MUTE_SWITCH_GPIO_PORT               GPIOC

#define AMPLIFIER_SHUTDOWN_PIN              GPIO_PIN_14
#define AMPLIFIER_SHUTDOWN_GPIO_PORT        GPIOB



extern bool gIsPlaying;
extern StorageDevice_Config_t* gStorageConfig;

bool gSoundMuteSw = false;
bool gSoundRemoteMuteAll= false;
bool gSoundRemoteMuteColon = false;

struct SoundEffects_Config_Tag
{
  I2S_HandleTypeDef*        hi2s;
  SoundEffect_Activation_e  activationState;
  bool                      bMuteState;
} SoundEffects_Config;


volatile CallBack_Result_t gCallbackResult = UNKNOWN;
volatile uint32_t gPlayedBytes            = 0;   // bytes that I2S has transmitted so far
uint32_t gRecordingBytes = 0;
static uint16_t gSamples[1000];

uint32_t gFileReadSize            = 0;
uint32_t gRecordingSize           = 0;
uint32_t gPlayedSize              = 0;



SoundEffects_Config_t* soundEffects_init(I2S_HandleTypeDef* hi2s)
{
  SoundEffects_Config_t* pConfig = malloc(sizeof(SoundEffects_Config_t));
  pConfig->hi2s = hi2s;

  return pConfig;
}

void soundEffects_deinit(SoundEffects_Config_t* pSoundEffectConfig)
{
  free(pSoundEffectConfig);
}

void soundEffects_disableAmplifier(void)
{
  static bool previousState = false;
  if (previousState != gSoundRemoteMuteAll)
  {
    //HAL_GPIO_WritePin(AMPLIFIER_SHUTDOWN_GPIO_PORT, AMPLIFIER_SHUTDOWN_PIN, gSoundRemoteMuteAll);
    previousState = gSoundRemoteMuteAll;
  }

}

void soundEffects_readMuteSwitch(void)
{
  gSoundMuteSw = HAL_GPIO_ReadPin(MUTE_SWITCH_GPIO_PORT, MUTE_SWITCH_PIN);

}


SoundEffects_Status_t soundEffects_playSound(SoundEffects_Config_t* pCfg,
    StorageDevice_Config_t* pSD, const char* const filename)
{
    // Stop anything current and close any open file
    HAL_I2S_DMAStop(pCfg->hi2s);
    storageDevice_closeFile(pSD);

    gCallbackResult = UNKNOWN;
    gPlayedBytes    = 0;
    gRecordingBytes = 0;

    // Get total PCM DATA size in BYTES (from WAV header 'data' chunk)
    uint32_t totalBytes = 0, dummy = 0;
    storageDevice_readWavDataSize(pSD, filename, &totalBytes, &dummy);
    gRecordingBytes = totalBytes;

    // Prefill FULL buffer (2000 bytes). Pad remainder with zeros if short.
    uint32_t justRead = 0;
    storageDevice_readFileData(pSD, gSamples, 2000, &justRead);
    if (justRead < 2000) {
        uint8_t* p = (uint8_t*)gSamples;
        memset(p + justRead, 0, 2000u - justRead);
    }

    gIsPlaying = true;
    HAL_I2S_Transmit_DMA(pCfg->hi2s, (uint16_t*)gSamples, 1000);  // count is 16-bit samples
    return 1;
}

SoundEffects_Status_t soundEffects_update(SoundEffects_Config_t* pCfg,
    StorageDevice_Config_t * pSD)
{
    // Stop when all bytes have been transmitted
    if (gPlayedBytes >= gRecordingBytes) {
        HAL_I2S_DMAStop(pCfg->hi2s);
        storageDevice_closeFile(pSD);
        gIsPlaying = false;
        gCallbackResult = UNKNOWN;
        return 1;
    }

    // Refill first half (0..999 bytes) after HALF complete
    if (gCallbackResult == HALF_COMPLETED) {
        uint32_t bytesLeft = (gRecordingBytes > gPlayedBytes) ? (gRecordingBytes - gPlayedBytes) : 0;
        uint32_t toRead    = (bytesLeft >= 1000u) ? 1000u : bytesLeft;

        uint32_t justRead = 0;
        if (toRead > 0) {
            storageDevice_readFileData(pSD, gSamples, toRead, &justRead);
        }
        if (justRead < 1000u) {
            uint8_t* p = (uint8_t*)gSamples;
            memset(p + justRead, 0, 1000u - justRead);
        }

        gCallbackResult = UNKNOWN;
    }

    // Refill second half (1000..1999 bytes) after FULL complete
    if (gCallbackResult == FULL_COMPLETED) {
        uint32_t bytesLeft = (gRecordingBytes > gPlayedBytes) ? (gRecordingBytes - gPlayedBytes) : 0;
        uint32_t toRead    = (bytesLeft >= 1000u) ? 1000u : bytesLeft;

        uint32_t justRead = 0;
        if (toRead > 0) {
            storageDevice_readFileData(pSD, &gSamples[500], toRead, &justRead);
        }
        if (justRead < 1000u) {
            uint8_t* p = (uint8_t*)&gSamples[500];
            memset(p + justRead, 0, 1000u - justRead);
        }

        gCallbackResult = UNKNOWN;
    }

    return 1;
}



void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    gPlayedBytes += 1000u;           // 500 samples * 2 bytes
    gCallbackResult = HALF_COMPLETED;
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    gPlayedBytes += 1000u;           // next 500 samples * 2 bytes
    gCallbackResult = FULL_COMPLETED;
}
