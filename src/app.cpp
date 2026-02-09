#include "app.h"
#include <WiFi.h>
#include <Wire.h>
#include <driver/rtc_io.h>
#include "board_def.h"
#include "states/app_menu_state.h"
#include "states/app_terminal_state.h"
#include "states/app_locked_state.h"

App::App() 
    : _ui(_display), _wifi(_terminal, _keyboard, _ui, _power), _ota(_display), _currentState(nullptr), _nextState(nullptr), _lastAniUpdate(0), _lastScreenRefresh(0), _refreshPending(false) {
    _settingsController.reset(new SettingsController(*this));
    _connectionController.reset(new ConnectionController(*this));
    _scriptController.reset(new ScriptController(*this));
}

App::~App() {
    // Resources automatically released by unique_ptr
}

void App::setup() {
    initializeHardware();
    
    _security.begin();
    
    // Setup WiFi (Scan or Connect)
    _wifi.setSecurityManager(&_security);
    _wifi.setRenderCallback([this]() { this->requestRefresh(); });

    _wifi.connectAsync(); 
    
    _ota.begin();
    
    if (_storage.begin()) {
        _ui.updateBootStatus("Storage OK");
    } else {
        _ui.updateBootStatus("Storage FAIL");
        Serial.println("SD Card Mount Failed");
    }

    _serverManager.setSecurityManager(&_security);
    _serverManager.begin();
    _menu.reset(new MenuSystem(_ui)); // Use reset for compatibility
    
    // Start with Locked State instead of Menu
    changeState(new AppLockedState());

    Serial.println("Setup complete, entering lock screen...");
}

void App::initializeHardware() {
    // Serial debugging
    if (DEBUG_SERIAL_ENABLED) {
        Serial.begin(DEBUG_SERIAL_BAUD);
        delay(100);
        Serial.println("\n\n=================================");
        Serial.println("SshDeck SSH Terminal");
        Serial.println("=================================\n");
    }

    pinMode(BOARD_BOOT_PIN, INPUT_PULLUP);

    // Initialize power pins
    Serial.println("Initializing power...");
    pinMode(BOARD_POWERON, OUTPUT);
    digitalWrite(BOARD_POWERON, HIGH);
    
    if (BOARD_1V8_EN >= 0) { pinMode(BOARD_1V8_EN, OUTPUT); digitalWrite(BOARD_1V8_EN, HIGH); }
    if (BOARD_GPS_EN >= 0) { pinMode(BOARD_GPS_EN, OUTPUT); digitalWrite(BOARD_GPS_EN, HIGH); }
    if (BOARD_6609_EN >= 0) { pinMode(BOARD_6609_EN, OUTPUT); digitalWrite(BOARD_6609_EN, HIGH); }
    
    // Backlight is initialized by KeyboardManager via PWM (LEDC channel 1).
    // No need to configure GPIO here — ledcAttachPin takes ownership.

    // Disable SPI devices
    pinMode(BOARD_SD_CS, OUTPUT);
    digitalWrite(BOARD_SD_CS, HIGH);
    pinMode(BOARD_LORA_CS, OUTPUT);
    digitalWrite(BOARD_LORA_CS, HIGH);
    
    // Put LoRa SX1262 into sleep mode to save ~600µA.
    // Send SetSleep(0x00) command: sleep with cold start (lowest power).
    SPI.begin(BOARD_SPI_SCK, BOARD_SPI_MISO, BOARD_SPI_MOSI);
    digitalWrite(BOARD_LORA_CS, LOW);
    SPI.transfer(0x84); // SetSleep opcode
    SPI.transfer(0x00); // Cold start (RC oscillator off)
    digitalWrite(BOARD_LORA_CS, HIGH);
    
    delay(1500); // Stabilization
    
    // Reduce CPU frequency from 240MHz to 80MHz to save ~30-40mA.
    // SSH crypto is handled by LibSSH in a separate task and remains fast enough.
    // E-ink SPI is also well within 80MHz capability.
    setCpuFrequencyMhz(80);
    
    // Initialize I2C and Fuel Gauge
    Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL, 100000); // Force 100kHz
    
    _power.begin(Wire);
    _touch.begin(Wire);

    if (!_display.begin()) {
        Serial.println("Display init failed!");
        delay(5000);
        esp_restart();
    }
    
    // Welcome Screen
    _ui.drawBootScreen("SshDeck", "Initializing...");
    
    if (!_keyboard.begin()) {
        _ui.updateBootStatus("Keyboard FAIL!");
        Serial.println("Keyboard init failed!");
        delay(5000);
        esp_restart();
    }
    _ui.updateBootStatus("Keyboard OK");
}

