/**
 ******************************************************************************
 * @file           : mr_echo.h
 * @brief          : Echo-Effekt für STM32G474 Synthesizer mit CMSIS-DSP
 * @author         : Michael Ruck michael.ruck@marsgasse.com + Claude
 * @date           : 2026-01-13
 ******************************************************************************
 * @details
 * https://claude.ai/chat/bbde1234-263b-4163-8c35-782383b547aa
 *
 * Echo-Effekt optimiert für STM32G474 mit SAI und PCM5102 DAC
 * Samplerate: 44270.83 Hz
 *
 * Nutzt CMSIS-DSP für optimierte Performance auf Cortex-M4F
 *
 * Benötigte STM32CubeIDE Einstellungen:
 * - CMSIS-DSP Library aktiviert
 * - Define ARM_MATH_CM4
 * - FPU: -mfloat-abi=hard -mfpu=fpv4-sp-d16
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
#include "arm_math.h"

/* Defines -------------------------------------------------------------------*/
#define ECHO_BUFFER_SIZE 8192   // ~0.185s bei 44270.83 Hz (muss 2^n sein, 32KB RAM)
#define ECHO_MAX_DELAY_MS 185   // Maximale Verzögerung in ms
#define ECHO_SAMPLERATE 44.27083f  // Hz

// Alternative Buffer-Größen (auskommentiert):
// #define ECHO_BUFFER_SIZE 16384  // ~0.37s (64KB RAM)
// #define ECHO_BUFFER_SIZE 4096   // ~0.092s (16KB RAM)

/* Typedef -------------------------------------------------------------------*/
typedef struct {
    float32_t buffer[ECHO_BUFFER_SIZE];  // Ringbuffer für verzögertes Signal
    uint32_t write_pos;                   // Aktuelle Schreibposition
    uint32_t delay_samples;               // Verzögerung in Samples
    float32_t feedback;                   // 0.0 - 0.9 (Stärke der Wiederholungen)
    float32_t mix;                        // 0.0 - 1.0 (Trocken/Nass-Verhältnis)

    // CMSIS-DSP Skalierungsfaktoren (Optimierung)
    float32_t wet_scale;                  // = mix
    float32_t dry_scale;                  // = 1.0 - mix
} Echo_t;

/* Exported variables --------------------------------------------------------*/
extern Echo_t echo;

/* Function prototypes -------------------------------------------------------*/

/**
 * @brief  Initialisiert den Echo-Effekt
 * @param  delay_ms: Verzögerungszeit in Millisekunden (1 bis ECHO_MAX_DELAY_MS)
 * @param  feedback: Feedback-Stärke (0.0 bis 0.9)
 *                   0.0 = keine Wiederholung
 *                   0.5 = moderate Wiederholungen
 *                   0.9 = sehr lange Wiederholungen
 * @param  mix: Trocken/Nass-Verhältnis (0.0 bis 1.0)
 *              0.0 = nur Originalsignal
 *              0.5 = 50/50 Mix
 *              1.0 = nur Echo-Signal
 * @retval None
 */
void Echo_Init(uint32_t delay_ms, float32_t feedback, float32_t mix);

/**
 * @brief  Verarbeitet Audio-Buffer mit Echo-Effekt (optimiert mit CMSIS-DSP)
 * @param  input: Eingangssignal (int32_t Array)
 * @param  output: Ausgangssignal (int32_t Array)
 * @param  size: Anzahl der zu verarbeitenden Samples
 * @retval None
 * @note   Nutzt CMSIS-DSP für effiziente Verarbeitung größerer Blöcke
 */
void Echo_ProcessBuffer(int32_t *input, int32_t *output, uint32_t size);

/**
 * @brief  Verarbeitet einzelnes Sample mit Echo-Effekt (Inline-Version)
 * @param  input: Eingangssample (int32_t)
 * @retval int32_t: Ausgangssample mit Echo-Effekt
 * @note   Für Verwendung innerhalb von Sample-by-Sample Schleifen
 */
