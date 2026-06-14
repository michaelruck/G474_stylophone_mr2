/*
 * synth_helpers.h
 *
 *  Created on: Sep 1, 2025
 *      Author: michael.ruck@marsgasse.com
 *
 *  2026-03-22: moog_ladder_c() auf State-Struct (moog_ladder_state_t) umgestellt.
 *              Ermöglicht mehrere unabhängige Filter-Instanzen (z.B. Stereo L/R).
 *              fast_tanh Lambda durch static inline Funktion ersetzt –
 *              verhindert Compiler-Probleme bei gleichzeitigen Stereo-Aufrufen.
 */

#ifndef INC_SYNTH_HELPERS_H_
#define INC_SYNTH_HELPERS_H_
#include "arm_math.h"

/**
 * @brief Rundet einen Float-Wert auf eine bestimmte Anzahl von Dezimalstellen
 * @param in        Eingabewert
 * @param decimals  Anzahl der Dezimalstellen
 * @retval Gerundeter Wert
 */
float mrroundf(float in, uint8_t decimals) {
	float factor = powf(10, decimals);
	return roundf(in * factor) / factor;
}

/**
 * @brief Gleitender Mittelwert über MOVINGAVERAGELENGTH Samples
 * @param input  Eingabewert (uint32_t, z.B. ADC-Wert)
 * @retval Gemittelter Ausgabewert
 */
#define MOVINGAVERAGELENGTH 100
uint32_t movingAverage(uint32_t input) {
	static uint32_t samp[MOVINGAVERAGELENGTH] = { 0 };
	static uint32_t pointer = 0;
	uint32_t s = 0;

	samp[pointer] = input;
	for (int i = 0; i < MOVINGAVERAGELENGTH; i++) {
		s = s + samp[i];
	}

	pointer = (pointer + 1) % MOVINGAVERAGELENGTH;
	return s / MOVINGAVERAGELENGTH;
}

/**
 * @brief Wet/Dry Effekt-Mix
 * @param dry     Trockenes Eingangssignal
 * @param effect  Effektsignal (z.B. LFO-moduliert)
 * @param mix     Mischverhältnis 0.0 (trocken) bis 1.0 (nass)
 * @retval Gemischtes Ausgangssignal
 */
float effect_mix(float dry, float effect, float mix) {
	return (dry * effect * mix) + (dry * (1.0 - mix));
}

/**
 * @brief Lineares Mapping eines Integer-Eingangswerts auf einen Float-Ausgangsbereich
 * @param input    Eingabewert
 * @param in_min   Untergrenze Eingang
 * @param in_max   Obergrenze Eingang
 * @param out_min  Untergrenze Ausgang
 * @param out_max  Obergrenze Ausgang
 * @retval Gemappter Float-Wert, auf [out_min, out_max] begrenzt
 */
