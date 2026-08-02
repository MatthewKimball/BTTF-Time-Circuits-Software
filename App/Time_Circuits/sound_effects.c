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


// Monotonic completion counters instead of a single overwritable "last event"
// flag: a single shared variable can silently lose an event if SoundTask is
// delayed long enough for two DMA interrupts to fire before it's polled once,
// which was causing stale/repeated audio mid-playback. Counters can't lose
// events - soundEffects_update() drains however many are pending each call.
volatile uint32_t gHalfCompleteCount = 0;
volatile uint32_t gFullCompleteCount = 0;
static uint32_t sHalfProcessed = 0;
static uint32_t sFullProcessed = 0;

volatile uint32_t gPlayedBytes            = 0;   // bytes that I2S has transmitted so far
uint32_t gRecordingBytes = 0;
static uint16_t gSamples[4096];

#define FADE_OUT_FRAMES 480u  // ~30ms at 16kHz stereo
#define FADE_OUT_WORDS  (FADE_OUT_FRAMES * 2u)

// Most source WAV files end abruptly at significant amplitude with no
// authored fade-out, which produces an audible click/pop on any correct,
// bug-free playback. Ramp the tail of the real audio down to silence in
// software so every file gets a clean stop regardless of how it was mastered.
static void applyFadeOut(uint16_t* buf, uint32_t validWords)
{
    uint32_t fadeWords = (validWords < FADE_OUT_WORDS) ? validWords : FADE_OUT_WORDS;
    if (fadeWords < 2) return;
    uint32_t fadeFrames = fadeWords / 2u;
    uint32_t startWord  = validWords - fadeWords;
    for (uint32_t f = 0; f < fadeFrames; f++) {
        float scale = 1.0f - ((float)(f + 1u) / (float)fadeFrames);
        uint32_t idx = startWord + f * 2u;
        buf[idx]     = (int16_t)((float)(int16_t)buf[idx]     * scale);
        buf[idx + 1] = (int16_t)((float)(int16_t)buf[idx + 1] * scale);
    }
}

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


volatile uint32_t gDbgPlaySoundEntryCount = 0;

SoundEffects_Status_t soundEffects_playSound(SoundEffects_Config_t* pCfg,
    StorageDevice_Config_t* pSD, const char* const filename)
{
    gDbgPlaySoundEntryCount++;
    HAL_I2S_DMAStop(pCfg->hi2s);
    storageDevice_closeSoundFile(pSD);

    gHalfCompleteCount = 0;
    gFullCompleteCount = 0;
    sHalfProcessed  = 0;
    sFullProcessed  = 0;
    gPlayedBytes    = 0;
    gRecordingBytes = 0;

    uint32_t totalBytes = 0, dummy = 0;
    if (!storageDevice_readWavDataSize(pSD, filename, &totalBytes, &dummy)) {
        // SD card went not-ready (e.g. after many rapid open/close cycles).
        // Force a fresh disk_initialize() via remount and retry once.
        storageDevice_mountDrive(pSD);
        storageDevice_readWavDataSize(pSD, filename, &totalBytes, &dummy);
    }
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
    // Unmute now that real audio is flowing again - masks any residual
    // glitch from the DMA stop/restart transition itself.
    HAL_GPIO_WritePin(AMPLIFIER_SHUTDOWN_GPIO_PORT, AMPLIFIER_SHUTDOWN_PIN, GPIO_PIN_SET);
    return 1;
}


SoundEffects_Status_t soundEffects_update(SoundEffects_Config_t* pCfg,
    StorageDevice_Config_t * pSD)
{
    const uint32_t HALF_BYTES = 4096;   // half-buffer = 2048 samples = 4096 bytes

    if (gPlayedBytes >= gRecordingBytes) {
        // Mute before stopping the clock - masks any pop from the I2S
        // clock/word-select itself stopping abruptly, independent of the
        // (already-faded-to-zero) sample values. Give the amp a couple of ms
        // to actually engage the mute before disturbing the clock.
        HAL_GPIO_WritePin(AMPLIFIER_SHUTDOWN_GPIO_PORT, AMPLIFIER_SHUTDOWN_PIN, GPIO_PIN_RESET);
        HAL_Delay(2);
        HAL_I2S_DMAStop(pCfg->hi2s);
        gPlayedBytes = gRecordingBytes; // prevent runaway
        gIsPlaying = false;
        storageDevice_closeSoundFile(pSD);
        return 1;
    }

    // Drain every pending completion, not just the most recent one - if
    // SoundTask was delayed long enough for both halves to complete before
    // being polled, both refills still happen here, in order.
    while (sHalfProcessed != gHalfCompleteCount) {
        sHalfProcessed++;

        uint32_t bytesLeft = (gRecordingBytes > gPlayedBytes) ? (gRecordingBytes - gPlayedBytes) : 0;
        uint32_t toRead    = (bytesLeft >= HALF_BYTES) ? HALF_BYTES : bytesLeft;
        toRead &= ~1u; // enforce 16-bit alignment

        uint32_t justRead = 0;
        if (toRead > 0) {
            storageDevice_readFileData(pSD, gSamples, toRead, &justRead);
        }
        if (justRead < HALF_BYTES) {
            applyFadeOut(gSamples, justRead / 2u);
            uint8_t* p = (uint8_t*)gSamples;
            memset(p + justRead, 0, HALF_BYTES - justRead);
            if (toRead > 0 && justRead < toRead && (gPlayedBytes + toRead) > gRecordingBytes) {
                gRecordingBytes = gPlayedBytes + toRead;
            }
        }
    }

    while (sFullProcessed != gFullCompleteCount) {
        sFullProcessed++;

        uint32_t bytesLeft = (gRecordingBytes > gPlayedBytes) ? (gRecordingBytes - gPlayedBytes) : 0;
        uint32_t toRead    = (bytesLeft >= HALF_BYTES) ? HALF_BYTES : bytesLeft;
        toRead &= ~1u;

        uint32_t justRead = 0;
        if (toRead > 0) {
            storageDevice_readFileData(pSD, &gSamples[2048], toRead, &justRead);
        }
        if (justRead < HALF_BYTES) {
            applyFadeOut(&gSamples[2048], justRead / 2u);
            uint8_t* p = (uint8_t*)&gSamples[2048];
            memset(p + justRead, 0, HALF_BYTES - justRead);
            if (toRead > 0 && justRead < toRead && (gPlayedBytes + toRead) > gRecordingBytes) {
                gRecordingBytes = gPlayedBytes + toRead;
            }
        }
    }

    return 1;
}





void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    gPlayedBytes += 4096u;   // 2048 samples * 2 bytes
    gHalfCompleteCount++;
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    gPlayedBytes += 4096u;
    gFullCompleteCount++;
}
