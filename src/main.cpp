#include <Arduino.h>
#include "app.h"
#include "board_def.h"

// Define Global I2C Mutex
SemaphoreHandle_t i2cMutex = NULL;

// The main application instance
App app;

void setup() {
    i2cMutex = xSemaphoreCreateMutex();
    app.setup();
}

void loop() {
    app.loop();
}
