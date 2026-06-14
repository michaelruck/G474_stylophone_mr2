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

/* =========================================================================
 * Moog Ladder Filter
 * =========================================================================
 *
 * Klassisches 4-Pol Tiefpassfilter nach dem Moog-Ladder-Prinzip.
 * Implementierung mit Soft-Clipping (fast_tanh) an den Filterstages
 * und Resonanz-Feedback.
 *
 * Der Filter-State ist in moog_ladder_state_t ausgelagert, damit mehrere
 * unabhängige Instanzen betrieben werden können – insbesondere für
 * Stereo-Verarbeitung (je eine Instanz für L und R).
 *
 * Typische Parameter-Ranges:
 *   cutoff_hz:  20 - 20000 Hz  (Grenzfrequenz)
 *   resonance:  0.0 - 4.0      (0 = kein Feedback, ~4 = Selbstoszillation)
 *   drive:      1.0 - 10.0     (Input-Sättigung vor dem Filter)
 *
 * Verwendung Stereo (in app.cpp):
 *   moog_ladder_state_t moog_L = {0};   // einmalig als globale Variable
 *   moog_ladder_state_t moog_R = {0};
 *
 *   outL = moog_ladder_c(outL, cutoff_hz, resonance, drive, moog_L);
 *   outR = moog_ladder_c(outR, cutoff_hz, resonance, drive, moog_R);
 * ========================================================================= */

/**
 * @brief Schnelle Tanh-Approximation für den Moog Ladder Filter
 *        Padé-Näherung, Fehler < 0.5% für |x| < 3
 *        Als static inline definiert – kein Lambda, kein Stack-Konflikt bei Stereo.
 */
static inline float moog_fast_tanh(float x) {
	if (x < -3.0f) return -1.0f;
	if (x >  3.0f) return  1.0f;
	float x2 = x * x;
	return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/**
 * @brief State-Struct für den Moog Ladder Filter
 *
 * Für jede Filter-Instanz separat anlegen und mit {0} initialisieren.
 */
typedef struct {
	float s1;  ///< Interner State Stage 1
	float s2;  ///< Interner State Stage 2
	float s3;  ///< Interner State Stage 3
	float s4;  ///< Interner State Stage 4 (Filterausgang)
	float fb;  ///< Resonanz-Feedback
} moog_ladder_state_t;

/**
 * @brief Verarbeitet ein Sample durch den Moog Ladder Filter
 * @param input      Eingangssample (-1.0 bis +1.0)
 * @param cutoff_hz  Grenzfrequenz in Hz          (20 - 20000)
 * @param resonance  Resonanz / Feedback-Stärke  (0.0 - 4.0)
 * @param drive      Input-Sättigung             (1.0 - 10.0)
 * @param state      Referenz auf den Filter-State (moog_ladder_state_t)
 * @retval Gefiltertes Ausgangssample
 */
__attribute__((optimize("O1"))) float moog_ladder_c(float input, float cutoff_hz, float resonance, float drive, moog_ladder_state_t &state) {
	// Parameter clipping
	drive     = fmaxf(1.0f,  fminf(drive,     10.0f));
	resonance = fmaxf(0.0f,  fminf(resonance,  4.0f));
	cutoff_hz = fmaxf(20.0f, fminf(cutoff_hz, 22000.0f));

	// Cutoff-Frequenz → Filterkoeffizient g (bilineare Transformation)
	volatile float g = tanf(M_PI * cutoff_hz / 44270.83f);
	g = g / (1.0f + g);

	// Resonanz-Feedback mit Kompensation bei hohen Cutoff-Frequenzen
	volatile float fb_amt = resonance * (1.0f - 0.15f * g * g);

	// Input-Sättigung und Feedback-Subtraktion
	volatile float sat_in   = moog_fast_tanh(input * drive) / drive;
	volatile float stage_in = sat_in - state.fb * fb_amt;

	// 4-stufige Ladder
	state.s1 += g * (moog_fast_tanh(stage_in) - state.s1);
	state.s2 += g * (moog_fast_tanh(state.s1) - state.s2);
	state.s3 += g * (moog_fast_tanh(state.s2) - state.s3);
	state.s4 += g * (moog_fast_tanh(state.s3) - state.s4);

	state.fb = state.s4;

	// Ausgang mit Resonanz-Kompensation (verhindert Lautstärkeeinbruch bei hoher Resonanz)
	return state.s4 * (1.0f + resonance * 0.3f);
}

#endif /* INC_SYNTH_HELPERS_H_ */
