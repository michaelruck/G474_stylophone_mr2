# G474 Stylophone MR2

A fully custom digital synthesizer built on the **STM32G474CEUx** microcontroller (Cortex-M4, 170 MHz). Inspired by the classic stylophone, it features a 24-key shift-register keyboard, physical knobs for real-time parameter control, and stereo I2S audio output via a PCM5102 DAC.

---

## Features

- **3 DDS Oscillators** — Sine, Triangle/Sawtooth, Square/PWM, Noise, SuperSaw (7-oscillator stereo, Adam Szabo JP-8000 algorithm)
- **LFO** — modulatable to filter cutoff, pulse width, or volume (vibrato)
- **ADSR Envelope** — targets: volume, filter cutoff, pulse width
- **Moog Ladder Filter** — classic 4-pole lowpass, hardware CORDIC-accelerated (`tan()` via STM32G4 CORDIC coprocessor), stereo L/R
- **Wavefolder** — up to 4 folding stages with drive control
- **Stereo Echo** — int16 ring buffer (32 KB in SRAM1), feedback and mix controllable in real time
- **6 Suboctave / Unison modes** — OFF, FAT, SUPER FAT, BASS, FIFTH, OCTAVE
- **WS2812B LEDs** — 3 Neopixels indicating LFO and envelope modulation targets
- **RTC backup registers** — persistent settings across power cycles (octave, waveform, targets)
- **USB CDC** — virtual COM port for debug output
- **Soft power control** — double-click to shut down

---

## Hardware

| Component | Detail |
|---|---|
| MCU | STM32G474CEUx, 512 KB Flash, 128 KB RAM |
| Clock | 170 MHz |
| Audio DAC | PCM5102 via SAI (I2S), ~44.27 kHz stereo |
| Keyboard | 24 keys, shift register via SPI3 + DMA |
| Knobs | 14 potentiometers via 3× ADC + DMA |
| Buttons | 6 push buttons (waveform, suboctave, octave ±, LFO target, envelope target) |
| LEDs | 3× WS2812B Neopixel via TIM3 PWM DMA |
| Coprocessors | CORDIC (filter), FMAC (reserved) |

---

## Signal Chain

```
LFO (Sine)
  └─ modulates ──► Cutoff / Pulse Width / Volume
  
Oscillator 1 ─┐
Oscillator 2 ─┼─► Wavefolder ─► [ADSR] ─► Moog Ladder Filter ─► Echo ─► PCM5102
Oscillator 3 ─┘                              (CORDIC-accelerated)   (int16)
     │
     └─ SuperSaw mode: 7-osc stereo
```

---

## Audio Buffer

- Buffer size: 512 stereo samples (1024 × int32)
- Double-buffered via SAI DMA — half-complete and complete callbacks trigger `ProcessAudioData()`
- CPU budget: ~3840 cycles/sample @ 170 MHz / 44.27 kHz

---

## Memory Layout

The linker script swaps the default SRAM1/SRAM2 assignment to fit large audio buffers:

| Region | Size | Used for |
|---|---|---|
| SRAM1 (`.sram2` section) | 96 KB | Echo buffers L+R (32 KB), audio out buffer |
| SRAM2 (`.ram`) | 32 KB | Stack, heap, general variables |
| CCMRAM | 16 KB | Time-critical data |

---

## Project Structure

```
Core/
├── Inc/
│   ├── mrDDS_Oscillator.h     # DDS oscillator (header-only C++ class)
│   ├── ADSR.h                 # ADSR envelope (header-only C++ class)
│   ├── mr_moog_ladder.h       # Moog ladder filter, CORDIC-accelerated (header-only)
│   ├── mr_echo_int16.h        # Stereo echo, int16 optimized (header-only C)
│   ├── mr_wavefolder.h        # Wavefolder (header-only C)
│   ├── synth_helpers.h        # mapf, mrroundf, overdrive, chorus utilities
│   ├── MR_Neopixel.h          # WS2812B driver via TIM PWM DMA
│   └── mrButtonHW.h           # Button class: debounce, short/long/double-click
└── Src/
    ├── app.cpp                # Main application: setup(), loop(), ProcessAudioData()
    └── main.c                 # STM32CubeMX-generated HAL init
```

---

## Build

Developed with **STM32CubeIDE 1.19.0**. Open the project directly in STM32CubeIDE — no additional setup required. The CMSIS DSP library and STM32G4 HAL drivers are included in the repository.

---

## Author

Michael Ruck — [michael.ruck@marsgasse.com](mailto:michael.ruck@marsgasse.com)
