/*
 * app.cpp
 *
 *  Created on: Aug 13, 2025
 *      Author: michael.ruck@marsgasse.com
 *
 *      Main program C++
 *
 *		STM32G474CEUx @ 170MHz
 *
 *      DMA Double Buffer von Phil’s Lab für I2S2 und SAI
 *      STM32 I2S ADC DMA & Double Buffering - Digital Audio Processing with STM32 #4 - Phil's Lab #55
 *      https://www.youtube.com/watch?v=zlGSxZGwj-E
 *
 *
 *
 *      14 ADC mit 1kHz Samplingrate, DMA, TIM15
 *
 *      UART (nicht verwendet)
 *
 *      USB VPC
 *
 *      CMSIS DSP (mit Hilfe von Claude)
 *
 *		Spezielle Funktionen für Synthesizer
 *
 *		Mikrosekunden-Timer 32Bit TIM5 1 MHz
 *
 */

#include "main.h"
#include "app.h"
#include <stdio.h>
//#define USB_PRINTF_REDIRECT_STDOUT
#include "usbprintf.h"
//#include "wavetables_uint12.h"
#include "synth_helpers.h"
#include "mrDDS_Oscillator.h"
#include "ADSR.h"
//#include "wavetables_float.h"
//#include "wavetables_int32.h"
#include "mr32toBits.h"
#include "mr_sai_samplerate.h"
//#include "mr_echo.h"
#include "mr_echo_int16.h"
#include "mr_wavefolder.h"
#include "MR_Neopixel.h"
#include "mrButtonHW.h"
#include "mrDDS_Voice.h"
#include "mr_moog_ladder.h"

/*
 * #include <Envelope.h>

 #include "mrPianoForteKey.h"
 #include "arm_math.h"
 #include "mrDigitalkey.h"
 */
