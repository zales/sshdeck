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
    bool isCtrlActive() const { return ctrl_active; }
    
private:
    Adafruit_TCA8418 keypad;
    
    bool sym_active;
    bool shift_active;
    bool ctrl_active;
    
    bool initializeKeypad();
    char processKeyEvent(int row, int col, bool pressed);
};
