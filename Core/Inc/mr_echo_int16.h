/**
 ******************************************************************************
 * @file           : mr_echo_int16.h
 * @brief          : Echo-Effekt für STM32G474 Synthesizer
 * @author         : Michael Ruck michael.ruck@marsgasse.com + Claude
 * @date           : 2026-02-26
 *
 * @modified       : 2026-03-22 michael.ruck@marsgasse.com
 *                   Stereo-Unterstützung: echo_L und echo_R als separate
 *                   Instanzen in SRAM1 (via .sram2 Section nach Linker-Tausch).
 *                   Echo_ProcessSampleStereo() verarbeitet L und R in-place
 *                   via Pointer.
 *
 ******************************************************************************
 * @details
 * Echo-Effekt optimiert für STM32G474 mit SAI und PCM5102 DAC
 * Samplerate: 44270.83 Hz
 *
 * Optimierungen:
 * - Buffer int16 statt float32: 16KB statt 32KB RAM pro Instanz
 * - Beide Buffer in SRAM1 (via .sram2 Section): 2x16KB = 32KB
 * - Ringbuffer per Bitmaske statt if-Abfrage
 * - Feedback in Integer-Arithmetik (kein float im heißen Pfad)
 *
 * Signal-Range: int32, ±2147483647 (= outf * fToInt32 * volume_temp)
 * Buffer speichert obere 16 Bit (>> 16), Auflösungsverlust unhörbar bei Echo
 *
 ******************************************************************************
 */

#ifndef MR_ECHO_H_
#define MR_ECHO_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <string.h>

/* Defines -------------------------------------------------------------------*/
#define ECHO_BUFFER_SIZE     8192                        // Muss 2^n sein
#define ECHO_BUFFER_MASK     (ECHO_BUFFER_SIZE - 1)     // 0x1FFF – Bitmasken-Ringbuffer
#define ECHO_MAX_DELAY_MS    185                         // ~0.185s bei 44270.83 Hz
#define ECHO_SAMPLERATE      44270.83f

/* Typedef -------------------------------------------------------------------*/
typedef struct {
    int16_t  buffer[ECHO_BUFFER_SIZE];  ///< Ringbuffer: int16, 16KB
    uint32_t write_pos;                 ///< Aktuelle Schreibposition
    uint32_t delay_samples;             ///< Verzögerung in Samples
    int32_t  feedback_q15;             ///< Feedback als Q15-Fixpunkt (0..29491 = 0.0..0.9)
    float    mix;                       ///< 0.0 - 1.0 (Trocken/Nass-Verhältnis)
    int32_t  wet_q15;                  ///< mix als Q15
    int32_t  dry_q15;                  ///< (1.0 - mix) als Q15
} Echo_t;

/* Exported variables --------------------------------------------------------*/

/// .sram2 Section liegt nach Linker-Tausch (2026-03-22) in SRAM1 (96KB).
/// Beide Instanzen zusammen 32KB – passt problemlos.
__attribute__((section(".sram2"))) Echo_t echo_L;
__attribute__((section(".sram2"))) Echo_t echo_R;

/* Hilfsmakro: float 0.0..1.0 nach Q15 (0..32767) ---------------------------*/
#define FLOAT_TO_Q15(x)  ((int32_t)((x) * 32767.0f))

/* Function prototypes -------------------------------------------------------*/

/**
 * @brief  Initialisiert beide Echo-Instanzen (L und R) mit denselben Parametern
 * @param  delay_ms   Verzögerungszeit in ms (1 bis ECHO_MAX_DELAY_MS)
 * @param  feedback   Feedback-Stärke (0.0 bis 0.9)
 * @param  mix        Trocken/Nass-Verhältnis (0.0 bis 1.0)
 */
void Echo_Init(uint32_t delay_ms, float feedback, float mix);

/**
 * @brief  Verarbeitet ein Stereo-Sample-Paar in-place
 * @param  sampleL  Pointer auf linkes  Sample (int32, wird direkt überschrieben)
 * @param  sampleR  Pointer auf rechtes Sample (int32, wird direkt überschrieben)
 *
 * Verwendung:
 *   int32_t outIntL = (int32_t)(outL * fToInt32 * volume_temp);
 *   int32_t outIntR = (int32_t)(outR * fToInt32 * volume_temp);
 *   Echo_ProcessSampleStereo(&outIntL, &outIntR);
 *   audioOutBufPtr[n]     = outIntL;
 *   audioOutBufPtr[n + 1] = outIntR;
 */
void Echo_ProcessSampleStereo(int32_t *sampleL, int32_t *sampleR);

/**
 * @brief  Setzt Verzögerungszeit auf beiden Kanälen zur Laufzeit
 * @param  delay_ms  Neue Verzögerungszeit in ms
 */
void Echo_SetDelay(uint32_t delay_ms);