extern "C" {
/* Extern Handlers */

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;
extern CORDIC_HandleTypeDef hcordic;
//extern DAC_HandleTypeDef hdac3;
//extern TIM_HandleTypeDef htim6;
extern FMAC_HandleTypeDef hfmac;
//extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim15;
//extern DAC_HandleTypeDef hdac1;
extern TIM_HandleTypeDef htim5;
extern SAI_HandleTypeDef hsai_BlockA1;
extern SPI_HandleTypeDef hspi3;
extern TIM_HandleTypeDef htim3;
extern RTC_HandleTypeDef hrtc;
extern UART_HandleTypeDef huart3;

/* Private Variables */
uint32_t adc1_buffer[6] = { 0 };
uint32_t adc2_buffer[6] = { 0 };
uint32_t adc3_buffer[2] = { 0 };

int ConvCplt1_Flag = 0;
int ConvCplt2_Flag = 0;
int ConvCplt3_Flag = 0;
int ConvCpltSPI3_Flag = 0;

//Welche WS-LED ist was?
#define WSLED_PW 2
#define WSLED_CUTOFF 1
#define WSLED_VOLUME 0
color_type ws_col_lfo = { 50, 0, 0, 0 };
color_type ws_col_env = { 0, 0, 50, 0 };
color_type ws_col_both = { 25, 0, 25, 0 };

//Welcher adc buffer ist was?
//Zeile 1 von links nach rechts
#define BUF_PULSEWIDTH		(adc1_buffer[0])		//PA0
#define BUF_ENV_A 			(adc1_buffer[1])		//PA1
#define BUF_ENV_D 			(adc1_buffer[2])		//PA2
#define BUF_LFO_HZ 			(adc1_buffer[3])		//PA3
#define BUF_LP_CUTOFF_HZ	(adc1_buffer[4])		//PB14
#define BUF_ECHO_MIX  		(adc1_buffer[5])		//PB12

//Zeile 2 von links nach rechts
#define BUF_WAVEFOLDER   	(adc2_buffer[0])		//PA6
#define BUF_ENV_S  			(adc2_buffer[1])		//PA7
#define BUF_ENV_R			(adc2_buffer[2])		//PC4
#define BUF_LFO_DEPTH 		(adc2_buffer[3])		//PB2
#define BUF_LP_RESONANCE	(adc2_buffer[4])		//PA5
#define BUF_ECHO_FEEDBACK  	(adc2_buffer[5])		//PB15 BUF_OVERDRIVE_MIX

//Ausserhalb der Reihe
//#define BUF_STYLUS 			(adc3_buffer[0])		//PB13
#define BUF_BATTERIE		(adc3_buffer[0])		//PB13
#define BUF_VOLUME 			(adc3_buffer[1])		//PB5

#define BUT_WAVEFORM	0
#define BUT_SUBOCTAVE	1
#define BUT_ENVTARGET	2
#define BUT_LFOTARGET	3
#define BUT_OCTMINUS	4
#define BUT_OCTPLUS		5

#define DEEPEST_NOTE	9 //G=9, C=12//midi note

int32_t fToInt32;
int32_t fToInt16;
int32_t fToInt12;

#define AUDIO_OUT_CHANNELS 2
#define AUDIO_OUT_CHANNEL_BUFFER_SIZE 512	//durch 2 teilbar, wegen double-buffer
#define AUDIO_OUT_BUFFER_SIZE (AUDIO_OUT_CHANNELS*AUDIO_OUT_CHANNEL_BUFFER_SIZE)

/* Targets */
#define LFO_TARGET_VOLUME 		(lfo_target & (1 << 0))
#define LFO_TARGET_CUTOFF 		(lfo_target & (1 << 1))
#define LFO_TARGET_PULSEWIDTH 	(lfo_target & (1 << 2))

#define ENVELOPE_TARGET_VOLUME		(envelope_target & (1 << 0))
#define ENVELOPE_TARGET_CUTOFF		(envelope_target & (1 << 1))
#define ENVELOPE_TARGET_PULSEWIDTH	(envelope_target & (1 << 2))

//int32_t doubleWave[2 * WAVETABLE_SIZE];

//#define CLAVIATUR_RAW adc3_buffer[0]

// für DAC
volatile int32_t audio_out_buffer[AUDIO_OUT_BUFFER_SIZE] = { 0 };
volatile int32_t *audioOutBufPtr = &audio_out_buffer[0];
//uint32_t accu = 0;

uint8_t dataReadyFlag;

//#define micros() (__HAL_TIM_GetCounter(&htim5))

uint32_t tw = 1, kpress;
float Samplefrequenz = 44270.832; //44270
float volume = 0.0;
float cutoff_hz = 9000, resonance = 0.5, cutoff_min = 200, cutoff_max = 5000;
float overdrive_mix = 0.0;
float lfo_depth = 0.0;
uint8_t lfo_target = 0, suboctave = 2, envelope_target = 1;
float pulsewidth = 0.5;

float basenote = 69.0;

uint8_t shift[4] = { 0 };
uint32_t shift32 = 0;

uint8_t button[6] = { 1, 1, 1, 1, 1, 1 };
int8_t octave = 4; //Beginnt bei midi 60

mr_wavefolder_t wavefolder;

/* Function Prototypes */
void ProcessAudioData(void);

/* Objects */
mrDDS_Oscillator osc[3], lfo;
mrDDS_Oscillator::WaveformType waveform;
uint32_t tempWaveform;
ADSR env;
mrButtonHW btnShutdown(SOFT_POWER_GPIO_Port, SOFT_POWER_Pin);

mrDDS_Voice voice[3];

MrMoogLadder filterL, filterR;


/* Functions */
void shutdown() {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	/* Do something before cutting power */
	usb_printf("Shutting Down...\r\n");
	//saveStatus();
	HAL_Delay(100);

	/* Cutoff Power */
	// Pin-Nummer und Port definieren (z.B. PA5)
	GPIO_InitStruct.Pin = SOFT_POWER_Pin;

	//GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD; // Behält den Modus bei oder setzt ihn neu
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT; // Behält den Modus bei oder setzt ihn neu

	//GPIO_InitStruct.Pull = GPIO_NOPULL;     // Hier den Pull-up aktivieren oder deaktivieren
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;     // Hier den Pull-up aktivieren oder deaktivieren

	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

	HAL_GPIO_Init(SOFT_POWER_GPIO_Port, &GPIO_InitStruct);
	//HAL_GPIO_WritePin(SOFT_POWER_GPIO_Port, SOFT_POWER_Pin, GPIO_PIN_RESET);
}

/* https://chat.deepseek.com/a/chat/s/e5ef4dc3-be49-4ebd-accf-43c4a10a6476 */
#define BACKUP_MAGIC_NUMBER  0x55AA1234
void saveStatus() {
	//Vor dem Abschalten speichern
	HAL_RTCEx_BKUPWrite(&hrtc, 31, BACKUP_MAGIC_NUMBER); //test ob es funktioniert

}

void restoreStatus() {
	octave = HAL_RTCEx_BKUPRead(&hrtc, BUT_OCTMINUS);
	//usb_printf("restoreStatus:\r\n");
	//usb_printf("octave=%u\r\n", octave);
	tempWaveform = HAL_RTCEx_BKUPRead(&hrtc, BUT_WAVEFORM);
	//usb_printf("waveform=%u\r\n", tempWaveform);
	waveform = static_cast<mrDDS_Oscillator::WaveformType>(tempWaveform);
	suboctave = HAL_RTCEx_BKUPRead(&hrtc, BUT_SUBOCTAVE);
	envelope_target = HAL_RTCEx_BKUPRead(&hrtc, BUT_ENVTARGET);
	lfo_target = HAL_RTCEx_BKUPRead(&hrtc, BUT_LFOTARGET);

}

#ifndef WAVETABLE_SIZE
#define WAVETABLE_SIZE 2048
#define WAVETABLE_BITSIZE 11
#endif
#define TWFAKTOR 2097152 //(pow(2,(32-WAVETABLE_BITSIZE)))
uint32_t pitchToTuningword(float pitch) {
	float f0 = Samplefrequenz / WAVETABLE_SIZE;
	uint32_t tw = lroundf(TWFAKTOR * pitch / f0);

	return tw;
}

float midiToPitch(float midi) {
	// MIDI Note 69 = A4 = 440 Hz
	// Formel: f = 440 * 2^((midi - 69) / 12)
	return 440.0f * powf(2.0f, (midi - 69.0f) / 12.0f);
}

void ws_test() {
	for (uint8_t lednr = 0; lednr < WS_STRIP_LENGTH; lednr++) {
		ws_clear();
		ws_set_led(lednr, ws_col_magenta);
		ws_send();
		HAL_Delay(100);
	}
	ws_clear();
	ws_send();
}

uint8_t brightness = 255;
void setWSLeds() {
	//static uint8_t envelope_target_old = 0, lfo_target_old = 0;

//envelope_target
//lfo_target
	static color_type col_pw, col_vol, col_cutoff;

	/*if (envelope_target_old != envelope_target) {
	 envelope_target_old = envelope_target;
	 */
	//ws_clear();
	col_pw.g = 0;
	col_vol.g = 0;
	col_cutoff.g = 0;

	//Volume
	if (ENVELOPE_TARGET_VOLUME)
		col_vol.b = brightness;
	else
		col_vol.b = 0;

	//Cut off
	if (ENVELOPE_TARGET_CUTOFF)
		col_cutoff.b = brightness;
	else
		col_cutoff.b = 0;

	//Pulsewidth
	if (ENVELOPE_TARGET_PULSEWIDTH)
		col_pw.b = brightness;
	else
		col_pw.b = 0;

	ws_set_led(WSLED_CUTOFF, col_cutoff);
	ws_set_led(WSLED_PW, col_pw);
	ws_set_led(WSLED_VOLUME, col_vol);
	ws_send();
	//}

	/*if (lfo_target_old != lfo_target) {
	 lfo_target_old = lfo_target;
	 */
	col_pw.g = 0;
	col_vol.g = 0;
	col_cutoff.g = 0;

	//Volume
	if (LFO_TARGET_VOLUME)
		col_vol.r = brightness;
	else
		col_vol.r = 0;

	//Cut off
	if ( LFO_TARGET_CUTOFF)
		col_cutoff.r = brightness;
	else
		col_cutoff.r = 0;

	//Pulsewidth
	if (LFO_TARGET_PULSEWIDTH)
		col_pw.r = brightness;
	else
		col_pw.r = 0;

	ws_set_led(WSLED_CUTOFF, col_cutoff);
	ws_set_led(WSLED_PW, col_pw);
	ws_set_led(WSLED_VOLUME, col_vol);
	ws_send();

	//}
}

void readButtons() {
	//static uint8_t button_old[4];
	button[0] = HAL_GPIO_ReadPin(PUSH1_GPIO_Port, PUSH1_Pin);
	button[1] = HAL_GPIO_ReadPin(PUSH2_GPIO_Port, PUSH2_Pin);
	button[2] = HAL_GPIO_ReadPin(PUSH3_GPIO_Port, PUSH3_Pin);
	button[3] = HAL_GPIO_ReadPin(PUSH4_GPIO_Port, PUSH4_Pin);
	button[4] = HAL_GPIO_ReadPin(PUSH5_GPIO_Port, PUSH5_Pin);
	button[5] = HAL_GPIO_ReadPin(PUSH6_GPIO_Port, PUSH6_Pin);
	/*
	 for(int i=0; i<4;i++){
	 if(button_old[i] != button[i]){
	 usb_printf("button %i=%u\r\n", i,button[i]);

	 button_old[i] = button[i];
	 }
	 }*/

}

void readShiftRegister() {
	HAL_GPIO_WritePin(LATCH_GPIO_Port, LATCH_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(LATCH_GPIO_Port, LATCH_Pin, GPIO_PIN_SET);
	HAL_SPI_Receive_DMA(&hspi3, (uint8_t*) &shift32, 4);
	//usb_printf("%lu\r\n", shift32);
}

void setParameters() {
	//int waveform;
	//static int waveform_old=5;

	static float pulsewidth_old = 2.0;

	//uint32_t temp;
	//volume = (float) BUF_VOLUME / 4095;
	volume = mrroundf(mapf_exponential(BUF_VOLUME, 50, 4000, 0.0, 1.0, 2.0), 2);

	//Envelope
	env.setParameters((float) BUF_ENV_A / 16, (float) BUF_ENV_D / 8, (float) BUF_ENV_S / 4095, (float) BUF_ENV_R / 4);
	//usb_printf("A:%lu, D:%lu, S:%lu, R:%lu\r\n",BUF_ENV_A, BUF_ENV_D, BUF_ENV_S, BUF_ENV_R);

	//LP Filter
	cutoff_hz = mapf_exponential(BUF_LP_CUTOFF_HZ, 20, 4000, 200, 20000, 2);
	resonance = mapf(BUF_LP_RESONANCE, 50, 4000, 0.0, 4.0);
	//LP_drive = mapf(BUF_LP_DRIVE, 50, 4000, 1.0, 10.0);

	//LFO
	lfo.setFrequency(mapf_exponential(BUF_LFO_HZ, 0, 4000, 0.05, 60.0, 2));
	lfo_depth = mapf(BUF_LFO_DEPTH, 50, 4000, 0.0, 1.0);

	//Pulswidth
	pulsewidth = mrroundf(mapf(BUF_PULSEWIDTH, 50, 4000, 0.01, 0.65), 2);
	if (pulsewidth_old != pulsewidth) {
		pulsewidth_old = pulsewidth;
		//for (int i = 0; i < 3; i++) osc[i].setPulseWidth(pulsewidth);
	}

	//Echo
	Echo_SetMix(mapf(BUF_ECHO_MIX, 40, 4000, 0.0, 0.5)); //Trocken/Nass-Verhältnis (0.0 bis 1.0)
	Echo_SetFeedback(mapf(BUF_ECHO_FEEDBACK, 40, 4000, 0.0, 0.8)); //Feedback-Stärke (0.0 bis 0.9)

	//Wavefolder
	float macro = (mapf(BUF_WAVEFOLDER, 40, 4000, 0.0, 1.0));   // 0.0 .. 1.0

	/* Drive: exponentiell */
	wavefolder.drive = 1.0f + 6.0f * macro * macro;

	/* Folds: erst ab ca. 40% */
	if (macro < 0.4f)
		wavefolder.folds = 1;
	else if (macro < 0.6f)
		wavefolder.folds = 2;
	else if (macro < 0.75f)
		wavefolder.folds = 3;
	else
		wavefolder.folds = 4;

	/* for SuperSaw */
	if (waveform == mrDDS_Oscillator::SUPER_SAW) {
		float detune = mapf(BUF_PULSEWIDTH, 40, 4000, 0.0, 1.0);
		float mix = mapf(BUF_WAVEFOLDER, 40, 4000, 0.0, 1.0);
		for (int i = 0; i < 3; i++) {
			osc[i].setPulseWidth(detune);
			osc[i].setSuperSawMix(mix);
		}
	}
	//Distortion
	//overdrive_mix = mapf(BUF_OVERDRIVE_MIX, 0, 4000, 0.0, 1.0);

}

/* Wrapper 1 */
void setup(void) {
// This runs one time
	btnShutdown.setDoubleClickCallback(shutdown);

	fToInt32 = pow(2, 31) - 1;
	fToInt16 = pow(2, 15) - 1;
	fToInt12 = pow(2, 11) - 1;

	usb_printf_init(false, false);

	HAL_TIM_Base_Start(&htim5);
	HAL_TIM_Base_Start(&htim15);
	/* 1 kHz ADC*/
	//TIM15->ARR = (170000 / 1) - 1;
	//HAL_TIM_Base_Start(&htim6);
	/* 40 kHz DAC*/
	//TIM6->ARR = (170000000 / Samplefrequenz) - 1;
	HAL_ADC_Start_DMA(&hadc1, adc1_buffer, 6);
	HAL_ADC_Start_DMA(&hadc2, adc2_buffer, 6);
	HAL_ADC_Start_DMA(&hadc3, adc3_buffer, 2);

	HAL_GPIO_WritePin(LATCH_GPIO_Port, LATCH_Pin, GPIO_PIN_SET);
	//HAL_DAC_Start_DMA(&hdac1, DAC1_CHANNEL_1, (uint32_t*) audio_out_buffer, AUDIO_OUT_BUFFER_SIZE, DAC_ALIGN_12B_R);
	//HAL_DAC_Start_DMA(&hdac1, DAC1_CHANNEL_1, (uint32_t*) sinus_u12, WAVETABLE_SIZE, DAC_ALIGN_12B_R);

	ws_init(&htim3, TIM_CHANNEL_4);
	ws_clear();
	ws_send();
	//ws_test();

	Echo_Init(ECHO_MAX_DELAY_MS, 0.5f, 0.4f);

	mr_wavefolder_init(&wavefolder);
	wavefolder.drive = 3.5f;
	wavefolder.folds = 4;
	wavefolder.offset = 0.0f;

	filterL.setSampleRate(Samplefrequenz);
	filterR.setSampleRate(Samplefrequenz);
	MrMoogLadder::initCordic();

	HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t*) audio_out_buffer, AUDIO_OUT_BUFFER_SIZE); //PCM5102
	//HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t*) doubleWave, 2*WAVETABLE_SIZE); //PCM5102 funktioniert
	Samplefrequenz = get_actual_SAI_sample_rate(&hsai_BlockA1);

	lfo.setSampleRate(Samplefrequenz);
	//lfo.setWaveform(mrDDS_Oscillator::TRIANGLE_SAW);
	lfo.setWaveform(mrDDS_Oscillator::SINE);
	lfo.setPulseWidth(0.5);
	lfo.setFrequency(10.0);

	/* RTC-Backup System aktivieren*/
	//__HAL_RCC_PWR_CLK_ENABLE();
	HAL_PWR_EnableBkUpAccess();
	//__HAL_RCC_BACKUPRESET_FORCE();
	//__HAL_RCC_BACKUPRESET_RELEASE();

	usb_printf("\r\n** Firmware: G474_stylophone_mr2 **\r\nSysClockFreq=%lu\r\nfs=%f\r\n", HAL_RCC_GetSysClockFreq(), Samplefrequenz);
	restoreStatus();
	/*******************************/
	for (int i = 0; i < 3; i++) {
		osc[i].setSampleRate(Samplefrequenz);
		osc[i].setWaveform(waveform);
		osc[i].setPulseWidth(0.5);
		osc[i].setMidiNote(basenote - 12 * i);
	}

	setParameters();
	readButtons();
	readShiftRegister();

}

