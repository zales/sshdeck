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

    void loop() {} // No-op for now, task handles vibration

private:
    Adafruit_TCA8418 keypad;
    QueueHandle_t vibeQueue;
    
    static void vibrationTask(void* parameter);
    void triggerVibration();

    bool sym_active;
    bool shift_active;
    bool ctrl_active;
    bool alt_active;
    unsigned long mic_press_time = 0;
    
    bool initializeKeypad();
    char processKeyEvent(int row, int col, bool pressed);
};
