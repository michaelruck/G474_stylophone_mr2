/*
 * mrDDS_Oscillator.h
 *
 *  Created on: Aug 29, 2025
 *      Author: michael.ruck@marsgasse.com mit Hilfe von claude
 *
 *  2026-03-21: SuperSaw Wellenform hinzugefügt (Adam Szabo Algorithmus)
 *              pulseWidth wird bei SUPER_SAW als Detune verwendet
 *              superSawMix steuert Center vs. Sides Verhältnis
 *  2026-03-22: Stereo SuperSaw via getNextSampleStereo()
 *              Oszillatoren 1,3,5 → links / 2,4,6 → rechts
 */

#ifndef MR_DDS_OSCILLATOR_H
#define MR_DDS_OSCILLATOR_H

#include <stdint.h>
#include <math.h>

// ARM CMSIS DSP für optimierte Berechnungen (falls verfügbar)
#ifdef ARM_MATH_CM4
#include "arm_math.h"
#define FAST_SIN(x) arm_sin_f32(x)
#define FAST_COS(x) arm_cos_f32(x)
#else
#define FAST_SIN(x) sinf(x)
#define FAST_COS(x) cosf(x)
#endif

class mrDDS_Oscillator {
public:
	enum WaveformType {
		SINE = 0, TRIANGLE_SAW, SQUARE_PWM, NOISE, SUPER_SAW
	};

private:
	// System-Parameter
	float sampleRate;
	static constexpr uint32_t PHASE_MAX = 0xFFFFFFFF;
	static constexpr float PHASE_TO_FLOAT = 1.0f / (float) PHASE_MAX;
	static constexpr float PHASE_SCALE = 4294967296.0f;  // 2^32
	static constexpr float TWO_PI = 2.0f * M_PI;
	static constexpr float DEFAULT_SAMPLE_RATE = 43904.0f;

	// Oszillator-State
	uint32_t phaseAccumulator;
	uint32_t tuningWord;
	WaveformType waveform;
	float pulseWidth;           // 0.0 - 1.0 für PWM/Sägezahn-Asymmetrie, bei SUPER_SAW: Detune
	float frequency;

	// Noise-Generator (LFSR)
	uint32_t noiseState;
	float noiseLP;
	float noiseLPCoeff;

	// Bandlimitierung
	float lastSample;

	// --- SuperSaw ---
	// Detune-Koeffizienten nach Adam Szabo (JP-8000 Reverse Engineering)
	// 7 Oszillatoren: Index 0 = Center, 1-6 = Sides (±3 Paare)
	static constexpr float SUPERSAW_DETUNE_COEFF[7] = {
		 0.0f,          // Center (kein Offset)
		-0.11002313f,   // Side 1 links
		-0.06288439f,   // Side 2 links
		-0.03024438f,   // Side 3 links
		 0.03024438f,   // Side 3 rechts
		 0.06288439f,   // Side 2 rechts
		 0.11002313f    // Side 1 rechts
	};

	// Maximale Detune-Spreizung in Cent (bei pulseWidth = 1.0)
	static constexpr float SUPERSAW_MAX_DETUNE_CENTS = 400.0f;

	// SuperSaw: 7 individuelle Phase-Akkumulatoren und Tuning-Words
	uint32_t ssPhaseAcc[7];
	uint32_t ssTuningWord[7];

	// superSawMix: 0.0 = nur Center, 1.0 = nur Sides
	float superSawMix;

public:
	// Konstruktor ohne Parameter
	mrDDS_Oscillator() :
			sampleRate(DEFAULT_SAMPLE_RATE),
			phaseAccumulator(0),
			tuningWord(0),
			waveform(SINE),
			pulseWidth(0.5f),
			frequency(440.0f),
			noiseState(0x12345678),
			noiseLP(0.0f),
			noiseLPCoeff(0.1f),
			lastSample(0.0f),
			superSawMix(0.5f) {
		for (int i = 0; i < 7; i++) {
			ssPhaseAcc[i] = 0;
			ssTuningWord[i] = 0;
		}
		setFrequency(440.0f);
	}

