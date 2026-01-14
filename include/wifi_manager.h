#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "security_manager.h"
#include "terminal_emulator.h"
#include "keyboard_manager.h"
#include "ui/ui_manager.h"
#include "power_manager.h"
#include <functional>

class WifiManager {
public:
    WifiManager(TerminalEmulator& term, KeyboardManager& kb, UIManager& ui, PowerManager& pwr);
    
    void setSecurityManager(SecurityManager* sec);
    
    void setIdleCallback(std::function<void()> cb);
    std::function<void()> getIdleCallback() const { return idleCallback; }
    void setRenderCallback(std::function<void()> cb);

    // Attempt to connect using saved credentials (auto-connect)
    bool connect(); 
    void connectAsync(); // Non-blocking auto-connect start
    
    // Polling for async status updates
    void loop();

    struct WifiScanResult {
        String ssid;
        int rssi;
        bool secure;
    };

    // Core Logic (Public for App UI)
    std::vector<WifiScanResult> scan(); 
    bool connectTo(const String& ssid, const String& pass);
    void save(const String& ssid, const String& pass);
    void forget(int index);
    void reEncryptAll();

    struct WifiInfo {
        String ssid;
        String pass; // Decrypted or raw
    };
    std::vector<WifiInfo> getSavedNetworks();

private:
   
    std::function<void()> idleCallback;
    std::function<void()> renderCallback;
    
    TerminalEmulator& terminal;
    KeyboardManager& keyboard;
    UIManager& ui;
    PowerManager& power;
    Preferences preferences;
    SecurityManager* security;
    
    void loadCredentials();
    void saveCredentials(const String& ssid, const String& pass);
    void deleteCredential(int index);
    
    // Helper to refresh screen if needed (mostly unnecessary in new arch)
    void refreshScreen();

    struct WifiCreds {
        String ssid;
        String pass;
    };
    
    // Support up to 5 saved networks
    static const int MAX_SAVED_NETWORKS = 5;
    WifiCreds savedNetworks[MAX_SAVED_NETWORKS];
    int maxSavedIndex = 0;
    int lastUsedIndex = -1;
    
    wl_status_t _lastStatus = WL_DISCONNECTED;
};

