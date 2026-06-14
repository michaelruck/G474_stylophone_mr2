/*
 * mrpianokey.h
 *
 *  Created on: Jan 14, 2026
 *      Author: michael.ruck@marsgasse.com
 */

#ifndef INC_MRPIANOKEY_H_
#define INC_MRPIANOKEY_H_

#include <ADSR.h>
#include <stdint.h>
#include <functional>
#include "main.h"

class mrPianokey {
public:
	mrPianokey(uint8_t midiNote) {
		_midiNote = midiNote;
	}

	~ mrPianokey() {
		;
	}

	float readKey() {
		GPIO_PinState s = HAL_GPIO_ReadPin(_GPIOx, _GPIO_Pin);

		if (_s_old != s && _millis_next < HAL_GetTick()) {
			//Status=CHANGED
			if (s == GPIO_PIN_SET) {
				adsr.trigger(); //TRIGGER
			} else {
				adsr.release(); //RELEASE
			}
			//Set Debounce Timer
			_millis_next = HAL_GetTick() + debounceTime;
			_s_old = s;
		}

		return adsr.getEnvelope();
	}

    // ADSR-Parameter setzen
    void setADSRParameters(float attackMs, float decayMs, float sustainLevel, float releaseMs) {
        adsr.setParameters(attackMs, decayMs, sustainLevel, releaseMs);
    }

private:
	ADSR adsr;
	uint8_t _midiNote;
	GPIO_PinState _s_old = GPIO_PIN_RESET;
	uint32_t _millis_next=0;
}
;




#endif /* INC_MRPIANOKEY_H_ */
