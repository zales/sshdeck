#pragma once
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <Update.h>
#include "display_manager.h"

class OtaManager {
public:
    OtaManager(DisplayManager& display);
    void begin();
    void loop();

    // Pull update from HTTP/HTTPS URL
    bool updateFromUrl(const String& url, const String& rootCaCert = "");

    // Check if update is available (fetches url replacing extension with .txt or using specific url)
    // Returns new version string if newer, else empty string.
    String checkUpdateAvailable(const String& binUrl, const String& currentVersion, const String& rootCaCert = "");

private:
    DisplayManager& display;
    
    // Helper to draw progress bar
    void drawProgress(int percent, String info);
};
