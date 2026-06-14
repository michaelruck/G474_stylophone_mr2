# mrButtonHW - Hardware Button Class für STM32

**Autoren:** michael.ruck@marsgasse.com und claude  
**Datum:** 2026-01-31  
**Plattform:** STM32CubeIDE 1.19.0, STM32G474CEU6, C++, HAL

## Übersicht

Die `mrButtonHW` Klasse bietet eine einfache Möglichkeit, verschiedene Klick-Typen eines Hardware-Buttons zu erkennen:

- **Short Click** - Kurzer Tastendruck
- **Long Click** - Langer Tastendruck (Dauer einstellbar)
- **Double Click** - Doppelklick

Die Klasse verwendet die STM32 HAL-Bibliothek und benötigt keine zusätzliche .cpp-Datei.

## Features

- ✅ Debouncing integriert (Standard: 50ms)
- ✅ Konfigurierbare Long-Press-Schwelle (Standard: 800ms)
- ✅ Konfigurierbare Double-Click-Zeitfenster (Standard: 400ms)
- ✅ Callback-Funktionen für alle Klick-Typen
- ✅ Pull-up und Pull-down Unterstützung
- ✅ State-Machine basierte Implementierung
- ✅ Header-only Implementierung

## Installation

1. Kopieren Sie `mrButtonHW.h` in Ihr Projekt
2. Inkludieren Sie die Header-Datei: `#include "mrButtonHW.h"`

## GPIO Konfiguration

Konfigurieren Sie den GPIO-Pin in STM32CubeMX oder im Code:

```c
// Beispiel für Pull-up Konfiguration (Button gegen GND)
GPIO_InitTypeDef GPIO_InitStruct = {0};
GPIO_InitStruct.Pin = GPIO_PIN_6;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
GPIO_InitStruct.Pull = GPIO_PULLUP;
HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
```

## Grundlegendes Beispiel

```cpp
#include "mrButtonHW.h"

// Globale Button-Instanz
mrButtonHW button1(GPIOB, GPIO_PIN_6);

// Callback-Funktionen
void onShortClick() {
    HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
}

void onLongClick() {
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
}

void onDoubleClick() {
    HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin);
}

// Setup
void setup() {
    // Callbacks zuweisen
    button1.setShortClickCallback(onShortClick);
    button1.setLongClickCallback(onLongClick);
    button1.setDoubleClickCallback(onDoubleClick);
}

// Main Loop
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    
    setup();
    
    while (1) {
        button1.update();  // Regelmäßig aufrufen (alle 10-20ms empfohlen)
        HAL_Delay(10);
    }
}
```

## Erweiterte Beispiele

### Beispiel 1: Lambda-Funktionen verwenden

```cpp
mrButtonHW button2(GPIOA, GPIO_PIN_0);

void setup() {
    // Lambda-Funktionen als Callbacks
    button2.setShortClickCallback([]() {
        printf("Short click detected!\n");
    });
    
    button2.setLongClickCallback([]() {
        printf("Long press detected!\n");
    });
    
    button2.setDoubleClickCallback([]() {
        printf("Double click detected!\n");
    });
}
```

### Beispiel 2: Mehrere Buttons

```cpp
mrButtonHW btnUp(GPIOB, GPIO_PIN_6);
mrButtonHW btnDown(GPIOB, GPIO_PIN_7);
mrButtonHW btnSelect(GPIOB, GPIO_PIN_8);

uint8_t menuIndex = 0;

void setup() {
    btnUp.setShortClickCallback([]() {
        menuIndex++;
        updateDisplay();
    });
    
    btnDown.setShortClickCallback([]() {
        if (menuIndex > 0) menuIndex--;
        updateDisplay();
    });
    
    btnSelect.setShortClickCallback([]() {
        selectMenuItem(menuIndex);
    });
    
    btnSelect.setLongClickCallback([]() {
        exitMenu();
    });
}

void mainLoop() {
    btnUp.update();
    btnDown.update();
    btnSelect.update();
}
```

### Beispiel 3: Konfigurierbare Parameter

```cpp
mrButtonHW button3(GPIOC, GPIO_PIN_13);

void setup() {
    // Long-Press auf 1.5 Sekunden setzen
    button3.setLongPressThreshold(1500);
    
    // Double-Click Zeitfenster auf 300ms setzen
    button3.setDoubleClickWindow(300);
    
    // Debounce-Zeit auf 30ms setzen
    button3.setDebounceTime(30);
    
    button3.setShortClickCallback(onShortClick);
    button3.setLongClickCallback(onLongClick);
    button3.setDoubleClickCallback(onDoubleClick);
}
```

