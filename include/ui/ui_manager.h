#pragma once

#include "../display_manager.h"
#include <Arduino.h>
#include <vector>

class DisplayManager;
class TerminalEmulator;

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
    
    // Wifi related screens
    void drawAutoConnectScreen(const String& ssid, int remainingSeconds, int batteryPercent, bool isCharging);
    void drawScanningScreen(int batteryPercent, bool isCharging);
    void drawConnectingScreen(const String& ssid, const String& password, int batteryPercent, bool isCharging);

    // Elements
    void drawStatusBar(const String& title, bool wifiConnected, int batteryPercent, bool isCharging);
    
    // Generic
    void drawMenu(const String& title, const std::vector<String>& items, int selectedIndex);
    void drawInputScreen(const String& title, const String& currentText, bool isPassword = false, bool textOnly = false);
    void drawTerminal(const TerminalEmulator& term, const String& statusTitle, int batteryPercent, bool isCharging, bool wifiConnected);
    void drawHelpScreen();

    void clearScreen(uint16_t color = GxEPD_WHITE);
    void drawCenteredText(int y, const String& text, const uint8_t* font);
    void drawTitleBar(const String& title);

    // For external custom drawing if needed
    U8G2_FOR_ADAFRUIT_GFX& getFonts();

    // Advanced Generic Renders
    void render(std::function<void(U8G2_FOR_ADAFRUIT_GFX&)> drawCallback);
    void setRefreshMode(bool partial);
    
    // Primitives
    void drawHeader(const String& title);
    void drawFooter(const String& message);
    void drawTextLine(int x, int y, const String& text, const uint8_t* font = nullptr, bool invert = false);
    void drawLabelValue(int y, const String& label, const String& value);
    
    // Helpers
    void fillRect(int x, int y, int w, int h, uint16_t color);
    void drawRect(int x, int y, int w, int h, uint16_t color);
    void drawFastHLine(int x, int y, int w, uint16_t color);

    void updateStatusState(int bat, bool charging, bool wifi);

private:
    DisplayManager& display;
    
    int currentBat = 0;
    bool currentCharging = false;
    bool currentWifi = false;
};

// UI Layout Helper for automatic positioning
class UILayout {
public:
    UILayout(UIManager& mgr, const String& title);
    
    void addText(const String& text);
    void addItem(const String& label, const String& value);
    void addFooter(const String& text);
    
    // Manual spacing control if needed
    void space(int pixels);
    int getY() const { return currentY; }

private:
    UIManager& ui;
    int currentY;
};
    void setRefreshMode(bool partial);
