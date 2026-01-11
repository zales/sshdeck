#include <Arduino.h>
#include "app.h"

// The main application instance
App app;

void setup() {
    app.setup();
}

void loop() {
    app.loop();
}