/* Wrapper 2 */
void loop(void) {
// This runs forever
	static uint32_t next_milli = 0;
	static uint32_t next_LED = 0;
	static uint32_t next_SPI = 0;
	//static uint32_t next_debounce = 0;

	//static uint32_t stylus_old = 0;
	//static uint32_t stylus, stylusraw, stylusraw_old, stylus_count = 0;
	//static uint8_t shift_old[4]={0};
	static uint32_t shift32_old = 0;
	static uint8_t button_old[6];
	//static uint8_t lednr=0;

	btnShutdown.update();

	if (ConvCplt1_Flag) {
		ConvCplt1_Flag = 0;
	}

	if (ConvCplt2_Flag) {
		ConvCplt2_Flag = 0;
	}

	if (ConvCplt3_Flag) {
		ConvCplt3_Flag = 0;
	}

	if (ConvCpltSPI3_Flag) {
		//readShiftRegister();
		ConvCpltSPI3_Flag = 0;
	}

	if (next_SPI < HAL_GetTick()) {
		next_SPI = HAL_GetTick() + 5;
		//readShiftRegister();
	}

	//LED Blinky
	if (next_LED < HAL_GetTick()) {
		next_LED = HAL_GetTick() + 250;
		/*
		 ws_clear();
		 ws_set_led(lednr, ws_col_magenta);
		 ws_send();

		 lednr=(lednr+1)%WS_STRIP_LENGTH;
		 */
	}
	/* Bedienpanel / Parameter Änderungen / Anschläge */
	if (next_milli <= HAL_GetTick()) {
		next_milli = HAL_GetTick() + 15;
		HAL_GPIO_TogglePin(LED_BUILTIN_GPIO_Port, LED_BUILTIN_Pin);

		// Note
		readShiftRegister();

		readButtons();

		for (int i = 0; i < 6; i++) {
			if (button_old[i] != button[i]) {
				button_old[i] = button[i];
				//usb_printf("button %i=%u\r\n", i, button[i]);

				if (button[BUT_OCTMINUS] == 0) {
					//Oktave runter
					if (octave > 0)
						octave--;
					usb_printf("Octave=%i\r\n\r\n", octave);
					HAL_RTCEx_BKUPWrite(&hrtc, BUT_OCTMINUS, octave);
				}

				if (button[BUT_OCTPLUS] == 0) {
					//Oktave rauf
					if (octave < 7)
						octave++;
					usb_printf("Octave=%i\r\n\r\n", octave);
					HAL_RTCEx_BKUPWrite(&hrtc, BUT_OCTMINUS, octave);
				}

				if (button[BUT_WAVEFORM] == 0) {
					//Waveform im Kreis
					//von Sinus=0 bis Rechteck=2
					tempWaveform = (tempWaveform + 1) % 4;
					waveform = static_cast<mrDDS_Oscillator::WaveformType>(tempWaveform + 1);
					HAL_RTCEx_BKUPWrite(&hrtc, BUT_WAVEFORM, waveform);

					for (int i = 0; i < 3; i++)
						osc[i].setWaveform(waveform);

					switch ((int) waveform) {
					case 0:
						usb_printf("Waveform: Sinus\r\n");
						break;
					case 1:
						usb_printf("Waveform: Triangle / Sawtooth\r\n");
						break;
					case 2:
						usb_printf("Waveform: Squarewave\r\n");
						break;
					case 3:
						usb_printf("Waveform: Noise\r\n");
						break;
					case 4:
						usb_printf("Waveform: SuperSaw\r\n");
						break;
					}
					usb_printf("\r\n");
				}

				if (button[BUT_LFOTARGET] == 0) {
					//LFO Target: Filter Cutoff, PW, Vibrato
					lfo_target = (lfo_target + 1) % 8;
					//std::string strtemp = mr32toBits((uint8_t) (lfo_target), 0).substr(5, 3);
					//usb_printf("LFO Target: %s\r\n", strtemp.c_str());
					HAL_RTCEx_BKUPWrite(&hrtc, BUT_LFOTARGET, lfo_target);

					if (LFO_TARGET_CUTOFF)
						usb_printf("LFO Filter Cutoff ON\r\n");
					else
						usb_printf("LFO Filter Cutoff OFF\r\n");

					if (LFO_TARGET_PULSEWIDTH)
						usb_printf("LFO Osc Pulsewidth ON\r\n");
					else
						usb_printf("LFO Osc Pulsewidth OFF\r\n");

					if (LFO_TARGET_VOLUME)
						usb_printf("LFO Vibrato ON\r\n");
					else
						usb_printf("LFO Vibrato OFF\r\n");

					usb_printf("\r\n");

				}

				if (button[BUT_SUBOCTAVE] == 0) {
					//Add Sub Octaves
					suboctave = (suboctave + 1) % 6;
					usb_printf("Suboctave Mode: %u\r\n\r\n", suboctave);
					HAL_RTCEx_BKUPWrite(&hrtc, BUT_SUBOCTAVE, suboctave);
				}

				if (button[BUT_ENVTARGET] == 0) {
					//Envelope Target: Note Volume, Filter Cutoff, Filter Resonance
					envelope_target = (envelope_target + 1) % 8;
					//std::string strtemp = mr32toBits((uint8_t) (envelope_target), 0).substr(6, 2);
					//usb_printf("Envelope Target: %s\r\n", strtemp.c_str());
					HAL_RTCEx_BKUPWrite(&hrtc, BUT_ENVTARGET, envelope_target);

					if (ENVELOPE_TARGET_VOLUME)
						usb_printf("Envelope Volume ON\r\n");
					else
						usb_printf("Envelope Volume OFF\r\n");

					if (ENVELOPE_TARGET_CUTOFF)
						usb_printf("Envelope Filter Cutoff ON\r\n");
					else
						usb_printf("Envelope Filter Cutoff OFF\r\n");

					if (ENVELOPE_TARGET_PULSEWIDTH)
						usb_printf("Envelope Pulsewidth ON\r\n");
					else
						usb_printf("Envelope Pulsewidth OFF\r\n");
					usb_printf("\r\n");
				}
			}

			// Stylus *************************************************
			// Entprellen: Noten Start soll zeitig erfolgen, aber nur ein mal. Noten  Stop verzögert im ms Bereich.
			if (shift32_old != shift32) {
				/* (__RBIT(shift32))>>8 */
				//usb_printf("%s\r\n", mr32toBits(shift32, 4).c_str());
				for (int k = 0; k < 24; k++) {
					if ((shift32 & (1 << k)) < (shift32_old & (1 << k))) {
						/* Note Stop */
						env.release();
						//usb_printf("- Stop %i\r\n", 23-k);
					}
				}

				for (int k = 0; k < 24; k++) {
					//gehe durch alle Bits und vergleiche mit old einzeln, dann mach notenstart oder notenstop
					//temp=__RBIT(shift32);
					if ((shift32 & (1 << k)) > (shift32_old & (1 << k))) {
						/* Note Start */
						basenote = (23 - k)/*reverse register*/+ (octave * 12)/**/+ DEEPEST_NOTE;

						//Suboctave Noten Einstellung
						switch (suboctave) {
						case 0: //OFF
							osc[0].setMidiNote(basenote);
							osc[1].setMidiNote(basenote);
							osc[2].setMidiNote(basenote);
							break;
						case 1: //FAT (Unison leicht)
							osc[0].setMidiNote(basenote - 0.08);
							osc[1].setMidiNote(basenote);
							osc[2].setMidiNote(basenote + 0.1);
							break;
						case 2: //SUPER FAT (Unison stark)
							osc[0].setMidiNote(basenote - 0.15);
							osc[1].setMidiNote(basenote);
							osc[2].setMidiNote(basenote + 0.18);
							break;
						case 3: //BASS (Oktave + Detune)
							osc[0].setMidiNote(basenote - 12);
							osc[1].setMidiNote(basenote);
							osc[2].setMidiNote(basenote + 0.12);
							break;
						case 4: //FIFTH (Quinte)
							osc[0].setMidiNote(basenote - 12);
							osc[1].setMidiNote(basenote);
							osc[2].setMidiNote(basenote + 7);
							break;
						case 5: //FIFTH (OCTAVE)
							osc[0].setMidiNote(basenote - 12);
							osc[1].setMidiNote(basenote);
							osc[2].setMidiNote(basenote + 12);
							break;
						default:
							break;
						}
						/*
						 for (int i = 0; i < 3; i++) {
						 osc[i].setMidiNote(basenote - i * 12);
						 //osc[i].resetPhase();
						 }
						 */
						env.trigger();
						lfo.resetPhase();
						//usb_printf("midi: %u\r\n", (uint8_t) basenote);
						//usb_printf("+Start %i\r\n", 23-k);
					}
				}

				shift32_old = shift32;
			}
		}

		setParameters();
		setWSLeds();

		/*
		 if (stylus_old != stylus) {
		 //usb_printf("BUF_STYLUS:%lu, stylus:%lu\r\n", BUF_STYLUS, stylus);
		 stylus_old = stylus;
		 if (stylus < 25) {
		 kpress = 1;
		 env.trigger();
		 osc[0].setMidiNote(stylus + 47 + 12);
		 tw = pitchToTuningword(midiToPitch(stylus + 47 + 12));
		 HAL_GPIO_WritePin(LED_BUILTIN_GPIO_Port, LED_BUILTIN_Pin, GPIO_PIN_SET);
		 } else {
		 kpress = 0;
		 env.release();
		 HAL_GPIO_WritePin(LED_BUILTIN_GPIO_Port, LED_BUILTIN_Pin, GPIO_PIN_RESET);
		 }

		 }
		 */
	}

	if (dataReadyFlag)
		ProcessAudioData();

}

