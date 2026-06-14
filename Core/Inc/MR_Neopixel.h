/*
 * MR_Neopixel.h
 *
 *  Created on: Aug 6, 2025
 *      Author: michael
 *  Überarbeitet: 15.1.2026
 */

#ifndef INC_MR_NEOPIXEL_H_
#define INC_MR_NEOPIXEL_H_

#include "main.h"
#include "MR_Neopixel.h"
#include <math.h>
#include <stdio.h>
#include "usbprintf.h"

#define WS_STRIP_LENGTH 3 //LEDs in Led strip

/* WS2812b, GRB */
#define GRB	1
#define WS_COLOR_SEQUENCE GRB
#define WS_DMA_BUFFER_LENGTH (WS_STRIP_LENGTH * 3 * 8 + WS_LATCH) //Jedes Datenbit ist ein PWM-Puls

#define WS_FREQUENCY 800000 //Hz = 1/1.25µs
#define WS_TIMING_LOW 	400 //ns
#define WS_TIMING_HIGH 	800 //ns
#define WS_TIMING_LATCH 50000 //ns

#define WS_TIMER_PERIOD (80-1)
//#define WS_LOW 25 //=>0.4µs
//#define WS_HIGH 50 //=>0.8µs
#define WS_LATCH 44	//Byte =>50µs

/* SK6812, GRB */
#define SK_FREQUENCY // 1/1.2µs
#define SK_TIMER_PERIOD (77-1)
#define SK_LOW 20 //0.32µs
#define SK_HIGH 40//0.64µs
#define SK_LATCH 70//>80µs

//**************************************************
#define TIMER_CLK 64.0 //MHz

TIM_HandleTypeDef *my_timer;
uint32_t my_channel;

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t w;
} color_type;

color_type ws_leds[WS_STRIP_LENGTH]; //Color data of each LED
uint8_t ws_dma_buffer[WS_DMA_BUFFER_LENGTH] = { 0 };

color_type ws_col_black = { 0, 0, 0, 0 };
color_type ws_col_all = { 255, 255, 255, 255 };
color_type ws_col_red = { 50, 0, 0, 0 };
color_type ws_col_green = { 0, 255, 0, 0 };
color_type ws_col_blue = { 0, 0, 50, 0 };
color_type ws_col_white = { 0, 0, 0, 255 };
color_type ws_col_yellow = { 255, 255, 0, 0 };
color_type ws_col_orange = { 255, 128, 0, 0 };
color_type ws_col_magenta = { 25, 0, 25, 0 };

/* Parameter werden beim init berechnet */
uint32_t cycles_low = 34;
uint32_t cycles_high = 69;

float timer_frequency = 800000; //Hz
/******************************************/

//color_type ws_black = {.r=0, .g=0, .b=0, .w=0};

/* Begin Function Prototypes */
void ws_build_buffer(void);

void ws_set_led(int led_idx, color_type led_color); // -1 sets all leds to color
void ws_send(void);
void ws_init(TIM_HandleTypeDef *htimer, uint32_t channel);
color_type HsvToRgb(uint8_t hue, uint8_t saturation, uint8_t brightness);
void ws_set_led_zone(int led_idx_start, int led_idx_end, color_type led_color);
void ws_set_led_bar(int led_idx_start, int led_width, color_type led_color);
void ws_clear(void);
void ws_set_led_smoothbar(float led_idx_start, float led_width, color_type led_color, color_type led_backcolor);
color_type ws_blendColor(uint8_t fade, color_type colorA, color_type colorB); // unsigned int Version
color_type ws_blendColorf(float fade, color_type colorA, color_type colorB); // float Version

/* End Function Prototypes */

//builds the led strip
void ws_clear(void) {
	ws_set_led(-1, ws_col_black);
}

void ws_set_led(int led_idx, color_type led_color) {
	if (led_idx < WS_STRIP_LENGTH && led_idx >= 0) {
		ws_leds[led_idx].r = led_color.r;
		ws_leds[led_idx].g = led_color.g;
		ws_leds[led_idx].b = led_color.b;
		ws_leds[led_idx].w = led_color.w;
	} else if (led_idx < 0) {
		//set all leds to same color
		for (int i = 0; i < WS_STRIP_LENGTH; i++) {
			ws_leds[i].r = led_color.r;
			ws_leds[i].g = led_color.g;
			ws_leds[i].b = led_color.b;
			ws_leds[i].w = led_color.w;
		}
	}
	//ws_build_buffer();
}

