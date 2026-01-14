#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_TCA8418.h>
#include "config.h"

/**
 * @brief Keyboard Manager for TCA8418 keyboard controller
 * Handles keyboard initialization, event processing, and key mapping
 */
class KeyboardManager {
public:
    KeyboardManager();
    
    bool begin();
    bool isKeyPressed();
    int available(); // Check number of events in buffer
    char getKeyChar();
    
    // Modifier states
    bool isSymActive() const { return sym_active; }
    bool isShiftActive() const { return shift_active; }
    bool isCtrlActive() const { return ctrl_active; } // Mic Key acts as Ctrl
    bool isAltActive() const { return alt_active; }
    bool isMicActive() const { return ctrl_active; } // Alias for clarity
    
    unsigned long getMicPressTime() const { return mic_press_time; }
    
    void toggleBacklight();
    void setBacklight(bool on);

    void loop() {} // No-op, task handles inputs

    // Queue item structure
    struct KeyEvent {
        char key;
        uint8_t modifiers; // 0=None, 1=Shift, 2=Ctrl/Mic, 4=Alt, 8=Sym
    };

private:
    Adafruit_TCA8418 keypad;
    QueueHandle_t hapticQueue;
    QueueHandle_t inputQueue;
    TaskHandle_t inputTaskHandle;
    
    static void hapticTask(void* parameter);
    static void inputTask(void* parameter);
    static void IRAM_ATTR isr();
    void triggerHaptic();

    volatile bool sym_active;
    volatile bool shift_active;
    volatile bool ctrl_active;
    volatile bool alt_active;
    volatile unsigned long mic_press_time = 0;
    
    bool initializeKeypad();
    char processKeyEvent(int row, int col, bool pressed);
    
public:
    void clearBuffer(unsigned long durationMs = 300);
};
