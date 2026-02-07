#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <CSE_CST328.h>
#include "board_def.h"

// Touch driver using CSE_CST328 library for CST328/CST226SE
// Runs polling in a FreeRTOS task for fast gesture capture
// even when main loop is blocked by e-ink updates

enum TouchGesture {
    GESTURE_NONE       = 0,
    GESTURE_SWIPE_UP   = 1,
    GESTURE_SWIPE_DOWN = 2,
    GESTURE_SWIPE_LEFT = 3,
    GESTURE_SWIPE_RIGHT= 4,
    GESTURE_SINGLE_TAP = 5,
    GESTURE_LONG_PRESS = 6,
};

struct TouchEvent {
    bool touched;
    TouchGesture gesture;
    int x;
    int y;
};

class TouchManager {
public:
    bool begin(TwoWire& wire);

    // Returns latest gesture event (thread-safe, non-blocking)
    TouchEvent read();
    bool available();

    // Diagnostics
    const char* getModelName();
    uint32_t getChipId();
    bool isInitialized() const { return _initialized; }

private:
    CSE_CST328* _panel = nullptr;
    bool _initialized = false;
    uint32_t _chipId = 0;

    // --- Polling task state (accessed only from pollLoop) ---
    bool _wasDown = false;
    int16_t _startX = 0, _startY = 0;
    int16_t _lastX = 0, _lastY = 0;
    unsigned long _startTime = 0;
    unsigned long _lastActiveTime = 0;

    // Gesture thresholds
    static constexpr int SWIPE_MIN_DIST = 20;
    static constexpr unsigned long TAP_MAX_MS = 600;
    static constexpr unsigned long SWIPE_MAX_MS = 3000;
    static constexpr unsigned long RELEASE_DEBOUNCE_MS = 150;
    static constexpr unsigned long TOUCH_TIMEOUT_MS = 5000;

    TouchGesture detectGesture(int16_t dx, int16_t dy, unsigned long duration);

    // --- Inter-task communication ---
    volatile TouchGesture _pendingGesture = GESTURE_NONE;
    volatile bool _pendingTouched = false;
    volatile int _pendingX = 0, _pendingY = 0;
    portMUX_TYPE _spinlock = portMUX_INITIALIZER_UNLOCKED;

    // FreeRTOS task
    TaskHandle_t _taskHandle = nullptr;
    static void pollTask(void* param);
    void pollLoop();
};