	// Konstruktor mit Samplerate
	mrDDS_Oscillator(float sr) :
			sampleRate(sr),
			phaseAccumulator(0),
			tuningWord(0),
			waveform(SINE),
			pulseWidth(0.5f),
			frequency(440.0f),
			noiseState(0x12345678),
			noiseLP(0.0f),
			noiseLPCoeff(0.1f),
			lastSample(0.0f),
			superSawMix(0.5f) {
		if (sampleRate <= 0.0f)
			sampleRate = DEFAULT_SAMPLE_RATE;
		for (int i = 0; i < 7; i++) {
			ssPhaseAcc[i] = 0;
			ssTuningWord[i] = 0;
		}
		setFrequency(440.0f);
	}

	~mrDDS_Oscillator() {}

	// Sample-Rate nachträglich ändern
	void setSampleRate(float sr) {
		if (sr <= 0.0f)
			return;
		sampleRate = sr;
		setFrequency(frequency);
	}

	// Frequenz-Einstellung
	void setFrequency(float freq) {
		frequency = freq;
		tuningWord = (uint32_t) ((freq * PHASE_SCALE) / sampleRate);

		// Noise Tiefpass-Koeffizient
		float omega = TWO_PI * freq / sampleRate;
		noiseLPCoeff = omega / (1.0f + omega);
		if (noiseLPCoeff > 0.9f)   noiseLPCoeff = 0.9f;
		if (noiseLPCoeff < 0.001f) noiseLPCoeff = 0.001f;

		// SuperSaw Tuning-Words aktualisieren
		updateSuperSawTuning();
	}

	// MIDI-Note
	void setMidiNote(float midiNote) {
		float freq = 440.0f * powf(2.0f, (midiNote - 69.0f) / 12.0f);
		setFrequency(freq);
	}

	void setWaveform(WaveformType wf) {
		waveform = wf;
	}

	// pulseWidth: bei SUPER_SAW = Detune (0.0 = Unisono, 1.0 = maximale Spreizung)
	void setPulseWidth(float pw) {
		pulseWidth = pw;
		if (pulseWidth < 0.001f) pulseWidth = 0.001f;
		if (pulseWidth > 0.999f) pulseWidth = 0.999f;

		if (waveform == SUPER_SAW)
			updateSuperSawTuning();
	}

	// SuperSaw Mix: 0.0 = nur Center, 1.0 = nur Sides
	void setSuperSawMix(float mix) {
		superSawMix = mix;
		if (superSawMix < 0.0f) superSawMix = 0.0f;
		if (superSawMix > 1.0f) superSawMix = 1.0f;
	}

	// Getter
	float getFrequency() const       { return frequency; }
	float getSampleRate() const      { return sampleRate; }
	WaveformType getWaveform() const { return waveform; }
	float getPulseWidth() const      { return pulseWidth; }
	float getSuperSawMix() const     { return superSawMix; }

	// Phase reset (für Sync)
	void resetPhase() {
		phaseAccumulator = 0;
		// SuperSaw: Phasen gleichmäßig verteilen statt alle auf 0
		for (int i = 0; i < 7; i++)
			ssPhaseAcc[i] = (uint32_t)(0xFFFFFFFF / 7 * i);
	}

	// Mono: Nächstes Sample
	float getNextSample() {
		phaseAccumulator += tuningWord;
		float phase = (float) phaseAccumulator * PHASE_TO_FLOAT;

		float sample = 0.0f;

		switch (waveform) {
		case SINE:
			sample = generateSine(phase);
			break;
		case TRIANGLE_SAW:
			sample = generateTriangleSaw(phase);
			break;
		case SQUARE_PWM:
			sample = generateSquarePWM(phase);
			break;
		case NOISE:
			sample = generateNoise();
			break;
		case SUPER_SAW: {
			float l, r;
			generateSuperSawStereo(l, r);
			sample = (l + r) * 0.5f;
			break;
		}
		}

		// Einfache Bandlimitierung (bei hohen Frequenzen)
		if (frequency > sampleRate * 0.25f) {
			sample = (sample + lastSample) * 0.5f;
			lastSample = sample;
		}

		return sample;
	}

