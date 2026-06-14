/*
 * mrDDS_Voice.h
 *
 *  Created on: Feb 14, 2026
 *      Author: michael.ruck@marsgasse.com + Claude
 *
 *  Polyphonic Voice for STM32G474 Stylophone Synthesizer
 *
 *  Architecture:
 *  - 3 Oscillators per Voice (for Unison/Detune/Sub-Octave modes)
 *  - 1 ADSR Envelope per Voice
 *  - 1 LFO per Voice (for PW and Tremolo modulation)
 *
 *  Global (shared across all voices):
 *  - Waveform type
 *  - Voice mode (Unison/Sub-Octave presets)
 *  - Filter (Moog Ladder - too expensive per-voice)
 *  - Effects (Echo, Wavefolder)
 */

#ifndef MRDDS_VOICE_H_
#define MRDDS_VOICE_H_

#include "mrDDS_Oscillator.h"
#include "ADSR.h"
#include "main.h"

class mrDDS_Voice {
public:
    /* ========================================================================
     * Constructor / Destructor
     * ======================================================================== */

    mrDDS_Voice() :
        baseMidiNote(0.0f),
        active(false),
        noteOnTimestamp(0)
    {
    }

    ~mrDDS_Voice() {
    }

    /* ========================================================================
     * Initialization
     * ======================================================================== */

    /**
     * @brief Initialize voice with sample rate
     * @param sampleRate Audio sample rate (e.g. 44270.83 Hz)
     */
    void init(float sampleRate) {
        // Initialize 3 oscillators
        for (int i = 0; i < 3; i++) {
            osc[i].setSampleRate(sampleRate);
            osc[i].setWaveform(mrDDS_Oscillator::SINE);
            osc[i].setPulseWidth(0.5f);
        }

        // Initialize LFO
        lfo.setSampleRate(sampleRate);
        lfo.setWaveform(mrDDS_Oscillator::SINE);
        lfo.setPulseWidth(0.5f);
        lfo.setFrequency(5.0f);  // Default 5 Hz

        // Reset envelope
        env.reset();

        // Mark as inactive
        active = false;
        baseMidiNote = 0.0f;
        noteOnTimestamp = 0;
    }

    /* ========================================================================
     * Note On / Off
     * ======================================================================== */

    /**
     * @brief Trigger note on
     * @param midiNote MIDI note number (float for microtonal support)
     */
    void noteOn(float midiNote) {
        baseMidiNote = midiNote;
        active = true;
        noteOnTimestamp = HAL_GetTick();

        // Trigger envelope
        env.trigger();

        // Reset LFO phase for consistent start
        lfo.resetPhase();

        // Note: Oscillator frequencies will be set externally via setOscillatorFrequencies()
        // This allows flexible voice modes (Unison, Sub-Octave, etc.)
    }

    /**
     * @brief Release note
     */
    void noteOff() {
        env.release();
        // Note: Voice stays active until envelope reaches IDLE state
    }

    /* ========================================================================
     * Global Settings (applied to all voices externally)
     * ======================================================================== */

    /**
     * @brief Set waveform for all 3 oscillators
     * @param wf Waveform type (SINE, TRIANGLE_SAW, SQUARE_PWM, NOISE)
     */
    void setWaveform(mrDDS_Oscillator::WaveformType wf) {
        for (int i = 0; i < 3; i++) {
            osc[i].setWaveform(wf);
        }
    }

    /**
     * @brief Set individual oscillator frequencies
     * @param note0 MIDI note for oscillator 0
     * @param note1 MIDI note for oscillator 1
     * @param note2 MIDI note for oscillator 2
     *
     * Example usage (external, in app.cpp):
     * - MONO mode:         voice.setOscillatorFrequencies(60, 60, 60)
     * - UNISON mode:       voice.setOscillatorFrequencies(59.92, 60, 60.1)
     * - SUB-OCTAVE mode:   voice.setOscillatorFrequencies(48, 60, 60.12)
     */
    void setOscillatorFrequencies(float note0, float note1, float note2) {
        osc[0].setMidiNote(note0);
        osc[1].setMidiNote(note1);
        osc[2].setMidiNote(note2);
    }

    /**
     * @brief Set LFO frequency
     * @param hz Frequency in Hz (e.g. 0.5 - 20 Hz)
     */
    void setLFOFrequency(float hz) {
        lfo.setFrequency(hz);
    }

