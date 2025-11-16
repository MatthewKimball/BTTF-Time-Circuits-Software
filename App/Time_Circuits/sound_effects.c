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
static uint16_t gSamples[4096];

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
    HAL_I2S_DMAStop(pCfg->hi2s);
    storageDevice_closeFile(pSD);

    gCallbackResult = UNKNOWN;
    gPlayedBytes    = 0;
    gRecordingBytes = 0;

    uint32_t totalBytes = 0, dummy = 0;
    storageDevice_readWavDataSize(pSD, filename, &totalBytes, &dummy);
    gRecordingBytes = totalBytes;

    // Prefill full buffer (8192 bytes)
    uint32_t justRead = 0;
    storageDevice_readFileData(pSD, gSamples, sizeof(gSamples), &justRead);
    if (justRead < sizeof(gSamples)) {
        uint8_t* p = (uint8_t*)gSamples;
        memset(p + justRead, 0, sizeof(gSamples) - justRead);
    }

    gIsPlaying = true;
    HAL_I2S_Transmit_DMA(pCfg->hi2s, gSamples, 4096);  // count in 16-bit samples
    return 1;
}


SoundEffects_Status_t soundEffects_update(SoundEffects_Config_t* pCfg,
    StorageDevice_Config_t * pSD)
{
    const uint32_t HALF_BYTES = 4096;   // half-buffer = 2048 samples = 4096 bytes

    if (gPlayedBytes >= gRecordingBytes) {
        // Feed one more zero buffer to flush out DMA pipeline
        memset(gSamples, 0, sizeof(gSamples));
        HAL_I2S_Transmit_DMA(pCfg->hi2s, gSamples, 4096);
        gPlayedBytes = gRecordingBytes; // prevent runaway
        // Now mark for graceful stop
        gIsPlaying = false;
        storageDevice_closeFile(pSD);
        gCallbackResult = UNKNOWN;
        return 1;
    }

    if (gCallbackResult == HALF_COMPLETED) {
        uint32_t bytesLeft = (gRecordingBytes > gPlayedBytes) ? (gRecordingBytes - gPlayedBytes) : 0;
        uint32_t toRead    = (bytesLeft >= HALF_BYTES) ? HALF_BYTES : bytesLeft;
        toRead &= ~1u; // enforce 16-bit alignment

        uint32_t justRead = 0;
        if (toRead > 0) {
            storageDevice_readFileData(pSD, gSamples, toRead, &justRead);
        }
        if (justRead < HALF_BYTES) {
            uint8_t* p = (uint8_t*)gSamples;
            memset(p + justRead, 0, HALF_BYTES - justRead);
            if (toRead > 0 && justRead < toRead && (gPlayedBytes + toRead) > gRecordingBytes) {
                gRecordingBytes = gPlayedBytes + toRead;
            }
        }

        gCallbackResult = UNKNOWN;
    }

    if (gCallbackResult == FULL_COMPLETED) {
        uint32_t bytesLeft = (gRecordingBytes > gPlayedBytes) ? (gRecordingBytes - gPlayedBytes) : 0;
        uint32_t toRead    = (bytesLeft >= HALF_BYTES) ? HALF_BYTES : bytesLeft;
        toRead &= ~1u;

        uint32_t justRead = 0;
        if (toRead > 0) {
            storageDevice_readFileData(pSD, &gSamples[2048], toRead, &justRead);
        }
        if (justRead < HALF_BYTES) {
            uint8_t* p = (uint8_t*)&gSamples[2048];
            memset(p + justRead, 0, HALF_BYTES - justRead);
            if (toRead > 0 && justRead < toRead && (gPlayedBytes + toRead) > gRecordingBytes) {
                gRecordingBytes = gPlayedBytes + toRead;
            }
        }

        gCallbackResult = UNKNOWN;
    }

    return 1;
}





void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    gPlayedBytes += 4096u;   // 2048 samples * 2 bytes
    gCallbackResult = HALF_COMPLETED;
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    gPlayedBytes += 4096u;
    gCallbackResult = FULL_COMPLETED;
}