void ws_set_led_bar(int led_idx_start, int led_width, color_type led_color) {
	// 8 LEDs vorhanden, start bei idx 4, 6 LEDs im Kreis an
	uint16_t idx = led_idx_start;
	if (led_width > WS_STRIP_LENGTH)
		led_width = WS_STRIP_LENGTH;
	for (int i = 0; i < led_width; i++) {
		ws_set_led(idx, led_color);
		idx = (idx + 1) % WS_STRIP_LENGTH;
	}

}

void ws_set_led_smoothbar(float led_idx_start, float led_width, color_type led_color, color_type led_backcolor) {
// Arbeitet mit Zwischenwerten um einen sanften Übergang darzustellen
// Challenge: benachbarte LEDs finden und ansteuern
	//float startFade = led_idx_start - truncf(led_idx_start);
	float idx = led_idx_start;

	if (led_width > WS_STRIP_LENGTH)
		led_width = WS_STRIP_LENGTH;

	for (int i = 0; i < (int) (led_width); i++) {
		ws_set_led((int) (idx), ws_blendColorf(idx, led_color, led_backcolor));
		idx = fmodf((idx + 1), WS_STRIP_LENGTH);
		//endFade= idx-truncf(idx);
	}

}

void ws_set_led_zone(int led_idx_start, int led_idx_end, color_type led_color) {
	if (led_idx_start < 0 || led_idx_start > WS_STRIP_LENGTH - 1)
		return;
	if (led_idx_end < 0 || led_idx_end > WS_STRIP_LENGTH - 1)
		return;

	for (int i = led_idx_start; i < led_idx_end; i++)
		ws_set_led(i, led_color);
}

/**
 * @brief Mischt zwei Farben basierend auf einem blend-Wert (0-255)
 * @param blend Mischfaktor (0=100% colorA, 255=100% colorB)
 * @param colorA Erste Farbe
 * @param colorB Zweite Farbe
 * @return Gemischte Farbe
 */
color_type ws_blendColor(uint8_t blend, color_type colorA, color_type colorB) {
	color_type result;

	// Berechne den inversen blend-Wert (für colorA)
	uint16_t inv_fade = 255 - blend;

	// Mische jede Farbkomponente (mit 16-Bit Zwischenergebnissen um Überlauf zu vermeiden)
	result.r = (uint8_t) ((colorA.r * inv_fade + colorB.r * blend) / 255);
	result.g = (uint8_t) ((colorA.g * inv_fade + colorB.g * blend) / 255);
	result.b = (uint8_t) ((colorA.b * inv_fade + colorB.b * blend) / 255);

	return result;
}

color_type ws_blendColorf(float blend, color_type colorA, color_type colorB) {
	blend = fabsf(blend);
	//usb_printf("blend=%f\r\n", blend);

	blend = fmodf(blend, 1.0);
	//usb_printf("blend=%f\r\n", blend);

	color_type val = { .r = (uint8_t) (colorA.r * (1.0f - blend) + colorB.r * blend), .g = (uint8_t) (colorA.g * (1.0f - blend) + colorB.g * blend), .b = (uint8_t) (colorA.b
			* (1.0f - blend) + colorB.b * blend) };
	return val;
}

void ws_build_buffer(void) {
//HAL_GPIO_TogglePin(TEST_GPIO_Port, TEST_Pin);
	int j = 0;
	uint8_t temp = 0;
	//usb_printf("ws_build_buffer--->\r\n");
	for (int i = 0; i < WS_STRIP_LENGTH; i++) {
		//j = i * 3 * 8;
		//grb
		//usb_printf("-- LED idx %i\r\n", i);
		temp = ws_leds[i].g;
		//usb_printf("ws_leds[%i].g=%u\r\n", i, temp);
		for (int bit = 7; bit >= 0; bit--) {
			ws_dma_buffer[j] = ((temp >> bit) & 0x01) ? cycles_high : cycles_low;
			//usb_printf("g bit=%i, pwm=%hu\r\n", bit, ws_dma_buffer[j]);
			j++;
		}

		//printf("r\r\n");
		temp = ws_leds[i].r;
		//usb_printf("ws_leds[%i].r=%u\r\n", i, temp);
		for (int bit = 7; bit >= 0; bit--) {
			ws_dma_buffer[j] = ((temp >> bit) & 0x01) ? cycles_high : cycles_low;
			//usb_printf("r bit=%i, pwm=%hu\r\n", bit, ws_dma_buffer[j]);
			j++;
		}

		//printf("b\r\n");
		temp = ws_leds[i].b;
		//usb_printf("ws_leds[%i].b=%u\r\n", i, temp);
		for (int bit = 7; bit >= 0; bit--) {
			ws_dma_buffer[j] = ((temp >> bit) & 0x01) ? cycles_high : cycles_low;
			//usb_printf("b bit=%i, pwm=%hu\r\n", bit, ws_dma_buffer[j]);
			j++;
		}

	}
}

