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

// TEMPORARY diagnostics for the "clicking on repeated sounds" investigation
// - read back live over SWD, not meant to be committed. See
// gDbgPlaySoundEntryCount below for the existing precedent of this pattern.
volatile uint32_t gDbgFadeCallCount        = 0;
volatile uint32_t gDbgFadeAppliedCount     = 0; // calls that actually ramped >=1 frame
volatile uint32_t gDbgLastFadeValidWords   = 0;
volatile uint32_t gDbgLastFadeAppliedWords = 0;
volatile uint32_t gDbgLastFadeRecordingBytes = 0; // gRecordingBytes at call time, to identify which file
volatile int16_t  gDbgLastFadeStartSampleL = 0;   // sample value right before the ramp begins
volatile int16_t  gDbgLastFadeStartSampleR = 0;
volatile int16_t  gDbgLastFadeEndSampleL   = 0;   // sample value at the very last ramped frame - should land at ~0
volatile int16_t  gDbgLastFadeEndSampleR   = 0;

// Most source WAV files end abruptly at significant amplitude with no
// authored fade-out, which produces an audible click/pop on any correct,
// bug-free playback. Ramp the tail of the real audio down to silence in
// software so every file gets a clean stop regardless of how it was mastered.
static void applyFadeOut(uint16_t* buf, uint32_t validWords)
{
    gDbgFadeCallCount++;
    gDbgLastFadeValidWords = validWords;
    gDbgLastFadeRecordingBytes = gRecordingBytes;

    uint32_t fadeWords = (validWords < FADE_OUT_WORDS) ? validWords : FADE_OUT_WORDS;

    // A fixed 30ms tail is inaudible on a multi-second one-shot (startup,
    // enter, locked) but can be a huge fraction of a short, frequently
    // repeated cue (the colon tick, the glitch beep) - fading 30ms of a
    // ~100ms clip tapers off a quarter or more of the whole sound, which is
    // heard as the clip getting cut short/quiet rather than a clean stop.
    // Never fade more than a quarter of the file's total length.
    uint32_t totalWords  = gRecordingBytes / 2u;
    uint32_t maxFadeWords = totalWords / 4u;
    if (fadeWords > maxFadeWords) fadeWords = maxFadeWords;

    gDbgLastFadeAppliedWords = fadeWords;
    if (fadeWords < 2) return;
    uint32_t fadeFrames = fadeWords / 2u;
    uint32_t startWord  = validWords - fadeWords;

    gDbgLastFadeStartSampleL = (int16_t)buf[startWord];
    gDbgLastFadeStartSampleR = (int16_t)buf[startWord + 1];

    // Diagnostic note (clicking-on-repeated-sounds investigation): tried
    // a smoothstep (3t^2 - 2t^3) curve here instead of this straight
    // linear ramp, on the theory that a linear ramp's slope discontinuity
    // right where the fade ends and true silence begins (constant
    // negative slope, then an abrupt snap to zero slope) could itself be
    // an audible click even with the sample VALUE landing on exact zero
    // either way. Unchanged on hardware; ruled out. Reverted to linear.
    for (uint32_t f = 0; f < fadeFrames; f++) {
        float scale = 1.0f - ((float)(f + 1u) / (float)fadeFrames);
        uint32_t idx = startWord + f * 2u;
        buf[idx]     = (int16_t)((float)(int16_t)buf[idx]     * scale);
        buf[idx + 1] = (int16_t)((float)(int16_t)buf[idx + 1] * scale);
    }

    gDbgFadeAppliedCount++;
    gDbgLastFadeEndSampleL = (int16_t)buf[startWord + (fadeFrames - 1u) * 2u];
    gDbgLastFadeEndSampleR = (int16_t)buf[startWord + (fadeFrames - 1u) * 2u + 1u];
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

  // Only the remote mute-all bit controls this - the physical Mute switch
  // is intentionally scoped to just the colon tick sound
  // (!gSoundRemoteMuteColon && !gSoundMuteSw, elsewhere), not general
  // playback. Previously the GPIO write here was commented out entirely,
  // so remote mute-all silently did nothing.
  bool muteRequested = gSoundRemoteMuteAll;

  if (previousState != muteRequested)
  {
    // SET = amp enabled/unmuted, RESET = amp disabled/muted - matches the
    // convention already used around DMA start/stop in
    // soundEffects_playSound()/soundEffects_update().
    HAL_GPIO_WritePin(AMPLIFIER_SHUTDOWN_GPIO_PORT, AMPLIFIER_SHUTDOWN_PIN,
        muteRequested ? GPIO_PIN_RESET : GPIO_PIN_SET);
    previousState = muteRequested;
  }
}

