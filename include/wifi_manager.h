#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "terminal_emulator.h"
#include "keyboard_manager.h"
#include "display_manager.h"
// #include "touch_manager.h"

#include <functional>

class WifiManager {
public:
    WifiManager(TerminalEmulator& term, KeyboardManager& kb, DisplayManager& disp);
    
    void setIdleCallback(std::function<void()> cb);

    // Attempt to connect using saved credentials or user selection
    // Returns true if successfully connected
    bool connect(); 
    
    // Manage saved networks (List, Edit, Delete)
    void manage();

private:
    void deleteCredential(int index);
std::function<void()> idleCallback;
    
    TerminalEmulator& terminal;
    KeyboardManager& keyboard;
    DisplayManager& display;
    // TouchManager& touch;
    Preferences preferences;
    
    struct WifiCreds {
        String ssid;
        String pass;
    };
    
    // Support up to 5 saved networks
    static const int MAX_SAVED_NETWORKS = 5;
    WifiCreds savedNetworks[MAX_SAVED_NETWORKS];
    int lastUsedIndex = -1;
    int maxSavedIndex = 0;
    
    void loadCredentials();
    void saveCredentials(const String& ssid, const String& pass);
    bool tryConnect(const String& ssid, const String& pass);
    void scanAndSelect();
    String readInput(bool passwordMask = false);
    String readInput(const String& prompt, bool passwordMask);
    
    // Helper to refresh display during input loops
    void refreshScreen();
};