/**
 * @brief  Setzt Feedback auf beiden Kanälen zur Laufzeit
 * @param  feedback  0.0 bis 0.9
 */
void Echo_SetFeedback(float feedback);

/**
 * @brief  Setzt Mix-Verhältnis auf beiden Kanälen zur Laufzeit
 * @param  mix  0.0 bis 1.0
 */
void Echo_SetMix(float mix);

/**
 * @brief  Löscht beide Echo-Buffer
 */
void Echo_Clear(void);

/* Implementation ------------------------------------------------------------*/

static void echo_init_instance(Echo_t *e, uint32_t delay_ms, float feedback, float mix) {
    memset(e, 0, sizeof(Echo_t));

    uint32_t d = (uint32_t)(delay_ms * (ECHO_SAMPLERATE / 1000.0f) + 0.5f);
    if (d >= ECHO_BUFFER_SIZE) d = ECHO_BUFFER_SIZE - 1;
    if (d < 1)                 d = 1;
    e->delay_samples = d;

    if (feedback > 0.9f) feedback = 0.9f;
    if (feedback < 0.0f) feedback = 0.0f;
    e->feedback_q15 = FLOAT_TO_Q15(feedback);

    if (mix > 1.0f) mix = 1.0f;
    if (mix < 0.0f) mix = 0.0f;
    e->mix     = mix;
    e->wet_q15 = FLOAT_TO_Q15(mix);
    e->dry_q15 = FLOAT_TO_Q15(1.0f - mix);

    e->write_pos = 0;
}

void Echo_Init(uint32_t delay_ms, float feedback, float mix) {
    echo_init_instance(&echo_L, delay_ms, feedback, mix);
    echo_init_instance(&echo_R, delay_ms, feedback, mix);
}

/**
 * Heißer Pfad – identische Logik für L und R, je auf eigener Instanz.
 *
 * Signal-Flow pro Kanal:
 *   input (int32, ±2^31)
 *   → >> 16 → int16 im Buffer speichern
 *   → delayed int16 lesen → Feedback Q15-Multiplikation
 *   → dry/wet mischen → int32 Ausgang (in-place)
 */
static int32_t echo_process_instance(Echo_t *e, int32_t input) {
    uint32_t read_pos = (e->write_pos - e->delay_samples) & ECHO_BUFFER_MASK;

    int32_t fb = (((int32_t)e->buffer[read_pos] * e->feedback_q15) >> 15) << 16;

    int16_t new_sample = (int16_t)((input >> 16) + (fb >> 16));
    e->buffer[e->write_pos] = new_sample;

    e->write_pos = (e->write_pos + 1) & ECHO_BUFFER_MASK;

    int32_t dry = ((input >> 16) * e->dry_q15) >> 15;
    int32_t wet = ((int32_t)e->buffer[read_pos] * e->wet_q15) >> 15;

    return (dry + wet) << 16;
}

void Echo_ProcessSampleStereo(int32_t *sampleL, int32_t *sampleR) {
    *sampleL = echo_process_instance(&echo_L, *sampleL);
    *sampleR = echo_process_instance(&echo_R, *sampleR);
}

void Echo_SetDelay(uint32_t delay_ms) {
    uint32_t d = (uint32_t)(delay_ms * (ECHO_SAMPLERATE / 1000.0f) + 0.5f);
    if (d >= ECHO_BUFFER_SIZE) d = ECHO_BUFFER_SIZE - 1;
    if (d < 1)                 d = 1;
    echo_L.delay_samples = d;
    echo_R.delay_samples = d;
}

void Echo_SetFeedback(float feedback) {
    if (feedback > 0.9f) feedback = 0.9f;
    if (feedback < 0.0f) feedback = 0.0f;
    echo_L.feedback_q15 = FLOAT_TO_Q15(feedback);
    echo_R.feedback_q15 = FLOAT_TO_Q15(feedback);
}

void Echo_SetMix(float mix) {
    if (mix > 1.0f) mix = 1.0f;
    if (mix < 0.0f) mix = 0.0f;
    echo_L.mix     = mix;
    echo_L.wet_q15 = FLOAT_TO_Q15(mix);
    echo_L.dry_q15 = FLOAT_TO_Q15(1.0f - mix);
    echo_R.mix     = mix;
    echo_R.wet_q15 = FLOAT_TO_Q15(mix);
    echo_R.dry_q15 = FLOAT_TO_Q15(1.0f - mix);
}

void Echo_Clear(void) {
    memset(echo_L.buffer, 0, sizeof(echo_L.buffer));
    echo_L.write_pos = 0;
    memset(echo_R.buffer, 0, sizeof(echo_R.buffer));
    echo_R.write_pos = 0;
}

#ifdef __cplusplus
}
#endif

#endif /* MR_ECHO_H_ */