InputEvent App::pollInputs() {
    // Note: keyboard.loop() for power button is already called in App::loop(),
    // no need to call it again here. Just check for queued key events.
    if (_keyboard.getSystemEvent() == SYS_EVENT_SLEEP) {
        return InputEvent(EVENT_SYSTEM, SYS_EVENT_SLEEP);
    }
    if (_keyboard.available()) {
        return InputEvent(_keyboard.getKeyChar());
    }
    return InputEvent();
}

void App::changeState(AppState* newState) {
    _nextState = newState;
}

void App::loop() {
    // Process pending state transition safely at start of loop
    if (_nextState) {
        if (_currentState) {
            _currentState->exit(*this);
        }
        _currentState.reset(_nextState);
        _nextState = nullptr;
        if (_currentState) {
            _currentState->enter(*this);
        }
    }

    _ota.loop();
    _wifi.loop();
    
    // Always poll the power button, regardless of keyboard queue state.
    // keyboard.loop() reads the BOOT_PIN GPIO to detect held-down power button.
    _keyboard.loop();
    if (_keyboard.getSystemEvent() == SYS_EVENT_SLEEP) {
        enterDeepSleep();
        return;
    }
    
    // Process pending refresh requests (debounced)
    if (_refreshPending) {
        _refreshPending = false;
        if (_currentState) {
            _currentState->onRefresh(*this);
        }
    }
    
    // Battery status update: read I2C only every 5 seconds instead of every 1s.
    // BQ27220 updates its registers at ~1Hz internally, but for a status bar
    // percentage display, 5s granularity is more than sufficient.
    // This saves ~8 I2C transactions per 5s window (was 10, now 2).
    static unsigned long lastStatusUpdate = 0;
    if (millis() - lastStatusUpdate > 5000) {
        _ui.updateStatusState(_power.getPercentage(), _power.isCharging(), WiFi.status() == WL_CONNECTED);
        lastStatusUpdate = millis();
    }
    
    // Delegate to Current State
    if (_currentState) {
        _currentState->update(*this);
    }
}

void App::handleMainMenu() {
    std::vector<String> items = {
        "Saved Servers",
        "Quick Connect",
        "Custom Commands",
        "Settings",
        "Power Off"
    };
    _menu->showMenu("Main Menu", items, [this](int choice) {
        if (choice == 0) _connectionController->showSavedServers();
        else if (choice == 1) _connectionController->showQuickConnect();
        else if (choice == 2) _scriptController->showScriptMenu();
        else if (choice == 3) _settingsController->showSettingsMenu();
        else if (choice == 4) enterDeepSleep();
    });
}




