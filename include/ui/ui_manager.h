#pragma once

#include "../display_manager.h"
#include <Arduino.h>
#include <vector>

class DisplayManager;
class TerminalEmulator;

// --- Screen Refresh Modes ---
// Controls how the display is updated and which regions are refreshed.
// The header (status bar, 16px) is a PROTECTED ZONE — it is only drawn
// during FULL mode. All other modes operate below the header to prevent
// e-ink partial refresh degradation (fading of black pixels).
enum ScreenMode {
    SCREEN_FULL,       // Full refresh, entire screen including header. Use for state transitions.
    SCREEN_CONTENT,    // Partial refresh, content area only (y >= HEADER_H). Header untouched.
    SCREEN_OVERLAY,    // Full refresh, entire screen but no header drawn. For special screens (help, shutdown).
};

// Layout constants
static const int HEADER_H    = 16;   // Status bar height
static const int CONTENT_Y   = 18;   // First usable Y for content (below header + 2px gap)
static const int FOOTER_H    = 16;   // Footer bar height
static const int LINE_H      = 16;   // Standard line height for menus/lists

class UIManager {
public:
    UIManager(DisplayManager& display);

    // Helpers to get dimensions dynamically
    int width() const;
    int height() const;
    
    // --- Screen lifecycle ---
    // Call beginScreen() before drawing, endScreen() after.
    // This ensures correct refresh mode, partial windows, and header protection.
    //   SCREEN_FULL:    full refresh, draws header. For state transitions.
    //   SCREEN_CONTENT: partial refresh below header. For updates within a state.
    //   SCREEN_OVERLAY: full refresh, no header. For special screens (help, boot, shutdown).
    void beginScreen(ScreenMode mode, const String& headerTitle = "");
    void endScreen();
    
    // Convenience: set mode + render with auto-header in one call.
    // This is the PREFERRED way to draw a screen.
    // Header is drawn automatically for FULL/CONTENT modes.
    void renderScreen(ScreenMode mode, const String& headerTitle,
                      std::function<void(U8G2_FOR_ADAFRUIT_GFX&)> drawContent);
    
    // Content area dimensions (below header, above footer)
    int contentWidth() const;
    int contentHeight() const;
    int contentTop() const { return CONTENT_Y; }
    int contentBottom() const { return height() - FOOTER_H; }

    // Standard Screens
    void drawPinEntry(const String& title, const String& subtitle, const String& entry, bool isWrong = false, bool fullRefresh = false);
    void drawMessage(const String& title, const String& message, bool partial = false);
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
    void drawMenu(const String& title, const std::vector<String>& items, int selectedIndex, bool navOnly = false);
    void drawInputScreen(const String& title, const String& currentText, bool isPassword = false, bool textOnly = false);
    void drawTerminal(const TerminalEmulator& term, const String& statusTitle, int batteryPercent, bool isCharging, bool wifiConnected, bool partial = true);
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
    
    // beginScreen/endScreen state
    ScreenMode _currentMode = SCREEN_FULL;
    String _currentHeaderTitle = "";
    bool _screenActive = false;
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