float mapf(int input, int in_min, int in_max, float out_min, float out_max) {
	if (input < in_min) input = in_min;
	if (input > in_max) input = in_max;
	return (input - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/**
 * @brief Lineares Mapping eines Integer-Eingangswerts auf einen Integer-Ausgangsbereich
 * @param input    Eingabewert
 * @param in_min   Untergrenze Eingang
 * @param in_max   Obergrenze Eingang
 * @param out_min  Untergrenze Ausgang
 * @param out_max  Obergrenze Ausgang
 * @retval Gemappter Integer-Wert, auf [out_min, out_max] begrenzt
 */
int map(int input, int in_min, int in_max, int out_min, int out_max) {
	if (input < in_min) input = in_min;
	if (input > in_max) input = in_max;
	return (input - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/**
 * @brief Exponentielles Mapping eines Integer-Eingangswerts auf einen Float-Ausgangsbereich
 * @param input     Eingabewert
 * @param in_min    Untergrenze Eingang
 * @param in_max    Obergrenze Eingang
 * @param out_min   Untergrenze Ausgang
 * @param out_max   Obergrenze Ausgang
 * @param exponent  Exponent der Kurve (1.0 = linear, 2.0 = quadratisch, etc.)
 * @retval Exponentiell gemappter Float-Wert
 */
float mapf_exponential(int input, int in_min, int in_max, float out_min, float out_max, float exponent = 1.0) {
	float normalized = (float)(input - in_min) / (in_max - in_min);
	float exp_value = powf(normalized, exponent);
	return exp_value * (out_max - out_min) + out_min;
}

/**
 * @brief Soft-Clipping Overdrive mit Tone-Control und Wet/Dry Mix
 * @param dry    Trockenes Eingangssignal
 * @param gain   Verstärkung vor dem Clipping   (1.0 - 20.0)
 * @param tone   Tonblende, 0.0 = dumpf, 1.0 = hell  (0.0 - 1.0)
 * @param level  Ausgangslautstärke             (0.0 - 2.0)
 * @param mix    Wet/Dry Mischverhältnis        (0.0 - 1.0)
 * @retval Verzerrtes Ausgangssignal
 * @note  Verwendet statischen State für Tone-Filter – nicht mehrfach instanziierbar
 */
float overdrive(float dry, float gain, float tone, float level, float mix) {
	if (gain < 0.0f)  gain = 0.0f;
	if (tone < 0.0f)  tone = 0.0f;
	if (tone > 1.0f)  tone = 1.0f;
	if (level < 0.0f) level = 0.0f;
	if (mix < 0.0f)   mix = 0.0f;
	if (mix > 1.0f)   mix = 1.0f;

	float boosted = dry * gain;
	float clipped = tanh(boosted);

	// Einfacher Tiefpass als Tone-Control
	static float prev_sample = 0.0f;
	float filtered = clipped * tone + prev_sample * (1.0f - tone);
	prev_sample = clipped * 0.3f + prev_sample * 0.7f;

	float leveled = filtered * level;
	return dry * (1.0f - mix) + leveled * mix;
}

/**
 * @brief Asymmetrischer Overdrive mit Tube-Charakter
 * @param dry    Trockenes Eingangssignal
 * @param gain   Verstärkung vor dem Clipping   (1.0 - 20.0)
 * @param tone   Tonblende                      (0.0 - 1.0)
 * @param level  Ausgangslautstärke             (0.0 - 2.0)
 * @param mix    Wet/Dry Mischverhältnis        (0.0 - 1.0)
 * @retval Asymmetrisch verzerrtes Ausgangssignal
 * @note  Positive und negative Halbwelle werden unterschiedlich stark geclippt
 *        für röhrenähnlichen Klang. Verwendet statischen State – nicht mehrfach instanziierbar.
 */
float overdrive_asymmetric(float dry, float gain, float tone, float level, float mix) {
	if (gain < 0.0f)  gain = 0.0f;
	if (tone < 0.0f)  tone = 0.0f;
	if (tone > 1.0f)  tone = 1.0f;
	if (level < 0.0f) level = 0.0f;
	if (mix < 0.0f)   mix = 0.0f;
	if (mix > 1.0f)   mix = 1.0f;

	float boosted = dry * gain;

	// Asymmetrisches Soft-Clipping (Tube-Style)
	float clipped;
	if (boosted > 0.0f)
		clipped = tanh(boosted * 0.8f);         // Positive Halbwelle weniger verzerrt
	else
		clipped = tanh(boosted * 1.2f) * 0.9f;  // Negative Halbwelle stärker verzerrt

	// State-Variable Filter als Tone-Control
	static float lp_state = 0.0f;
	static float hp_state = 0.0f;
	float cutoff = tone * 0.5f + 0.1f;
	lp_state += (clipped - lp_state) * cutoff;
	hp_state = clipped - lp_state;
	float filtered = lp_state + hp_state * tone;

	float leveled = filtered * level;
	return dry * (1.0f - mix) + leveled * mix;
}

/**
 * @brief Chorus-Effekt mit LFO-modulierter Verzögerung
 * @param dry    Trockenes Eingangssignal
 * @param lfo    LFO-Wert zur Delay-Modulation (normiert)
 * @param depth  Modulationstiefe              (0.0 - 1.0)
 * @param level  Ausgangspegel des Wet-Signals (0.0 - 1.0)
 * @retval Chorus-bearbeitetes Signal (Dry + Delayed)
 * @note  Interner Delay-Buffer im CCMRAM (~46ms max Delay bei 44270 Hz).
 *        Statischer State – nicht mehrfach instanziierbar.
 */
__attribute__((section(".ccmram"))) static float delay_buffer[2048] = { 0.0f };
static uint16_t write_pos = 0;

float chorus(float dry, float lfo, float depth, float level) {
	depth = fminf(fmaxf(depth, 0.0f), 1.0f);
	level = fminf(fmaxf(level, 0.0f), 1.0f);

	delay_buffer[write_pos] = dry;

	// 15ms Basis-Delay + ±10ms LFO-Modulation
	const float samples_per_ms = 44.27083f;
	float delay_samples = 15.0f * samples_per_ms + lfo * depth * 10.0f * samples_per_ms;

	float read_pos_f = (float)write_pos - delay_samples;
	if (read_pos_f < 0.0f)
		read_pos_f += 2048.0f;

	uint16_t read_pos_int = (uint16_t)read_pos_f;
	float frac = read_pos_f - (float)read_pos_int;
	uint16_t next_pos = (read_pos_int + 1) & 2047;

	// Lineare Interpolation
	float delayed = delay_buffer[read_pos_int] * (1.0f - frac) + delay_buffer[next_pos] * frac;

	write_pos = (write_pos + 1) & 2047;

	return dry + delayed * level;
}


#endif /* INC_SYNTH_HELPERS_H_ */
