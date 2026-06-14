/*
 * mr_moog_ladder.h
 *
 *  Created on: 2026-03-22
 *      Author: michael.ruck@marsgasse.com mit Hilfe von Claude
 *
 *  Klassisches 4-Pol Moog Ladder Tiefpassfilter als C++ Objekt.
 *  State ist in der Instanz gekapselt – mehrere unabhängige Instanzen
 *  (z.B. Stereo L/R) sind problemlos möglich.
 *
 *  Optimierungen:
 *  - CORDIC Hardware-Beschleuniger für tan()-Berechnung in updateCoefficients()
 *    Faktor 5-10x schneller als tanf() aus math.h (ST AN5325)
 *  - LL-Direktzugriff auf CORDIC (kein HAL-Overhead)
 *  - CORDIC liefert cos UND sin gleichzeitig → tan = sin/cos
 *  - Winkel-Eingabe: θ/π als Q1.31 Fixpunkt
 *  - process() enthält nur noch Multiplikationen und Additionen
 *
 *  Voraussetzung: CORDIC muss in CubeMX aktiviert und initialisiert sein (hcordic).
 *  Einmalig in setup() konfigurieren via MrMoogLadder::initCordic().
 *
 *  Verwendung in app.cpp:
 *
 *    MrMoogLadder filterL, filterR;   // global
 *
 *    // Einmalig in setup():
 *    MrMoogLadder::initCordic();
 *
 *    // Einmal pro Buffer, VOR dem Sample-Loop:
 *    filterL.updateCoefficients(cutoff_hz_temp, resonance_temp, sampleRate);
 *    filterR.updateCoefficients(cutoff_hz_temp, resonance_temp, sampleRate);
 *
 *    // Im Sample-Loop:
 *    outfl = filterL.process(outfl, drive);
 *    outfr = filterR.process(outfr, drive);
 *
 *  Typische Parameter-Ranges:
 *    cutoff_hz:  20 - 20000 Hz  (Grenzfrequenz)
 *    resonance:  0.0 - 4.0      (0 = kein Feedback, ~4 = Selbstoszillation)
 *    drive:      1.0 - 10.0     (Input-Sättigung vor dem Filter)
 */

#ifndef MR_MOOG_LADDER_H_
#define MR_MOOG_LADDER_H_

#include "stm32g4xx_ll_cordic.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Q1.31 Konversion
#define FLOAT_TO_Q131(x)  ((int32_t)((x) * 2147483648.0f))
#define Q131_TO_FLOAT(x)  ((float)(x) / 2147483648.0f)

class MrMoogLadder {
public:
	/**
	 * @brief Konstruktor
	 */
	MrMoogLadder(float sampleRate = 44270.83f) :
		_sampleRate(sampleRate),
		_s1(0.0f), _s2(0.0f), _s3(0.0f), _s4(0.0f), _fb(0.0f),
		_g(0.0f), _fb_amt(0.0f), _resonance_comp(1.0f) {
	}

	/**
	 * @brief Samplerate nachträglich setzen (z.B. nach get_actual_SAI_sample_rate())
	 */
	void setSampleRate(float sampleRate) {
		_sampleRate = sampleRate;
	}

	/**
	 * @brief CORDIC einmalig konfigurieren – in setup() aufrufen.
	 *        Konfiguriert CORDIC für Cosine-Funktion (liefert cos + sin):
	 *        - Q1.31 Ein- und Ausgabe
	 *        - 6 Iterationen (Fehler < 2^-19, ausreichend für Audio)
	 *        - 1 Eingabewort (Winkel), 2 Ausgabeworte (cos, sin)
	 */
	static void initCordic() {
		// CORDIC für Cosine konfigurieren (gibt cos UND sin zurück)
		LL_CORDIC_Config(CORDIC,
			LL_CORDIC_FUNCTION_COSINE,   // cos + sin
			LL_CORDIC_PRECISION_6CYCLES, // 6 Iterationen, Fehler < 2^-19
			LL_CORDIC_SCALE_0,            // kein Scaling nötig für |θ/π| < 1
			LL_CORDIC_NBWRITE_1,          // 1 Eingabewort: Winkel
			LL_CORDIC_NBREAD_2,           // 2 Ausgabeworte: cos, sin
			LL_CORDIC_INSIZE_32BITS,      // Q1.31
			LL_CORDIC_OUTSIZE_32BITS);    // Q1.31
	}

