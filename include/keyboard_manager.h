#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_TCA8418.h>
#include "config.h"

enum SystemEvent {
    SYS_EVENT_NONE,
    SYS_EVENT_SLEEP
};

/**
 * @brief Keyboard Manager for TCA8418 keyboard controller
 * Handles keyboard initialization, event processing, and key mapping
 */
class KeyboardManager {
public:
    KeyboardManager();
    static KeyboardManager* getInstance();
    
    bool begin();
    void loop(); // System loop for handling side buttons etc

    SystemEvent getSystemEvent();
    
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
    void setBacklight(bool on); // Legacy: sets max or off
    void setBacklightLevel(uint8_t level); // 0=off, 1=low, 2=med, 3=high
    uint8_t getBacklightLevel() const { return _backlightLevel; }
    
    void triggerHaptic(); // Public for UI

private:
    static KeyboardManager* instance;
    Adafruit_TCA8418 keypad;
    QueueHandle_t hapticQueue;
    QueueHandle_t inputQueue;
    uint8_t _backlightLevel = 0; // 0=off, 1=low, 2=med, 3=high
    TaskHandle_t inputTaskHandle;
    
    // State
    unsigned long pwrBtnStart;

    // Queue item structure
    struct KeyEvent {
        char key;
        uint8_t modifiers; // 0=None, 1=Shift, 2=Ctrl/Mic, 4=Alt, 8=Sym
    };
    
    static void hapticTask(void* parameter);
    static void inputTask(void* parameter);
    static void IRAM_ATTR isr();
    // void triggerHaptic(); // Moved to public

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