	// Stereo: outL und outR separat
	// Bei Nicht-SuperSaw Wellenformen: outL == outR (Mono)
	void getNextSampleStereo(float &outL, float &outR) {
		if (waveform == SUPER_SAW) {
			phaseAccumulator += tuningWord; // Haupt-Akkumulator mitlaufen lassen
			generateSuperSawStereo(outL, outR);
		} else {
			float mono = getNextSample();
			outL = outR = mono;
		}
	}

private:
	// Berechnet die 7 SuperSaw Tuning-Words
	void updateSuperSawTuning() {
		float detuneCents = pulseWidth * SUPERSAW_MAX_DETUNE_CENTS;

		for (int i = 0; i < 7; i++) {
			float offsetCents = detuneCents * SUPERSAW_DETUNE_COEFF[i];
			float detunedFreq = frequency * powf(2.0f, offsetCents / 1200.0f);
			ssTuningWord[i]   = (uint32_t)((detunedFreq * PHASE_SCALE) / sampleRate);
		}
	}

	// Stereo SuperSaw:
	// Center      → gleichgewichtet auf L+R
	// Osc 1,3,5   → links  (ungerade Indizes)
	// Osc 2,4,6   → rechts (gerade Indizes > 0)
	void generateSuperSawStereo(float &outL, float &outR) {
		float centerSample = 0.0f;
		float sidesL = 0.0f;
		float sidesR = 0.0f;

		for (int i = 0; i < 7; i++) {
			ssPhaseAcc[i] += ssTuningWord[i];
			float phase = (float) ssPhaseAcc[i] * PHASE_TO_FLOAT;
			float saw = 2.0f * phase - 1.0f;

			if (i == 0) {
				centerSample = saw;
			} else if (i & 1) {  // 1, 3, 5 → links
				sidesL += saw;
			} else {             // 2, 4, 6 → rechts
				sidesR += saw;
			}
		}

		// Normalisieren (je 3 Oszillatoren pro Seite)
		sidesL *= (1.0f / 3.0f);
		sidesR *= (1.0f / 3.0f);

		// Mix: Center auf beide Kanäle, Sides getrennt
		outL = centerSample * (1.0f - superSawMix) + sidesL * superSawMix;
		outR = centerSample * (1.0f - superSawMix) + sidesR * superSawMix;
	}

	// Sinus
	float generateSine(float phase) {
		return FAST_SIN(phase * TWO_PI);
	}

	// Dreieck/Sägezahn mit Asymmetrie
	float generateTriangleSaw(float phase) {
		if (phase < pulseWidth) {
			return -1.0f + (2.0f * phase / pulseWidth);
		} else {
			return 1.0f - (2.0f * (phase - pulseWidth) / (1.0f - pulseWidth));
		}
	}

	// Rechteck/PWM
	float generateSquarePWM(float phase) {
		return (phase < pulseWidth) ? 1.0f : -1.0f;
	}

	// LFSR Noise mit Tiefpass
	float generateNoise() {
		uint32_t bit = ((noiseState >> 0) ^ (noiseState >> 1) ^ (noiseState >> 21) ^ (noiseState >> 31)) & 1;
		noiseState = (noiseState >> 1) | (bit << 31);

		float rawNoise = ((float)(int32_t) noiseState) / 2147483648.0f;

		noiseLP += (rawNoise - noiseLP) * noiseLPCoeff;

		float compensatedNoise = noiseLP / (noiseLPCoeff * 0.5f + 0.3f);

		if (compensatedNoise >  1.0f) compensatedNoise =  1.0f;
		if (compensatedNoise < -1.0f) compensatedNoise = -1.0f;

		return compensatedNoise;
	}
};

// Konstanten-Definition (außerhalb der Klasse, da constexpr float array)
constexpr float mrDDS_Oscillator::SUPERSAW_DETUNE_COEFF[7];

#endif // MR_DDS_OSCILLATOR_H