int32_t Echo_ProcessSample(int32_t input);

/**
 * @brief  Setzt die Verzögerungszeit zur Laufzeit
 * @param  delay_ms: Neue Verzögerungszeit in Millisekunden
 * @retval None
 */
void Echo_SetDelay(uint32_t delay_ms);

/**
 * @brief  Setzt das Feedback zur Laufzeit
 * @param  feedback: Neue Feedback-Stärke (0.0 bis 0.9)
 * @retval None
 */
void Echo_SetFeedback(float32_t feedback);

/**
 * @brief  Setzt das Mix-Verhältnis zur Laufzeit
 * @param  mix: Neues Trocken/Nass-Verhältnis (0.0 bis 1.0)
 * @retval None
 */
void Echo_SetMix(float32_t mix);

/**
 * @brief  Löscht den Echo-Buffer (für stumme Pausen)
 * @retval None
 */
void Echo_Clear(void);

/* Implementation ------------------------------------------------------------*/

Echo_t echo;

/**
 * @brief  Initialisierung
 */
void Echo_Init(uint32_t delay_ms, float32_t feedback, float32_t mix) {
    memset(&echo, 0, sizeof(Echo_t));

    // Delay in Samples umrechnen
    echo.delay_samples = (uint32_t)((delay_ms * ECHO_SAMPLERATE) + 0.5f);

    // Begrenzung auf Puffergröße
    if (echo.delay_samples >= ECHO_BUFFER_SIZE) {
        echo.delay_samples = ECHO_BUFFER_SIZE - 1;
    }
    if (echo.delay_samples < 1) {
        echo.delay_samples = 1;
    }

    // Parameter begrenzen
    echo.feedback = (feedback > 0.9f) ? 0.9f : ((feedback < 0.0f) ? 0.0f : feedback);
    echo.mix = (mix > 1.0f) ? 1.0f : ((mix < 0.0f) ? 0.0f : mix);

    echo.wet_scale = echo.mix;
    echo.dry_scale = 1.0f - echo.mix;

    echo.write_pos = 0;
}

/**
 * @brief  Echo-Verarbeitung für Buffer mit CMSIS-DSP
 */
void Echo_ProcessBuffer(int32_t *input, int32_t *output, uint32_t size) {
    static float32_t float_input[512];
    static float32_t float_output[512];
    static float32_t delayed_signal[512];

    uint32_t blocks = (size + 511) / 512;
    uint32_t offset = 0;

    for (uint32_t block = 0; block < blocks; block++) {
        uint32_t block_size = (size - offset > 512) ? 512 : (size - offset);

        // int32 nach float32 konvertieren
        for (uint32_t i = 0; i < block_size; i++) {
            float_input[i] = (float32_t)input[offset + i];
        }

        // Für jedes Sample im Block
        for (uint32_t i = 0; i < block_size; i++) {
            // Leseposition berechnen
            uint32_t read_pos = echo.write_pos;
            if (read_pos >= echo.delay_samples) {
                read_pos -= echo.delay_samples;
            } else {
                read_pos = ECHO_BUFFER_SIZE - (echo.delay_samples - read_pos);
            }

            // Verzögertes Sample lesen
            delayed_signal[i] = echo.buffer[read_pos];

            // Neues Sample mit Feedback berechnen
            float32_t new_sample = float_input[i] + (delayed_signal[i] * echo.feedback);

            // Soft-Clipping
            if (new_sample > 2147483647.0f) new_sample = 2147483647.0f;
            if (new_sample < -2147483648.0f) new_sample = -2147483648.0f;

            // In Buffer schreiben
            echo.buffer[echo.write_pos] = new_sample;

            // Write-Position erhöhen (Ringbuffer)
            echo.write_pos++;
            if (echo.write_pos >= ECHO_BUFFER_SIZE) {
                echo.write_pos = 0;
            }
        }

        // Dry-Signal skalieren (CMSIS-DSP)
        arm_scale_f32(float_input, echo.dry_scale, float_output, block_size);

        // Wet-Signal skalieren und addieren (CMSIS-DSP)
        arm_scale_f32(delayed_signal, echo.wet_scale, delayed_signal, block_size);
        arm_add_f32(float_output, delayed_signal, float_output, block_size);

        // float32 nach int32 konvertieren
        for (uint32_t i = 0; i < block_size; i++) {
            output[offset + i] = (int32_t)float_output[i];
        }

        offset += block_size;
    }
}