void soundEffects_readMuteSwitch(void)
{
  // Same electrical setup (GPIO_MODE_INPUT, GPIO_NOPULL) as the Glitch and
  // Keypad Enter switches, both of which invert their reading - this one
  // didn't, so it read backwards: HIGH (the switch's normal resting state,
  // confirmed live at GPIOC->IDR bit10) was taken as "mute requested",
  // permanently muting all sound regardless of the Mute switch or Mute All.
  gSoundMuteSw = !HAL_GPIO_ReadPin(MUTE_SWITCH_GPIO_PORT, MUTE_SWITCH_PIN);
}


volatile uint32_t gDbgPlaySoundEntryCount = 0;
volatile uint32_t gDbgCutShortCount = 0; // playSound() re-entered while the previous file was still mid-playback
volatile uint32_t gDbgLastCutShortRecordingBytes = 0; // the interrupted file's total size, to identify it
volatile uint32_t gDbgLastCutShortPlayedBytes    = 0; // how far into it playback had gotten
volatile uint32_t gDbgLastPlayRecordingBytes = 0; // gRecordingBytes actually used for the most recent play
volatile uint32_t gDbgLastPlayPrefillRead    = 0; // justRead from the initial 8192-byte prefill
volatile uint32_t gDbgLastPlayNameWord       = 0; // filename's first 4 chars packed into a uint32, to identify it
volatile uint32_t gDbgSizeReadFailCount      = 0; // storageDevice_readWavDataSize() failed even after retry

// Diagnostic note (clicking-on-repeated-sounds investigation): tried
// keeping the I2S clock running for the entire session instead of
// stopping/restarting it on every playSound() call - the MAX98357A
// datasheet confirms the ICs "automatically enter standby mode when
// BCLK is removed", a real state transition this was meant to avoid
// re-triggering on every repeat of a sound like the colon tick. Started
// the DMA exactly once and let new sounds refill the already-circulating
// buffer directly (accepting an unsynchronized write into a live DMA
// buffer as a quick way to test the theory). Click was unchanged on
// hardware - ruled out - so reverted to the simpler, already-safe
// stop/restart-per-sound behavior rather than keep that unproven race
// around for no benefit.