    /**
     * @brief Set ADSR parameters
     * @param attackMs Attack time in milliseconds
     * @param decayMs Decay time in milliseconds
     * @param sustainLevel Sustain level (0.0 - 1.0)
     * @param releaseMs Release time in milliseconds
     */
    void setADSRParameters(float attackMs, float decayMs, float sustainLevel, float releaseMs) {
        env.setParameters(attackMs, decayMs, sustainLevel, releaseMs);
    }

    /* ========================================================================
     * Audio Processing
     * ======================================================================== */

    /**
     * @brief Process one audio sample
     * @param globalLFO Global LFO sample (-1.0 to +1.0) for vibrato
     * @param pulsewidth Base pulsewidth (0.0 - 1.0)
     * @param lfoDepth LFO modulation depth (0.0 - 1.0)
     * @param lfoTarget LFO target bitmask (bit 0=Volume/Tremolo, bit 1=PW)
     * @param envTarget Envelope target bitmask (bit 0=Amplitude, bit 1=reserved, bit 2=PW)
     * @param numActiveOscs Number of oscillators to mix (1 or 3)
     * @return Audio sample (-1.0 to +1.0)
     */
    float process(float globalLFO, float pulsewidth, float lfoDepth, uint8_t lfoTarget,
                  uint8_t envTarget, uint8_t numActiveOscs) {

        // Update envelope
        env.getEnvelope();
        float envelope = env.getCurrentLevel();

        // Check if voice should be deactivated
        if (env.getState() == ADSR::IDLE) {
            active = false;
            return 0.0f;
        }

        // Get local LFO sample (for PW and Tremolo)
        float lfoSample = (lfo.getNextSample() / 2.0f + 0.5f);  // 0.0 to 1.0

        // Calculate modulated pulsewidth
        float pw_modulated = pulsewidth;

        // LFO -> Pulsewidth
        if (lfoTarget & (1 << 1)) {
            pw_modulated = pulsewidth * (lfoSample * lfoDepth + (1.0f - lfoDepth));
        }

        // Envelope -> Pulsewidth
        if (envTarget & (1 << 2)) {
            pw_modulated = pw_modulated * envelope;
        }

        // Set pulsewidth for oscillators
        for (int i = 0; i < 3; i++) {
            osc[i].setPulseWidth(pw_modulated);
        }

        // Mix oscillators
        float voiceOut = 0.0f;

        if (numActiveOscs == 1) {
            // MONO mode: only osc[1] (center)
            voiceOut = osc[1].getNextSample();
        } else {
            // Unison/Sub-Octave mode: mix all 3
            voiceOut = (osc[0].getNextSample() +
                       osc[1].getNextSample() +
                       osc[2].getNextSample()) / 3.0f;
        }

        // Apply amplitude envelope
        if (envTarget & (1 << 0)) {
            voiceOut *= envelope;
        } else {
            // If envelope doesn't control amplitude, gate the note
            if (env.getState() == ADSR::IDLE || env.getState() == ADSR::RELEASE) {
                voiceOut = 0.0f;
            }
        }

        // Apply tremolo (local LFO -> Volume)
        if (lfoTarget & (1 << 0)) {
            voiceOut *= (lfoSample * lfoDepth + (1.0f - lfoDepth));
        }

        return voiceOut;
    }

    /* ========================================================================
     * Getters
     * ======================================================================== */

    bool isActive() const {
        return active;
    }

    float getMidiNote() const {
        return baseMidiNote;
    }

    uint32_t getNoteOnTime() const {
        return noteOnTimestamp;
    }

    ADSR::State getEnvelopeState() const {
        return env.getState();
    }

private:
    /* ========================================================================
     * Member Variables
     * ======================================================================== */

    mrDDS_Oscillator osc[3];   // 3 oscillators for Unison/Detune/Sub modes
    ADSR env;                   // Envelope generator
    mrDDS_Oscillator lfo;       // Local LFO (PW + Tremolo)

    float baseMidiNote;         // Base MIDI note (float for microtonal)
    bool active;                // Is this voice currently playing?
    uint32_t noteOnTimestamp;   // HAL_GetTick() when note was triggered
};

#endif /* MRDDS_VOICE_H_ */
