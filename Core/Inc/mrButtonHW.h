/**
 * @file mrButtonHW.h
 * @brief Hardware button class with click detection for STM32
 * @author michael.ruck@marsgasse.com und claude
 * @date 2026-01-31
 * 
 * Supported click types:
 * - Short click
 * - Long click (configurable duration)
 * - Double click
 */

#ifndef MRBUTTONHW_H
#define MRBUTTONHW_H

#include "stm32g4xx_hal.h"
#include <functional>

class mrButtonHW {
public:
    /**
     * @brief Button click types
     */
    enum class ClickType {
        NONE,
        SHORT,
        LONG,
        DOUBLE
    };

    /**
     * @brief Callback function type
     */
    using ButtonCallback = std::function<void(void)>;

    /**
     * @brief Constructor
     * @param port GPIO port (e.g., GPIOB)
     * @param pin GPIO pin (e.g., GPIO_PIN_6)
     * @param pull GPIO pull configuration (GPIO_PULLUP or GPIO_PULLDOWN)
     */
    mrButtonHW(GPIO_TypeDef* port, uint16_t pin, uint32_t pull = GPIO_PULLUP)
        : m_port(port)
        , m_pin(pin)
        , m_activeLevel((pull == GPIO_PULLUP) ? GPIO_PIN_RESET : GPIO_PIN_SET)
        , m_state(STATE_IDLE)
        , m_lastState(GPIO_PIN_SET)
        , m_pressTime(0)
        , m_releaseTime(0)
        , m_clickCount(0)
        , m_longPressThreshold(800)
        , m_doubleClickWindow(400)
        , m_debounceTime(50)
        , m_onShortClick(nullptr)
        , m_onLongClick(nullptr)
        , m_onDoubleClick(nullptr)
    {
    }

    /**
     * @brief Set callback for short click
     * @param callback Function to call on short click
     */
    void setShortClickCallback(ButtonCallback callback) {
        m_onShortClick = callback;
    }

    /**
     * @brief Set callback for long click
     * @param callback Function to call on long click
     */
    void setLongClickCallback(ButtonCallback callback) {
        m_onLongClick = callback;
    }

    /**
     * @brief Set callback for double click
     * @param callback Function to call on double click
     */
    void setDoubleClickCallback(ButtonCallback callback) {
        m_onDoubleClick = callback;
    }

    /**
     * @brief Set long press threshold in milliseconds
     * @param ms Threshold in milliseconds (default: 800ms)
     */
    void setLongPressThreshold(uint32_t ms) {
        m_longPressThreshold = ms;
    }

    /**
     * @brief Set double click time window in milliseconds
     * @param ms Time window in milliseconds (default: 400ms)
     */
    void setDoubleClickWindow(uint32_t ms) {
        m_doubleClickWindow = ms;
    }

    /**
     * @brief Set debounce time in milliseconds
     * @param ms Debounce time in milliseconds (default: 50ms)
     */
    void setDebounceTime(uint32_t ms) {
        m_debounceTime = ms;
    }

    /**
     * @brief Update button state - call regularly in main loop
     * Must be called at least every 10-20ms for proper debouncing
     */
    void update() {
        uint32_t currentTime = HAL_GetTick();
        GPIO_PinState currentState = HAL_GPIO_ReadPin(m_port, m_pin);

        switch (m_state) {
            case STATE_IDLE:
                if (currentState == m_activeLevel) {
                    m_pressTime = currentTime;
                    m_state = STATE_DEBOUNCE_PRESS;
                }
                // Check for double-click timeout
                else if (m_clickCount == 1 && (currentTime - m_releaseTime) > m_doubleClickWindow) {
                    // Single click confirmed
                    if (m_onShortClick) {
                        m_onShortClick();
                    }
                    m_clickCount = 0;
                }
                break;

            case STATE_DEBOUNCE_PRESS:
                if ((currentTime - m_pressTime) >= m_debounceTime) {
                    if (currentState == m_activeLevel) {
                        m_state = STATE_PRESSED;
                    } else {
                        m_state = STATE_IDLE;
                    }
                }
                break;

            case STATE_PRESSED:
                if (currentState != m_activeLevel) {
                    m_releaseTime = currentTime;
                    m_state = STATE_DEBOUNCE_RELEASE;
                }
                // Check for long press
                else if ((currentTime - m_pressTime) >= m_longPressThreshold) {
                    m_state = STATE_LONG_PRESS;
                    if (m_onLongClick) {
                        m_onLongClick();
                    }
                    m_clickCount = 0; // Reset click count on long press
                }
                break;

            case STATE_DEBOUNCE_RELEASE:
                if ((currentTime - m_releaseTime) >= m_debounceTime) {
                    if (currentState != m_activeLevel) {
                        // Button released
                        uint32_t pressDuration = m_releaseTime - m_pressTime;
                        
                        if (pressDuration < m_longPressThreshold) {
                            m_clickCount++;
                            
                            if (m_clickCount == 2) {
                                // Double click detected
                                if (m_onDoubleClick) {
                                    m_onDoubleClick();
                                }
                                m_clickCount = 0;
                            }
                        }
                        m_state = STATE_IDLE;
                    } else {
                        m_state = STATE_PRESSED;
                    }
                }
                break;

            case STATE_LONG_PRESS:
                if (currentState != m_activeLevel) {
                    m_state = STATE_IDLE;
                }
                break;
        }

        m_lastState = currentState;
    }

    /**
     * @brief Get current button state (pressed or not)
     * @return true if button is currently pressed
     */
    bool isPressed() const {
        return HAL_GPIO_ReadPin(m_port, m_pin) == m_activeLevel;
    }

private:
    enum State {
        STATE_IDLE,
        STATE_DEBOUNCE_PRESS,
        STATE_PRESSED,
        STATE_DEBOUNCE_RELEASE,
        STATE_LONG_PRESS
    };

    GPIO_TypeDef* m_port;
    uint16_t m_pin;
    GPIO_PinState m_activeLevel;
    
    State m_state;
    GPIO_PinState m_lastState;
    uint32_t m_pressTime;
    uint32_t m_releaseTime;
    uint8_t m_clickCount;
    
    uint32_t m_longPressThreshold;
    uint32_t m_doubleClickWindow;
    uint32_t m_debounceTime;
    
    ButtonCallback m_onShortClick;
    ButtonCallback m_onLongClick;
    ButtonCallback m_onDoubleClick;
};

#endif // MRBUTTONHW_H