void App::enterDeepSleep() {
    Serial.println("Shutting down...");
    
    _ui.drawShutdownScreen();

    delay(1000);

    pinMode(BOARD_BOOT_PIN, INPUT_PULLUP);
    while(digitalRead(BOARD_BOOT_PIN) == LOW) {
        delay(50);
    }

    _display.getDisplay().powerOff();
    
    ledcWrite(1, 0); // Backlight PWM off
    if (BOARD_1V8_EN >= 0) digitalWrite(BOARD_1V8_EN, LOW);
    if (BOARD_GPS_EN >= 0) digitalWrite(BOARD_GPS_EN, LOW);
    if (BOARD_6609_EN >= 0) digitalWrite(BOARD_6609_EN, LOW);
    digitalWrite(BOARD_POWERON, LOW);

    esp_sleep_enable_ext0_wakeup((gpio_num_t)BOARD_BOOT_PIN, 0); 
    rtc_gpio_pullup_en((gpio_num_t)BOARD_BOOT_PIN);
    rtc_gpio_pulldown_dis((gpio_num_t)BOARD_BOOT_PIN);

    esp_deep_sleep_start();
}

void App::checkSystemInput() {
    _keyboard.loop();
    if (_keyboard.getSystemEvent() == SYS_EVENT_SLEEP) {
        enterDeepSleep();
    }
}

void App::requestRefresh() {
    _refreshPending = true;
}

void App::drawTerminalScreen(bool partial) {
    // Thread-safe display update with mutex
    _display.lock();
    _terminal.lock();
    
    // Construct Title for status bar
    String title = "";
    if (WiFi.status() == WL_CONNECTED) {
        title = WiFi.SSID();
        if (title.length() > 8) title = title.substring(0, 8);
    } else {
        title = "Offline";
    }
    
    if (_sshClient && _sshClient->isConnected()) {
        title += " > ";
        String host = _sshClient->getConnectedHost();
        if (host.length() > 10) host = host.substring(0, 10);
        title += host;
    }
    
    // Show scroll indicator when viewing history
    if (_terminal.isViewingHistory()) {
        title = "[+" + String(_terminal.getViewOffset()) + "] " + title;
    }

    _ui.drawTerminal(_terminal, title, _power.getPercentage(), _power.isCharging(), WiFi.status() == WL_CONNECTED, partial);
    _terminal.clearUpdateFlag();
    
    _terminal.unlock();
    _display.unlock();
}

void App::showHelpScreen() {
    _display.lock();
    _ui.drawHelpScreen();
    _display.unlock();
    
    while(true) {
        if(_keyboard.isKeyPressed()) {
             _keyboard.getKeyChar(); 
             break;
        }
        delay(50);
    }
    drawTerminalScreen();
}

void App::connectToServer(const String& host, int port, const String& user, const String& pass, const String& name, const String& script) {
    // Show connecting message first (non-blocking render, but next steps might block network)
    // We can't easily make LibSSH non-blocking in this codebase without major rewrite.
    // So we accept that the UI freezes during "Connecting..."
    
    _ui.drawMessage("Connecting...", "To: " + name); 
    // Force display update since we might block immediately after
    
    // unique_ptr automatically deletes the old object when assigned a new one
    _sshClient.reset(new SSHClient(_terminal, _keyboard));
    
    if (!_sshClient) {
        _menu->showMessage("Error", "Alloc Failed", [this](){ handleMainMenu(); });
        return;
    }
    // Use lambdas for callbacks
    _sshClient->setRefreshCallback([this]() { this->requestRefresh(); });
    _sshClient->setHelpCallback([this]() { this->showHelpScreen(); });
    if (script.length() > 0) {
        _sshClient->setStartupCommand(script);
    }

    if (WiFi.status() != WL_CONNECTED) {
            _wifi.setIdleCallback([this]() { 
                _keyboard.loop();
                if (_keyboard.getSystemEvent() == SYS_EVENT_SLEEP) {
                    enterDeepSleep();
                }
            });
            if (!_wifi.connect()) {
                _menu->showMessage("Error", "WiFi Failed", [this](){ handleMainMenu(); });
                return;
            }
    }

    String privKey = _security.getSSHKey();
    const char* keyData = (privKey.length() > 0) ? privKey.c_str() : nullptr;

    _sshClient->connectSSH(host.c_str(), port, user.c_str(), pass.c_str(), keyData);
    changeState(new AppTerminalState());
}


