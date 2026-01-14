#pragma once

#include <Arduino.h>
#include <vector>
#include <memory>
#include "config.h"
#include "display_manager.h"
#include "keyboard_manager.h"
#include "terminal_emulator.h"
#include "ssh_client.h"
#include "server_manager.h"
#include "ui/menu_system.h"
#include "ui/ui_manager.h"
#include "wifi_manager.h"
#include "security_manager.h"
#include "storage_manager.h"
#include "ota_manager.h"
#include "power_manager.h"
#include "app_state.h"

// Forward declare states
class AppMenuState;
class AppTerminalState;

class App {
public:
    App();
    ~App();
    void setup();
    void loop();

    // State Management
    void changeState(AppState* newState);

    // Subsystems (Public for States)
    DisplayManager display;
    KeyboardManager keyboard;
    UIManager ui;
    TerminalEmulator terminal;
    PowerManager power;
    ServerManager serverManager;
    StorageManager storage;
    WifiManager wifi;
    std::unique_ptr<MenuSystem> menu;
    std::unique_ptr<SSHClient> sshClient;
    SecurityManager security;
    OtaManager ota;

    
    // State
    std::unique_ptr<AppState> currentState;
    unsigned long lastAniUpdate;
    unsigned long lastScreenRefresh;
    volatile bool refreshPending;

public:

    // Helper Methods
    void initializeHardware();
    void enterDeepSleep();
    void checkSystemInput();

    void unlockSystem();
    void requestRefresh(); // Thread-safe refresh request

    // Event Loop Logic
    InputEvent pollInputs();

    // UI & Logic
    void drawTerminalScreen(bool partial = true);
    void showHelpScreen();

    // Menu Handlers
    void handleMainMenu();
    void handleSavedServers();
    void handleQuickConnect();
    void handleSettings();
    void handleBatteryInfo();
    void handleChangePin();
    void handleStorage();
    void handleSystemUpdate();
    void handleWifiMenu();
    void exitStorageMode();
    void connectToServer(const String& host, int port, const String& user, const String& pass, const String& name);
};
