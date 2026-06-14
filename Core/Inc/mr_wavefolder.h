#ifndef MR_WAVEFOLDER_H
#define MR_WAVEFOLDER_H

/*
 * mr_wavefolder.h
 *
 * Digital Wavefolder for Audio DSP
 * Suitable for STM32 (e.g. STM32G474) real-time audio processing
 *
 * Author: Michael Ruck + ChatGPT
 * https://chatgpt.com/c/696764a1-21cc-832d-9139-9aee3a65e455
 */

#include <stdint.h>
#include <math.h>

#ifdef ARM_MATH_CM4
#include "arm_math.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ==============================
   Configuration / Constants
   ============================== */

#ifndef MR_WF_MAX_FOLDS
#define MR_WF_MAX_FOLDS 8
#endif

/* ==============================
   Data Structure
   ============================== */

typedef struct
{
    float drive;     // Input gain before folding (>1.0 = more folds)
    float offset;    // DC offset before folding (-1..+1)
    uint8_t folds;   // Number of folding stages (1..MR_WF_MAX_FOLDS)

} mr_wavefolder_t;

/* ==============================
   Initialization
   ============================== */

static inline void mr_wavefolder_init(mr_wavefolder_t *wf)
{
    wf->drive  = 1.0f;
    wf->offset = 0.0f;
    wf->folds  = 1;
}

/* ==============================
   Core Folding Function
   ============================== */

/*
 * Symmetric wave folding around ±1.0
 * Input and output range: -1.0 .. +1.0
 */
static inline float mr_wavefolder_fold(float x)
{
    if (x > 1.0f)
        return 2.0f - x;
    if (x < -1.0f)
        return -2.0f - x;
    return x;
}

/* ==============================
   Audio Processing (single sample)
   ============================== */

static inline float mr_wavefolder_process(mr_wavefolder_t *wf, float in)
{
    float x;

    /* Pre-gain and offset */
    x = (in + wf->offset) * wf->drive;

    /* Folding stages */
    uint8_t f = wf->folds;
    if (f > MR_WF_MAX_FOLDS)
        f = MR_WF_MAX_FOLDS;

    for (uint8_t i = 0; i < f; i++)
    {
        x = mr_wavefolder_fold(x);
    }

    /* Soft limiting (safety) */
    if (x > 1.0f)  x = 1.0f;
    if (x < -1.0f) x = -1.0f;

    return x;
}

#ifdef __cplusplus
}
#endif

/* ============================================================
   Usage Example (STM32 SAI Double Buffer Audio Callback)
   ============================================================

#include "mr_wavefolder.h"

mr_wavefolder_t wavefolder;

void AudioInit(void)
{
    mr_wavefolder_init(&wavefolder);
    wavefolder.drive  = 3.5f;
    wavefolder.folds  = 4;
    wavefolder.offset = 0.0f;
}

void ProcessAudioData(void)
{
    float outf;
    int32_t out;

    for (int n = 0; n < (AUDIO_OUT_BUFFER_SIZE / AUDIO_OUT_CHANNELS) - 1;
         n += AUDIO_OUT_CHANNELS)
    {
        // Beispiel: Sinus / Oszillator
        float osc = outf; // -1.0 .. +1.0

        // Wavefolder
        float folded = mr_wavefolder_process(&wavefolder, osc);

        // Float -> Int32 (PCM5102, signed)
        out = (int32_t)(folded * fToInt32 * volume_temp);

        audioOutBufPtr[n]     = out; // Left
        audioOutBufPtr[n + 1] = out; // Right
    }

    dataReadyFlag = 0;
}

*/

#endif /* MR_WAVEFOLDER_H */