SoundEffects_Status_t soundEffects_playSound(SoundEffects_Config_t* pCfg,
    StorageDevice_Config_t* pSD, const char* const filename)
{
    gDbgPlaySoundEntryCount++;
    if (gIsPlaying) {
        gDbgCutShortCount++;
        gDbgLastCutShortRecordingBytes = gRecordingBytes;
        gDbgLastCutShortPlayedBytes    = gPlayedBytes;
    }
    HAL_I2S_DMAStop(pCfg->hi2s);
    storageDevice_closeSoundFile(pSD);

    gHalfCompleteCount = 0;
    gFullCompleteCount = 0;
    sHalfProcessed  = 0;
    sFullProcessed  = 0;
    gPlayedBytes    = 0;
    gRecordingBytes = 0;

    gDbgLastPlayNameWord = ((uint32_t)filename[0])
        | ((uint32_t)filename[1] << 8)
        | ((uint32_t)filename[2] << 16)
        | ((uint32_t)filename[3] << 24);

    uint32_t totalBytes = 0, dummy = 0;
    if (!storageDevice_readWavDataSize(pSD, filename, &totalBytes, &dummy)) {
        // SD card went not-ready (e.g. after many rapid open/close cycles).
        // Force a fresh disk_initialize() via remount and retry once.
        storageDevice_mountDrive(pSD);
        if (!storageDevice_readWavDataSize(pSD, filename, &totalBytes, &dummy)) {
            gDbgSizeReadFailCount++;
        }
    }
    gRecordingBytes = totalBytes;
    gDbgLastPlayRecordingBytes = totalBytes;

    // Prefill full buffer (8192 bytes)
    uint32_t justRead = 0;
    storageDevice_readFileData(pSD, gSamples, sizeof(gSamples), &justRead);
    gDbgLastPlayPrefillRead = justRead;
    if (justRead < sizeof(gSamples)) {
        // A clip short enough to fit entirely in this one prefill (e.g. the
        // colon tick, the glitch cue) never passes through
        // soundEffects_update()'s refill loop, which is the only other place
        // that calls applyFadeOut() - without it here too, its raw, unfaded
        // ending plays at full amplitude right up to the abrupt mute/DMA-stop
        // that follows once playback completes, producing a click.
        applyFadeOut(gSamples, justRead / 2u);
        uint8_t* p = (uint8_t*)gSamples;
        memset(p + justRead, 0, sizeof(gSamples) - justRead);
    }

    gIsPlaying = true;
    HAL_I2S_Transmit_DMA(pCfg->hi2s, gSamples, 4096);  // count in 16-bit samples
    // Unmute now that real audio is flowing again - masks any residual
    // glitch from the DMA stop/restart transition itself. Only if remote
    // mute-all isn't asking to stay muted, though: this used to run
    // unconditionally, so starting ANY sound (e.g. a keypad tone) silently
    // re-enabled the amplifier even while Mute All was engaged, defeating
    // it - soundEffects_disableAmplifier() would mute again on its next
    // 20ms tick, but by then the click/pop had already played audibly.
    // The physical Mute switch (gSoundMuteSw) is deliberately not checked
    // here - it only scopes the colon tick sound, not general playback.
    if (!gSoundRemoteMuteAll) {
      HAL_GPIO_WritePin(AMPLIFIER_SHUTDOWN_GPIO_PORT, AMPLIFIER_SHUTDOWN_PIN, GPIO_PIN_SET);
    }
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
        //
        // Diagnostic notes (clicking-on-repeated-sounds investigation):
        // temporarily skipping this mute pulse, separately temporarily
        // skipping the HAL_I2S_DMAStop() below entirely, and separately
        // still keeping the I2S clock running for the whole session
        // (never stopping it at all, see the removed gDmaStarted) were
        // all tried on hardware - none removed the click on the colon
        // tick/glitch. All ruled out; restored as-is.
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

        // "justRead < HALF_BYTES" used to be the only signal for "this is
        // the last chunk of the file, fade+pad it" - but that's really a
        // proxy for bytesLeft, not the actual test. Whenever a file's
        // length lands on an exact HALF_BYTES multiple, its true final
        // chunk reads a full HALF_BYTES (bytesLeft == HALF_BYTES here), so
        // the old check missed it: that buffer played at raw amplitude
        // right up to the abrupt mute+DMA-stop the very next update() call
        // makes once gPlayedBytes catches up to gRecordingBytes - an
        // audible click, but only on files whose length happens to hit
        // that boundary, and only reproducible on sounds long enough to
        // reach a second/later refill (the short one-shot clips that fit
        // in a single prefill are faded unconditionally in
        // soundEffects_playSound() instead). Checking bytesLeft directly
        // catches that case too; the justRead<toRead half still catches a
        // genuine I/O shortfall mid-file, which needs the same treatment
        // plus shrinking gRecordingBytes to match reality.
        // Diagnostic note (clicking-on-repeated-sounds investigation):
        // tried muting right here too - as soon as this is known to be
        // the last chunk, ~64ms before the true end, instead of only the
        // ~2ms lead the natural-completion branch below gives it - in
        // case the amp's own click/pop suppression needed more time to
        // engage before the clock stops. Click was unchanged on
        // hardware; ruled out.
        bool isLastChunk = (bytesLeft <= HALF_BYTES) || (justRead < toRead);
        if (isLastChunk) {
            if (toRead > 0 && justRead < toRead && (gPlayedBytes + toRead) > gRecordingBytes) {
                gRecordingBytes = gPlayedBytes + toRead;
            }
            applyFadeOut(gSamples, justRead / 2u);
            uint8_t* p = (uint8_t*)gSamples;
            memset(p + justRead, 0, HALF_BYTES - justRead);
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

        // See the matching half-buffer loop above for why this checks
        // bytesLeft directly instead of just justRead < HALF_BYTES.
        bool isLastChunk = (bytesLeft <= HALF_BYTES) || (justRead < toRead);
        if (isLastChunk) {
            if (toRead > 0 && justRead < toRead && (gPlayedBytes + toRead) > gRecordingBytes) {
                gRecordingBytes = gPlayedBytes + toRead;
            }
            applyFadeOut(&gSamples[2048], justRead / 2u);
            uint8_t* p = (uint8_t*)&gSamples[2048];
            memset(p + justRead, 0, HALF_BYTES - justRead);
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
