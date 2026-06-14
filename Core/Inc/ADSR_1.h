/*
 * ADSR.h
 *
 *  Created on: Aug 25, 2025
 *      Author: michael.ruck@marsgasse.com
 */

#ifndef INC_ADSR_H_
#define INC_ADSR_H_


#include <stdint.h>
#include "main.h"

class ADSR {
public:
    enum State { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };

    ADSR() : state(IDLE), currentLevel(0.0f) {
        setParameters(10, 200, 0.5f, 500);
    }

    void setParameters(uint32_t attackMs, uint32_t decayMs, float sustainLevel, uint32_t releaseMs) {
        this->attackMs = attackMs;
        this->decayMs = decayMs;
        this->sustainLevel = sustainLevel;
        this->releaseMs = releaseMs;
    }

    void trigger() {
        state = ATTACK;
        startTime = HAL_GetTick();
        currentLevel = 0.0f;
    }

    void release() {
        if (state != IDLE && state != RELEASE) {
            releaseStartLevel = currentLevel;
            startTime = HAL_GetTick();
            state = RELEASE;
        }
    }

    void reset() {
        state = IDLE;
        currentLevel = 0.0f;
    }

    float getEnvelope() {
        if (state == IDLE) return 0.0f;

        uint32_t currentTime = HAL_GetTick();
        uint32_t elapsed = currentTime - startTime;

        switch (state) {
            case ATTACK:
                if (attackMs > 0 && elapsed < attackMs) {
                    currentLevel = (float)elapsed / attackMs;
                } else {
                    currentLevel = 1.0f;
                    state = DECAY;
                    startTime = currentTime;
                }
                break;

            case DECAY:
                if (decayMs > 0 && elapsed < decayMs) {
                    float decayProgress = (float)elapsed / decayMs;
                    currentLevel = 1.0f - (1.0f - sustainLevel) * decayProgress;
                } else {
                    currentLevel = sustainLevel;
                    state = SUSTAIN;
                }
                break;

            case SUSTAIN:
                currentLevel = sustainLevel;
                break;

            case RELEASE:
                if (releaseMs > 0 && elapsed < releaseMs) {
                    currentLevel = releaseStartLevel * (1.0f - (float)elapsed / releaseMs);
                } else {
                    currentLevel = 0.0f;
                    state = IDLE;
                }
                break;

            default:
                currentLevel = 0.0f;
                break;
        }

        return currentLevel;
    }

    State getState() const { return state; }
    bool isActive() const { return state != IDLE; }
    float getCurrentLevel() const { return currentLevel; }

private:
    State state;
    uint32_t startTime;
    uint32_t attackMs, decayMs, releaseMs;
    float currentLevel, sustainLevel;
    float releaseStartLevel;
};




#endif /* INC_ADSR_H_ */