/**
 * @brief  Echo-Verarbeitung für einzelnes Sample
 */
int32_t Echo_ProcessSample(int32_t input) {
    // Leseposition berechnen
    uint32_t read_pos = echo.write_pos;
    if (read_pos >= echo.delay_samples) {
        read_pos -= echo.delay_samples;
    } else {
        read_pos = ECHO_BUFFER_SIZE - (echo.delay_samples - read_pos);
    }

    // Verzögertes Sample lesen
    float32_t delayed = echo.buffer[read_pos];
    float32_t in_float = (float32_t)input;

    // Neues Sample mit Feedback
    float32_t new_sample = in_float + (delayed * echo.feedback);

    // Clipping
    if (new_sample > 2147483647.0f) new_sample = 2147483647.0f;
    if (new_sample < -2147483648.0f) new_sample = -2147483648.0f;

    echo.buffer[echo.write_pos] = new_sample;

    // Write-Position erhöhen
    echo.write_pos++;
    if (echo.write_pos >= ECHO_BUFFER_SIZE) {
        echo.write_pos = 0;
    }

    // Mischen: Dry + Wet
    float32_t out_float = (in_float * echo.dry_scale) + (delayed * echo.wet_scale);

    // Clipping und Konvertierung
    if (out_float > 2147483647.0f) out_float = 2147483647.0f;
    if (out_float < -2147483648.0f) out_float = -2147483648.0f;

    return (int32_t)out_float;
}

/**
 * @brief  Parameter zur Laufzeit ändern
 */
void Echo_SetDelay(uint32_t delay_ms) {
    uint32_t new_delay = (uint32_t)((delay_ms * ECHO_SAMPLERATE) + 0.5f);
    if (new_delay >= ECHO_BUFFER_SIZE) {
        new_delay = ECHO_BUFFER_SIZE - 1;
    }
    if (new_delay < 1) {
        new_delay = 1;
    }
    echo.delay_samples = new_delay;
}

void Echo_SetFeedback(float32_t feedback) {
    echo.feedback = (feedback > 0.9f) ? 0.9f : ((feedback < 0.0f) ? 0.0f : feedback);
}

void Echo_SetMix(float32_t mix) {
    echo.mix = (mix > 1.0f) ? 1.0f : ((mix < 0.0f) ? 0.0f : mix);
    echo.wet_scale = echo.mix;
    echo.dry_scale = 1.0f - echo.mix;
}

void Echo_Clear(void) {
    memset(echo.buffer, 0, sizeof(echo.buffer));
    echo.write_pos = 0;
}

/* ============================================================================
   VERWENDUNGSBEISPIEL
   ============================================================================ */

#if 0  // Auskommentiert - nur zur Referenz

/* Deine bestehende Buffer-Konfiguration: */

#define AUDIO_OUT_BUFFER_SIZE 512  // Beispielwert
#define AUDIO_OUT_CHANNELS 2       // Stereo

volatile int32_t audio_out_buffer[AUDIO_OUT_BUFFER_SIZE] = { 0 };
volatile int32_t *audioOutBufPtr = &audio_out_buffer[0];
volatile uint8_t dataReadyFlag = 0;