// printf to UART
/*
 int _write(int file, char *ptr, int len) {
 HAL_StatusTypeDef hstatus;

 if (file == 1 || file == 2) {
 hstatus = HAL_UART_Transmit(&huart1, (uint8_t*) ptr, len,
 HAL_MAX_DELAY);
 if (hstatus == HAL_OK)
 return len;
 else
 return -1;
 }
 return -1;
 }
 */

void ProcessAudioData(void) {
	//uint32_t t_start = __HAL_TIM_GetCounter(&htim5);
	int32_t outl, outr;
	float outfl = 0.0, outfr = 0.0, lfoSample, cutoff_hz_temp, volume_temp, pulsewidth_temp, resonance_temp, envelope;

	for (int n = 0; n < (AUDIO_OUT_BUFFER_SIZE / AUDIO_OUT_CHANNELS) - 1; n += AUDIO_OUT_CHANNELS) {
		//outf = overdrive(outf, 10.0, 0.5, 1.0, overdrive_mix);

		//LFO Target: Filter Cutoff, PW, Volume (Vibrato) *******************************************
		lfoSample = (lfo.getNextSample() / 2 + 0.5);

		if (LFO_TARGET_CUTOFF) {
			//LP Cutoff
			cutoff_hz_temp = cutoff_hz * (lfoSample * lfo_depth + 1 - lfo_depth);
		} else
			cutoff_hz_temp = cutoff_hz;

		if (LFO_TARGET_PULSEWIDTH) {
			//PW
			pulsewidth_temp = pulsewidth * (lfoSample * lfo_depth + 1 - lfo_depth);
		} else {
			pulsewidth_temp = pulsewidth;
		}

		if (LFO_TARGET_VOLUME) {
			//Volume (Vibrato)
			volume_temp = volume * (lfoSample * lfo_depth + 1 - lfo_depth);
		} else {
			volume_temp = volume;
		}

		//END LFO Target: Filter Cutoff, PW, Volume (Vibrato) ***************************************

		//Suboctave Mode Mix ************************************************************************
		if (waveform == mrDDS_Oscillator::SUPER_SAW) {
			osc[0].getNextSampleStereo(outfl, outfr);
		} else if (suboctave == 0) {
			outfl = osc[0].getNextSample();
			outfr = outfl;
		} else {
			outfl = (osc[0].getNextSample() + osc[1].getNextSample() + osc[2].getNextSample()) / 3;
			outfr = outfl;
		}
		if (waveform != mrDDS_Oscillator::SUPER_SAW) {
			outfl = mr_wavefolder_process(&wavefolder, outfl);
			outfr = outfl;
		}
		//END Suboctave Mode Mix *********************************************************************

		//Envelope Target: Note Volume, Filter Cutoff, Filter Resonance ******************************
		env.getEnvelope();
		envelope = env.getCurrentLevel();

		//Volume
		if (ENVELOPE_TARGET_VOLUME) {
			outfl = outfl * envelope;
			outfr = outfr * envelope;
		} else {
			if (env.getState() == 0 || env.getState() == 4) {
				outfl = 0;
				outfr = 0;
			}
		}

		//Filter Cutoff
		resonance_temp = resonance;

		if (ENVELOPE_TARGET_CUTOFF) {
			//cutoff_hz_temp = cutoff_hz_temp * env.getCurrentLevel();
			//cutoff_hz_temp = 200.0 + env.getCurrentLevel()*env.getCurrentLevel()*(cutoff_hz_temp-200.0) ;//* powf(cutoff_hz / 200.0, env.getCurrentLevel() * 0.8);
			cutoff_hz_temp = (cutoff_hz_temp - 200) * envelope * envelope * envelope + 200;
			resonance_temp = resonance * (1 - envelope);
		}

		//Pulsewidth
		if (ENVELOPE_TARGET_PULSEWIDTH) {
			pulsewidth_temp = pulsewidth_temp * envelope;
		}
		//END Envelope Target: Note Volume, Filter Cutoff, Filter Resonance **************************

		if (waveform != mrDDS_Oscillator::SUPER_SAW) {
			switch (suboctave) {
			case 0: //OFF
				for (int i = 0; i < 3; i++)
					osc[i].setPulseWidth(pulsewidth_temp);
				break;
			case 1: //FAT (Unison leicht)
				osc[0].setPulseWidth(pulsewidth_temp);
				osc[1].setPulseWidth(pulsewidth_temp);
				osc[2].setPulseWidth(pulsewidth_temp);
				break;

			case 2: //SUPER FAT (Unison stark)
				osc[0].setPulseWidth(pulsewidth_temp - 0.1f);
				osc[1].setPulseWidth(pulsewidth_temp);
				osc[2].setPulseWidth(pulsewidth_temp + 0.1f);
				break;

			case 3: //BASS (Oktave + Detune)
				osc[0].setPulseWidth(0.0);
				osc[1].setPulseWidth(pulsewidth_temp);
				osc[2].setPulseWidth(0.0);
				break;

			case 4: //FIFTH (Quinte)
				osc[0].setPulseWidth(pulsewidth_temp - 0.1f);
				osc[1].setPulseWidth(pulsewidth_temp);
				osc[2].setPulseWidth(pulsewidth_temp + 0.1f);
				break;

			case 5: //FIFTH (OCTAVE)
				osc[0].setPulseWidth(pulsewidth_temp / 2);
				osc[1].setPulseWidth(0.0);
				osc[2].setPulseWidth(1.0f - pulsewidth_temp / 2);
				break;

			default: //OFF
				for (int i = 0; i < 3; i++)
					osc[i].setPulseWidth(pulsewidth_temp);
				break;
			}
		}

		cutoff_hz_temp = mrroundf(cutoff_hz_temp, 0);  // auf 1 Hz genau
		resonance_temp = mrroundf(resonance_temp, 2);   // auf 0.01 genau

		//outfl = filterL.process(outfl, cutoff_hz_temp, resonance_temp, 1.0f);
		//outfr = filterR.process(outfr, cutoff_hz_temp, resonance_temp, 1.0f);

		filterL.updateCoefficients(cutoff_hz_temp, resonance_temp);
		filterR.updateCoefficients(cutoff_hz_temp, resonance_temp);
		outfl = filterL.process(outfl, 1.0f);
		outfr = filterR.process(outfr, 1.0f);

		outl = (int32_t) (outfl * fToInt32 * volume_temp);
		outr = (int32_t) (outfr * fToInt32 * volume_temp);

		Echo_ProcessSampleStereo(&outl, &outr);

		audioOutBufPtr[n] = outl;		//left
		audioOutBufPtr[n + 1] = outr;	//right

	}
	// Einmal ausgeben:

	/*
	uint32_t t_end = __HAL_TIM_GetCounter(&htim5);
	uint32_t t_us = t_end - t_start;

	static uint32_t count = 0;
	if (++count >= 1000) {
		count = 0;
		usb_printf("ProcessAudioData: %lu us\r\n", t_us);
	}
	 */
	dataReadyFlag = 0;

}
/*
 void ProcessAudioData1(void) {
 //static uint32_t count1 = 0;

 for (int n = 0; n < (AUDIO_OUT_BUFFER_SIZE / AUDIO_OUT_CHANNELS) - 1; n += AUDIO_OUT_CHANNELS) {
 //int32_t sample_l = sin_float[count1 >> 21] * env[0].getEnvelope() *  fToInt32; //key[0].getVelocity()*
 int32_t sample_l = sin_int32[accu >> 21];

 audioOutBufPtr[n] = sample_l;	//tri_float[count0 >> 21] * fToInt32;
 audioOutBufPtr[n + 1] = sample_l;	//sin_float[count1 >> 21] * fToInt32;
 //count0 = (count0 + 8);
 accu = (accu + (1 << 21) * 5);
 }

 //HAL_GPIO_TogglePin(LED_BUILTIN_GPIO_Port, LED_BUILTIN_Pin);

 dataReadyFlag = 0;

 }
 */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
	if (hspi->Instance == SPI3) {
		ConvCpltSPI3_Flag = 1;
	}
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {

	if (hadc->Instance == ADC1) {
		ConvCplt1_Flag = 1;
		//HAL_GPIO_TogglePin(LED_BUILTIN_GPIO_Port, LED_BUILTIN_Pin);
	}
	if (hadc->Instance == ADC2) {
		ConvCplt2_Flag = 1;
	}

	if (hadc->Instance == ADC3) {
		//HAL_GPIO_TogglePin(LED_BUILTIN_GPIO_Port, LED_BUILTIN_Pin);
		ConvCplt3_Flag = 1;
	}
//HAL_GPIO_TogglePin(LED_BUILTIN_GPIO_Port, LED_BUILTIN_Pin);
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai) {
	if (hsai->Instance == SAI1_Block_A) {
		audioOutBufPtr = &audio_out_buffer[AUDIO_OUT_BUFFER_SIZE / 2];
		dataReadyFlag = 1;

	}
	if (hsai->Instance == SAI1_Block_B) {
		;
	}
}

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai) {
	if (hsai->Instance == SAI1_Block_A) {
		audioOutBufPtr = &audio_out_buffer[0];
		dataReadyFlag = 1;
	}
	if (hsai->Instance == SAI1_Block_B) {
		;
	}

}

/*
 void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac) {
 if (hdac->Instance == DAC1) {
 audioOutBufPtr = &audio_out_buffer[AUDIO_OUT_BUFFER_SIZE / 2];
 dataReadyFlag = 1;
 }

 }
 void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac) {
 if (hdac->Instance == DAC1) {
 audioOutBufPtr = &audio_out_buffer[0];
 dataReadyFlag = 1;
 }
 }
 */
} /* extern "C" */
