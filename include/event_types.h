#pragma once

#include <Arduino.h>

enum EventType {
    EVENT_NONE,
    EVENT_KEY_PRESS,
    EVENT_SYSTEM,
    EVENT_NETWORK
};

struct InputEvent {
    EventType type;
    char key;           // For KEY_PRESS
    int systemCode;     // For SYSTEM
    
    // Constructors for convenience
    InputEvent() : type(EVENT_NONE), key(0), systemCode(0) {}
    InputEvent(char k) : type(EVENT_KEY_PRESS), key(k), systemCode(0) {}
    InputEvent(EventType t, int code) : type(t), key(0), systemCode(code) {}
};