void ws_send(void) {
	ws_build_buffer();
	/*usb_printf("ws_send--->\r\n");
	 for (int i = 0; i < WS_DMA_BUFFER_LENGTH; i++) {
	 usb_printf("%0hu", ws_dma_buffer[i]);
	 if (i % 8 == 7)
	 usb_printf("|");
	 else
	 usb_printf(",");
	 if (i % 24 == 23)
	 usb_printf("\r\n");
	 }
	 usb_printf("\r\n");
	 */
	HAL_TIM_PWM_Start_DMA(my_timer, my_channel, (uint32_t*) ws_dma_buffer,
	WS_DMA_BUFFER_LENGTH);
}

void ws_init(TIM_HandleTypeDef *htimer, uint32_t channel) {
	my_timer = htimer;
	my_channel = channel;

	//set Timer Prescalers and Timings
	uint32_t sysclk = HAL_RCC_GetSysClockFreq();
	my_timer->Instance->PSC = 1;

	uint16_t temp_arr = llround((float) sysclk / (my_timer->Instance->PSC + 1) / WS_FREQUENCY) - 1;
	//usb_printf("temp_arr: %u\r\n", temp_arr);
	my_timer->Instance->ARR = temp_arr;
	timer_frequency = (float) sysclk / (my_timer->Instance->PSC + 1) / (my_timer->Instance->ARR + 1);
	//usb_printf("real timer_frequency: %f\r\n", timer_frequency);

	//Low, High and Latch
	//cycles_low=
}

/**
 * @brief Konvertiert HSV zu RGB mit 8-Bit-Eingängen (0-255) und automatischem Überlauf
 * @param hue Farbton (0-255, wird auf 0-359° skaliert)
 * @param saturation Sättigung (0-255)
 * @param brightness Helligkeit (0-255)
 * @return color_type RGB-Farbwert
 */
color_type HsvToRgb(uint8_t hue, uint8_t saturation, uint8_t brightness) {
	color_type rgb;
	uint16_t h, s, v, region, remainder, p, q, t;

// Skaliere Hue auf 0-359° (0-255 → 0-359)
	h = ((uint16_t) hue * 359) / 255;
	s = saturation;
	v = brightness;

// Reduziere auf 6 Regionen (je 60°)
	region = h / 60;
	remainder = (h % 60) * 255 / 60; // Skaliert auf 0-255 für Interpolation

// Zwischenwerte berechnen (p, q, t)
	p = (v * (255 - s)) / 255;
	q = (v * (255 - (s * remainder) / 255)) / 255;
	t = (v * (255 - (s * (255 - remainder)) / 255)) / 255;

// RGB-Werte je nach Region zuweisen
	switch (region % 6) {
	case 0:
		rgb.r = v;
		rgb.g = t;
		rgb.b = p;
		break;
	case 1:
		rgb.r = q;
		rgb.g = v;
		rgb.b = p;
		break;
	case 2:
		rgb.r = p;
		rgb.g = v;
		rgb.b = t;
		break;
	case 3:
		rgb.r = p;
		rgb.g = q;
		rgb.b = v;
		break;
	case 4:
		rgb.r = t;
		rgb.g = p;
		rgb.b = v;
		break;
	case 5:
		rgb.r = v;
		rgb.g = p;
		rgb.b = q;
		break;
	}

	return rgb;
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == my_timer->Instance) {
		//my_timer->Instance->CCR2=0;
		//HAL_TIM_PWM_Stop_DMA(my_timer, my_channel); //führt bei timern ohne "CH Idle State = Reset" zu Problemen
		//HAL_GPIO_TogglePin(TEST_GPIO_Port, TEST_Pin);
		//HAL_GPIO_TogglePin(LED_BUILTIN_GPIO_Port, LED_BUILTIN_Pin);
	}
}

#endif /* INC_MR_NEOPIXEL_H_ */
