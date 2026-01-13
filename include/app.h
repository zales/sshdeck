#pragma once

#include <Arduino.h>
#include <vector>
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

enum AppState {
    STATE_MENU,
    STATE_TERMINAL
};

class App {
public:
    App();
    void setup();
    void loop();

private:
    // Subsystems
    DisplayManager display;
    KeyboardManager keyboard;
    UIManager ui;
    TerminalEmulator terminal;
    ServerManager serverManager;
    StorageManager storage;
    WifiManager wifi;
    MenuSystem* menu;
    SSHClient* sshClient;
    SecurityManager security;
    OtaManager ota;

    // State
    AppState currentState;
    unsigned long pwrBtnStart;

    // Helper Methods
    void initializeHardware();
    void enterDeepSleep();
    void checkPowerButton();
    float getBatteryVoltage();
    int getBatteryPercentage();
    bool isCharging();
    void unlockSystem();

    // UI & Logic
    void drawTerminalScreen();
    void showHelpScreen();

    // Menu Handlers
    void handleMainMenu();
    void handleSavedServers();
    void handleQuickConnect();
    void handleSettings();
    void handleChangePin();
    void handleStorage();
    void handleSystemUpdate();
    void exitStorageMode();
    void connectToServer(const String& host, int port, const String& user, const String& pass, const String& name);
};