/* In main() oder Audio_Init(): */
void Audio_Init(void) {
    // Echo initialisieren: 250ms Delay, 0.5 Feedback, 0.4 Mix
    Echo_Init(250, 0.5f, 0.4f);

    // SAI DMA starten
    HAL_SAI_Transmit_DMA(&hsai1, (uint8_t*)audio_out_buffer, AUDIO_OUT_BUFFER_SIZE);
}

/* Deine SAI Callbacks (unverändert): */
void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai) {
    if (hsai->Instance == SAI1_Block_A) {
        audioOutBufPtr = &audio_out_buffer[AUDIO_OUT_BUFFER_SIZE / 2];
        dataReadyFlag = 1;
    }
}

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai) {
    if (hsai->Instance == SAI1_Block_A) {
        audioOutBufPtr = &audio_out_buffer[0];
        dataReadyFlag = 1;
    }
}

/* Erweiterte ProcessAudioData() Funktion mit Echo: */
void ProcessAudioData(void) {
    int32_t out;
    float outf = 0, lfoSample, cutoff_hz_temp, volume_temp;

    // Temporärer Buffer für Echo-Processing
    static int32_t temp_buffer[AUDIO_OUT_BUFFER_SIZE / 2];

    for (int n = 0; n < (AUDIO_OUT_BUFFER_SIZE / AUDIO_OUT_CHANNELS) - 1; n += AUDIO_OUT_CHANNELS) {

        // Deine bestehenden Berechnungen...
        // out = (int32_t) (outf * fToInt32 * volume_temp);

        // Mono-Signal für Echo vorbereiten
        temp_buffer[n / AUDIO_OUT_CHANNELS] = out;
    }

    // Echo-Effekt anwenden (Mono-Processing)
    static int32_t echo_out[AUDIO_OUT_BUFFER_SIZE / (2 * AUDIO_OUT_CHANNELS)];
    Echo_ProcessBuffer(temp_buffer, echo_out, (AUDIO_OUT_BUFFER_SIZE / AUDIO_OUT_CHANNELS) / 2);

    // Echo-Signal auf Stereo verteilen
    for (int n = 0; n < (AUDIO_OUT_BUFFER_SIZE / AUDIO_OUT_CHANNELS) - 1; n += AUDIO_OUT_CHANNELS) {
        int32_t echo_sample = echo_out[n / AUDIO_OUT_CHANNELS];
        audioOutBufPtr[n] = echo_sample;        // Left
        audioOutBufPtr[n + 1] = echo_sample;    // Right
    }

    dataReadyFlag = 0;
}

/* ODER: Sample-by-Sample Processing (einfacher): */
void ProcessAudioData_SampleBySample(void) {
    int32_t out;
    float outf = 0, lfoSample, cutoff_hz_temp, volume_temp;

    for (int n = 0; n < (AUDIO_OUT_BUFFER_SIZE / AUDIO_OUT_CHANNELS) - 1; n += AUDIO_OUT_CHANNELS) {

        // Deine bestehenden Berechnungen...
        out = (int32_t) (outf * fToInt32 * volume_temp);

        // Echo direkt auf Sample anwenden
        out = Echo_ProcessSample(out);

        audioOutBufPtr[n] = out;        // Left
        audioOutBufPtr[n + 1] = out;    // Right
    }

    dataReadyFlag = 0;
}

/* Parameter während der Laufzeit ändern (z.B. über Potis/MIDI): */
void UpdateEchoParameters(void) {
    // Beispiel: Delay über ADC steuern (100-500ms)
    uint32_t delay = 100 + (ADC_Value * 400 / 4095);
    Echo_SetDelay(delay);

    // Feedback über Poti (0.0-0.8)
    float feedback = (float)ADC_Value2 * 0.8f / 4095.0f;
    Echo_SetFeedback(feedback);

    // Mix über Poti (0.0-1.0)
    float mix = (float)ADC_Value3 / 4095.0f;
    Echo_SetMix(mix);
}

#endif  // VERWENDUNGSBEISPIEL

#ifdef __cplusplus
}
#endif

#endif /* MR_ECHO_H_ */
