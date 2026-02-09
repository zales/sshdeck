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
#include "touch_manager.h"
#include "app_state.h"
#include "system_context.h"
#include "controllers/settings_controller.h"
#include "controllers/connection_controller.h"
#include "controllers/script_controller.h"

// Forward declare states
class AppMenuState;
class AppTerminalState;

class App : public ISystemContext {
public:
    App();
    ~App();
    void setup();
    void loop();

    // State Management
    void changeState(AppState* newState);

    // Helpers
    void initializeHardware();
    void enterDeepSleep();
    void checkSystemInput();

    void requestRefresh(); // Thread-safe refresh request

    // Event Loop Logic
    InputEvent pollInputs();

    // UI & Logic
    void drawTerminalScreen(bool partial = true);
    void showHelpScreen();

    // Menu Handlers
    void handleMainMenu() override;
    void connectToServer(const String& host, int port, const String& user, const String& pass, const String& name, const String& script = "") override;

    // Getters (ISystemContext implementation)
    DisplayManager& display() override { return _display; }
    KeyboardManager& keyboard() override { return _keyboard; }
    UIManager& ui() override { return _ui; }
    TerminalEmulator& terminal() override { return _terminal; }
    PowerManager& power() override { return _power; }
    ServerManager& serverManager() override { return _serverManager; }
    StorageManager& storage() override { return _storage; }
    WifiManager& wifi() override { return _wifi; }
    MenuSystem* menu() override { return _menu.get(); }
    SSHClient* sshClient() override { return _sshClient.get(); }
    SecurityManager& security() override { return _security; }
    OtaManager& ota() override { return _ota; }
    TouchManager& touch() override { return _touch; }

    SettingsController& settingsController() { return *_settingsController; }
    ConnectionController& connectionController() { return *_connectionController; }
    ScriptController& scriptController() { return *_scriptController; }

    // Special setters for unique_ptrs
    void resetMenu(MenuSystem* m) { _menu.reset(m); }
    void resetSshClient(SSHClient* s) { _sshClient.reset(s); }

private:
    // Subsystems
    DisplayManager _display;
    KeyboardManager _keyboard;
    UIManager _ui;
    TerminalEmulator _terminal;
    PowerManager _power;
    TouchManager _touch;
    ServerManager _serverManager;
    StorageManager _storage;
    WifiManager _wifi;
    std::unique_ptr<MenuSystem> _menu;
    std::unique_ptr<SSHClient> _sshClient;
    SecurityManager _security;
    OtaManager _ota;
    
    // Controllers
    std::unique_ptr<SettingsController> _settingsController;
    std::unique_ptr<ConnectionController> _connectionController;
    std::unique_ptr<ScriptController> _scriptController;

    // State
    std::unique_ptr<AppState> _currentState;
    AppState* _nextState; // Pending state transition
    unsigned long _lastAniUpdate;
    unsigned long _lastScreenRefresh;
    volatile bool _refreshPending;
};