### Beispiel 4: Pull-down Konfiguration (Button gegen VCC)

```cpp
// Für Buttons, die gegen VCC geschaltet sind
mrButtonHW button4(GPIOA, GPIO_PIN_1, GPIO_PULLDOWN);

void setup() {
    button4.setShortClickCallback([]() {
        printf("Button with pull-down pressed!\n");
    });
}
```

### Beispiel 5: State-Abfrage

```cpp
mrButtonHW button5(GPIOB, GPIO_PIN_5);

void mainLoop() {
    button5.update();
    
    // Direkte State-Abfrage
    if (button5.isPressed()) {
        // Button ist aktuell gedrückt
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    }
}
```

### Beispiel 6: Timer-basiertes Update (empfohlen)

```cpp
mrButtonHW button6(GPIOB, GPIO_PIN_6);

// TIM2 Interrupt Handler (10ms Timer)
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) {
        button6.update();  // Update alle 10ms
    }
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM2_Init();
    
    button6.setShortClickCallback(onShortClick);
    
    HAL_TIM_Base_Start_IT(&htim2);
    
    while (1) {
        // Hauptschleife kann für andere Aufgaben genutzt werden
    }
}
```

## Wichtige Hinweise

### Update-Frequenz
- Die `update()` Methode sollte **regelmäßig** aufgerufen werden (empfohlen: alle 10-20ms)
- Bei zu seltenen Updates funktioniert das Debouncing und die Click-Erkennung nicht korrekt
- Timer-basiertes Update ist die bevorzugte Methode für präzises Timing

### Timing-Parameter
- **Long Press Threshold:** Zeit in ms, nach der ein Long-Click erkannt wird (Standard: 800ms)
- **Double Click Window:** Maximale Zeit zwischen zwei Klicks für Double-Click (Standard: 400ms)
- **Debounce Time:** Entprellzeit für mechanische Taster (Standard: 50ms)

### Hardware-Konfiguration
- **Pull-up (Standard):** Button gegen GND, `GPIO_PULLUP` verwenden (oder weglassen)
- **Pull-down:** Button gegen VCC, `GPIO_PULLDOWN` verwenden

### Callback-Verhalten
- Callbacks sind optional - nur die benötigten setzen
- Short-Click wird **nicht** ausgelöst bei Long-Click
- Double-Click benötigt zwei schnelle Short-Clicks
- Bei Double-Click wird kein Short-Click ausgelöst

## API-Referenz

### Konstruktor
```cpp
mrButtonHW(GPIO_TypeDef* port, uint16_t pin, uint32_t pull = GPIO_PULLUP)
```
- `port`: GPIO Port (z.B. GPIOB)
- `pin`: GPIO Pin (z.B. GPIO_PIN_6)
- `pull`: GPIO Pull-Konfiguration (GPIO_PULLUP oder GPIO_PULLDOWN)

### Methoden

#### Callback-Konfiguration
```cpp
void setShortClickCallback(ButtonCallback callback)
void setLongClickCallback(ButtonCallback callback)
void setDoubleClickCallback(ButtonCallback callback)
```

#### Timing-Konfiguration
```cpp
void setLongPressThreshold(uint32_t ms)      // Standard: 800ms
void setDoubleClickWindow(uint32_t ms)       // Standard: 400ms
void setDebounceTime(uint32_t ms)            // Standard: 50ms
```

#### Runtime-Methoden
```cpp
void update()           // Muss regelmäßig aufgerufen werden
bool isPressed()        // Gibt aktuellen Button-State zurück
```

## Troubleshooting

**Problem:** Double-Click wird nicht erkannt
- Lösung: `setDoubleClickWindow()` erhöhen oder schneller klicken

**Problem:** Mehrfache Klicks werden erkannt
- Lösung: `setDebounceTime()` erhöhen (schlechte Taster brauchen mehr Debouncing)

**Problem:** Long-Press wird zu früh ausgelöst
- Lösung: `setLongPressThreshold()` erhöhen

**Problem:** Callbacks werden nicht aufgerufen
- Lösung: Sicherstellen, dass `update()` regelmäßig aufgerufen wird

## Lizenz

Dieses Projekt steht zur freien Verwendung zur Verfügung.

## Support

Bei Fragen oder Problemen: michael.ruck@marsgasse.com