	/**
	 * @brief Setzt alle internen State-Variablen auf 0 zurück
	 */
	void reset() {
		_s1 = _s2 = _s3 = _s4 = _fb = 0.0f;
	}

	/**
	 * @brief Berechnet Filterkoeffizienten via CORDIC – einmal pro Buffer aufrufen.
	 *
	 *  tan(π·f/fs) wird berechnet als sin/cos via CORDIC.
	 *  Eingabe: θ/π = f/fs (dimensionslos, Bereich 0..0.5)
	 *  CORDIC liefert cos(π·f/fs) und sin(π·f/fs) in einem Durchgang.
	 *
	 * @param cutoff_hz   Grenzfrequenz in Hz  (20 - 20000)
	 * @param resonance   Resonanz             (0.0 - 4.0)
	 * @param sampleRate  Samplerate in Hz
	 */
	void updateCoefficients(float cutoff_hz, float resonance) {
		// Parameter clipping
		if (cutoff_hz < 20.0f)    cutoff_hz = 20.0f;
		if (cutoff_hz > 20000.0f) cutoff_hz = 20000.0f;
		if (resonance < 0.0f)     resonance = 0.0f;
		if (resonance > 4.0f)     resonance = 4.0f;

		// Winkel für CORDIC: θ/π = f/fs (Bereich 0..0.5 → Q1.31)
		// CORDIC erwartet θ/π im Bereich [-1, 1]
		float angle = cutoff_hz / _sampleRate;
		int32_t angle_q31 = FLOAT_TO_Q131(angle);

		// CORDIC starten – Berechnung läuft parallel während CPU weiterrechnet
		LL_CORDIC_WriteData(CORDIC, angle_q31);

		// Resonanz-Kompensation schon hier berechnen (CPU-Arbeit während CORDIC rechnet)
		_resonance_comp = 1.0f + resonance * 0.3f;

		// CORDIC Ergebnis lesen (wartet automatisch bis fertig)
		float c = Q131_TO_FLOAT((int32_t)LL_CORDIC_ReadData(CORDIC)); // cos
		float s = Q131_TO_FLOAT((int32_t)LL_CORDIC_ReadData(CORDIC)); // sin

		// tan(π·f/fs) = sin/cos → bilineare Transformation
		float w = s / c;
		_g = w / (1.0f + w);

		// Resonanz-Feedback mit Kompensation bei hohen Cutoff-Frequenzen
		_fb_amt = resonance * (1.0f - 0.15f * _g * _g);
	}

	/**
	 * @brief Verarbeitet ein Sample – heißer Pfad, nur Multiplikationen und Additionen.
	 *        updateCoefficients() muss vorher aufgerufen worden sein.
	 *
	 * @param input  Eingangssample (-1.0 bis +1.0)
	 * @param drive  Input-Sättigung (1.0 - 10.0)
	 * @retval Gefiltertes Ausgangssample
	 */
	__attribute__((optimize("O3")))
	float process(float input, float drive) {
		// Input hard-clip
		float sat_in = input * drive;
		if (sat_in >  1.0f) sat_in =  1.0f;
		if (sat_in < -1.0f) sat_in = -1.0f;
		sat_in *= (1.0f / drive);

		// Feedback-Subtraktion
		float stage_in = sat_in - _fb * _fb_amt;

		// 4-stufige Ladder
		_s1 += _g * (stage_in - _s1);
		_s2 += _g * (_s1      - _s2);
		_s3 += _g * (_s2      - _s3);
		_s4 += _g * (_s3      - _s4);

		_fb = _s4;

		return _s4 * _resonance_comp;
	}

private:
	// Konfiguration
	float _sampleRate;

	// Filter-State
	float _s1, _s2, _s3, _s4, _fb;

	// Koeffizienten – gesetzt durch updateCoefficients()
	float _g;               ///< Filterkoeffizient
	float _fb_amt;          ///< Feedback-Betrag
	float _resonance_comp;  ///< Resonanz-Kompensation für Ausgang
};

#endif /* MR_MOOG_LADDER_H_ */
