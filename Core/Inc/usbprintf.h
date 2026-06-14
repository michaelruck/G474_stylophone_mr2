/*
 * usbprintf.h
 *
 *  Created on: Jan 25, 2026
 *      Author: michael.ruck@marsgasse.com
 *      mit Claude
 *      https://claude.ai/chat/f80e3eb5-a435-4917-b099-58a5fe4f0c25
 */
/*
 * usbprintf.h - Minimales USB Virtual COM Port Setup für STM32
 *
 * Minimale CubeMX Einstellungen erforderlich:
 * 1. USB_OTG_FS aktivieren (Device Only)
 * 2. USB_DEVICE Middleware aktivieren
 * 3. Class: Communication Device Class (Virtual Port Com)
 *
 * Verwendung:
 * #include "usbprintf.h"
 *
 * int main(void) {
 *   HAL_Init();
 *   SystemClock_Config();
 *
 *   // Einfachste Variante - sendet nur wenn Terminal verbunden
 *   usb_printf_init();
 *   // ODER: usb_printf_init(false, false);
 *
 *   // Mit stdout Umleitung - printf() geht über USB
 *   // usb_printf_init(true, false);
 *
 *   // Mit Warten auf Terminal - gut für Debug (blockiert bis Terminal offen!)
 *   // usb_printf_init(false, true);
 *
 *   // Beides - printf() über USB + wartet auf Terminal
 *   // usb_printf_init(true, true);
 *
 *   while(1) {
 *     usb_printf("Hello World! Counter: %d\n", counter++);
 *     HAL_Delay(1000);
 *   }
 * }
 */

#ifndef USBPRINTF_H
#define USBPRINTF_H

#include "main.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// Buffer für printf-Ausgaben
#define USB_PRINTF_BUFFER_SIZE 256

// Externe Variablen (normalerweise von CubeMX generiert)
extern USBD_HandleTypeDef hUsbDeviceFS;

// USB Descriptor Makros - MANUELL ANPASSEN!
// Schauen Sie in USB_Device/App/usbd_desc.c nach der Zeile:
//   USBD_DescriptorsTypeDef [NAME] = {
// und setzen Sie hier den [NAME] ein:
#ifndef USB_DESC_NAME
    #define USB_DESC_NAME CDC_Desc
#endif

// Schauen Sie in USB_Device/App/usbd_cdc_if.c nach der Zeile:
//   USBD_CDC_ItfTypeDef [NAME] = {
// und setzen Sie hier den [NAME] ein:
#ifndef USB_ITF_NAME
    #define USB_ITF_NAME USBD_Interface_fops_FS
#endif

// Externe Deklarationen mit den Makros
extern USBD_DescriptorsTypeDef USB_DESC_NAME;
extern USBD_CDC_ItfTypeDef USB_ITF_NAME;

// Globale Konfiguration
static struct {
    bool redirect_stdout;
    bool wait_for_terminal;
} usb_printf_config = {false, false};

// Hilfsfunktion um zu prüfen ob USB verbunden ist
static inline uint8_t usb_is_connected(void) {
    return hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED;
}

// Initialisierung
// redirect_stdout: true = normales printf() wird über USB umgeleitet
// wait_for_terminal: true = wartet bis Terminal-Programm verbunden ist (gut für Debug)
static inline void usb_printf_init(bool redirect_stdout = false, bool wait_for_terminal = false) {
    usb_printf_config.redirect_stdout = redirect_stdout;
    usb_printf_config.wait_for_terminal = wait_for_terminal;

    // USB Device initialisieren (normalerweise in main.c von CubeMX)
    if (USBD_Init(&hUsbDeviceFS, &USB_DESC_NAME, DEVICE_FS) != USBD_OK) {
        Error_Handler();
    }
    if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) != USBD_OK) {
        Error_Handler();
    }
    if (USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USB_ITF_NAME) != USBD_OK) {
        Error_Handler();
    }
    if (USBD_Start(&hUsbDeviceFS) != USBD_OK) {
        Error_Handler();
    }

    // Längere Wartezeit für stabilere Verbindung
    HAL_Delay(2000);

    // USB "Soft Reconnect" durchführen
    USBD_Stop(&hUsbDeviceFS);
    HAL_Delay(100);
    USBD_Start(&hUsbDeviceFS);
    HAL_Delay(500);

    // Optional: Auf Terminal-Verbindung warten
    if (wait_for_terminal) {
        while (!usb_is_connected()) {
            HAL_Delay(100);
        }
    }
}

// USB printf Funktion
static inline void usb_printf(const char* format, ...) {
    static char buffer[USB_PRINTF_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len > 0) {
        // Wenn wait_for_terminal aktiv, auf Verbindung warten
        if (usb_printf_config.wait_for_terminal) {
            while (!usb_is_connected()) {
                HAL_Delay(100);
            }
        } else {
            // NICHT warten - wenn nicht verbunden, sofort beenden
            if (!usb_is_connected()) {
                return;
            }
        }

        // Nur wenn verbunden: Warten bis vorherige Übertragung abgeschlossen
        if (hUsbDeviceFS.pClassData != NULL) {
            while (((USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData)->TxState != 0) {
                // Nochmal checken ob noch verbunden während wir warten
                if (!usb_is_connected() && !usb_printf_config.wait_for_terminal) {
                    return;
                }
                HAL_Delay(1);
            }

            // Daten senden
            CDC_Transmit_FS((uint8_t*)buffer, len);
        }
    }
}

// Standard printf umleiten - wird über usb_printf_init() Parameter aktiviert
int _write(int file, char *ptr, int len) {
    if (!usb_printf_config.redirect_stdout) {
        return len;  // Nicht umleiten
    }

    static char buffer[USB_PRINTF_BUFFER_SIZE];
    if (len >= (int)sizeof(buffer)) {
        len = sizeof(buffer) - 1;
    }

    memcpy(buffer, ptr, len);
    buffer[len] = '\0';

    // Wenn wait_for_terminal aktiv, auf Verbindung warten
    if (usb_printf_config.wait_for_terminal) {
        while (!usb_is_connected()) {
            HAL_Delay(100);
        }
    } else {
        // NICHT warten - wenn nicht verbunden, sofort beenden
        if (!usb_is_connected()) {
            return len;
        }
    }

    // Nur wenn verbunden: Warten bis vorherige Übertragung abgeschlossen
    if (hUsbDeviceFS.pClassData != NULL) {
        while (((USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData)->TxState != 0) {
            // Nochmal checken ob noch verbunden während wir warten
            if (!usb_is_connected() && !usb_printf_config.wait_for_terminal) {
                return len;
            }
            HAL_Delay(1);
        }

        CDC_Transmit_FS((uint8_t*)buffer, len);
    }

    return len;
}

// Sichere printf-Version die nur sendet wenn verbunden
static inline void usb_printf_safe(const char* format, ...) {
    if (!usb_is_connected()) {
        return;
    }

    static char buffer[USB_PRINTF_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len > 0) {
        while (((USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData)->TxState != 0) {
            HAL_Delay(1);
        }
        CDC_Transmit_FS((uint8_t*)buffer, len);
    }
}

#endif /* USBPRINTF_H */
