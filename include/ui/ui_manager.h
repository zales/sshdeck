#pragma once

#include "../display_manager.h"
#include <Arduino.h>

class UIManager {
public:
    UIManager(DisplayManager& display);

    // Helpers to get dimensions dynamically
    int width() const;
    int height() const;

    // Standard Screens
    void drawPinEntry(const String& title, const String& subtitle, const String& entry, bool isWrong = false);
    void drawMessage(const String& title, const String& message);
    void drawSystemInfo(const String& ip, const String& bat, const String& ram, const String& mac);
    void drawShutdownScreen();
    void drawBootScreen(const String& line1, const String& line2);
    void updateBootStatus(const String& status);
    
    // Elements
    void drawStatusBar(bool wifiConnected, const String& wifiSSID, 
                       bool sshConnected, const String& sshHost, 
                       int batteryPercent, float batteryVolts);
    
    // Generic
    void clearScreen(uint16_t color = GxEPD_WHITE);
    void drawCenteredText(int y, const String& text, const uint8_t* font);
    void drawTitleBar(const String& title);

    // For external custom drawing if needed
    U8G2_FOR_ADAFRUIT_GFX& getFonts();

private:
    DisplayManager& display;
};
