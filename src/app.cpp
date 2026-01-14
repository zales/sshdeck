#include "app.h"
#include <WiFi.h>
#include <Wire.h>
#include <driver/rtc_io.h>
#include "board_def.h"

App::App() 
    : wifi(terminal, keyboard, ui, power), ui(display), ota(display), currentState(STATE_MENU), lastAniUpdate(0), lastScreenRefresh(0), refreshPending(false) {
}

App::~App() {
    // Resources automatically released by unique_ptr
}

void App::setup() {
    initializeHardware();
    
    security.begin();
    unlockSystem();

    // Setup WiFi (Scan or Connect)
    wifi.setSecurityManager(&security);
    wifi.setRenderCallback([this]() { this->requestRefresh(); });
    wifi.connect(); 
    
    ota.begin();

    serverManager.setSecurityManager(&security);
    serverManager.begin();
    menu.reset(new MenuSystem(ui)); // Use reset for compatibility
    
    Serial.println("Setup complete, entering menu...");
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
    
    // Backlight handled by KeyboardManager (keep off during boot)
    pinMode(BOARD_KEYBOARD_LED, OUTPUT);
    digitalWrite(BOARD_KEYBOARD_LED, LOW);

    // Disable SPI devices
    pinMode(BOARD_SD_CS, OUTPUT);
    digitalWrite(BOARD_SD_CS, HIGH);
    pinMode(BOARD_LORA_CS, OUTPUT);
    digitalWrite(BOARD_LORA_CS, HIGH);
    
    delay(1500); // Stabilization
    
    // Initialize I2C and Fuel Gauge
    Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL, 100000); // Force 100kHz
    
    power.begin(Wire);

    if (!display.begin()) {
        Serial.println("Display init failed!");
        delay(5000);
        esp_restart();
    }
    
    // Welcome Screen
    ui.drawBootScreen("SshDeck", "Initializing...");
    
    // terminal.appendString("Init System...\n");
    // drawTerminalScreen();
    
    if (!keyboard.begin()) {
        ui.updateBootStatus("Keyboard FAIL!");
        Serial.println("Keyboard init failed!");
        delay(5000);
        esp_restart();
    }
    ui.updateBootStatus("Keyboard OK");
}

InputEvent App::pollInputs() {
    keyboard.loop();
    if (keyboard.getSystemEvent() == SYS_EVENT_SLEEP) {
        return InputEvent(EVENT_SYSTEM, SYS_EVENT_SLEEP);
    }
    if (keyboard.available()) {
        return InputEvent(keyboard.getKeyChar());
    }
    return InputEvent();
}

void App::loop() {
    ota.loop();
    
    // Process pending refresh requests (debounced)
    if (refreshPending) {
        refreshPending = false;
        drawTerminalScreen();
    }
    
    // Status update logic
    static unsigned long lastStatusUpdate = 0;
    if (millis() - lastStatusUpdate > 1000) {
        ui.updateStatusState(power.getPercentage(), power.isCharging(), WiFi.status() == WL_CONNECTED);
        lastStatusUpdate = millis();
    }
    
    InputEvent event = pollInputs();
    if (event.type == EVENT_SYSTEM && event.systemCode == SYS_EVENT_SLEEP) {
        enterDeepSleep();
        return;
    }

    if (currentState == STATE_MENU) {
        if (!menu) {
            Serial.println("FATAL: menu is NULL in STATE_MENU!");
            ESP.restart();
            return;
        }
        
        if (!menu->isRunning()) {
            handleMainMenu();
        }
        
        if (menu->handleInput(event)) {
             // UI updated internally
        }
        
        if (!menu->isRunning() && menu->isConfirmed()) {
             handleMainMenuSelection(menu->getSelection());
        }
    } else {
        // STATE_TERMINAL
        if (sshClient && sshClient->isConnected()) {
            sshClient->process();
            
            // Forward input to SSH client
            if (sshClient && event.type == EVENT_KEY_PRESS && event.key != 0) {
                sshClient->write(event.key);
            }
            
            bool forceRedraw = false;
            // Battery Charging Animation (every 1 sec)
            if (power.isCharging() && (millis() - lastAniUpdate > 1000)) {
                lastAniUpdate = millis();
                forceRedraw = true;
            }

            if ((terminal.needsUpdate() || forceRedraw) && (millis() - lastScreenRefresh > DISPLAY_UPDATE_INTERVAL_MS)) {
                drawTerminalScreen();
                lastScreenRefresh = millis();
            }
            if (!sshClient->isConnected()) {
                currentState = STATE_MENU;
                ui.drawMessage("Disconnected", "Session Ended");
                delay(1000); // Give user time to see
            }
        } else {
            currentState = STATE_MENU;
        }
        delay(5); 
    }
}

void App::handleMainMenu() {
    std::vector<String> items = {
        "Saved Servers",
        "Quick Connect",
        "Settings",
        "Power Off"
    };
    menu->showMenu("Main Menu", items);
}

void App::handleMainMenuSelection(int choice) {
    if (choice == 0) {
        handleSavedServers();
    } else if (choice == 1) {
        handleQuickConnect();
    } else if (choice == 2) {
        handleSettings();
    } else if (choice == 3) {
        enterDeepSleep();
    }
}


void App::handleSystemUpdate() {
    if (WiFi.status() != WL_CONNECTED) {
        ui.drawMessage("Error", "No WiFi Connection");
        delay(2000);
        return;
    }

    String url = UPDATE_SERVER_URL; 
    // If URL ends in .bin, we try to find a manifest .json instead if user wants full list
    // Or we simply define MANIFEST_URL in config. 
    // For now, let's construct manifest URL from base if possible, or just hardcode convention.
    String manifestUrl = url;
    manifestUrl.replace("firmware.bin", "firmware.json");
    
    String msg = String(APP_VERSION);
    if (!msg.startsWith("v")) msg = "v" + msg;
    ui.drawMessage("Checking...", msg);
    
    UpdateManifest manifest = ota.fetchManifest(manifestUrl, UPDATE_ROOT_CA);
    
    if (manifest.versions.empty()) {
         // Fallback to legacy check if manifest request failed
         String newVer = ota.checkUpdateAvailable(url, APP_VERSION, UPDATE_ROOT_CA);
         if (newVer == "") {
            std::vector<String> opts = {"Reinstall?", "Cancel"};
            if (menu->showMenuBlocking("No update found", opts, keyboard) != 0) return;
            ota.updateFromUrl(url, UPDATE_ROOT_CA);
         } else {
             // Logic for simple update
             ota.updateFromUrl(url, UPDATE_ROOT_CA);
         }
         return;
    }

    // Show version selection menu
    std::vector<String> options;
    std::vector<String> urls;
    
    for (const auto& v : manifest.versions) {
        String label = v.version;
        if (v.version == String(APP_VERSION)) label += " (Curr)";
        if (v.version == manifest.latestVersion) label += " *";
        options.push_back(label);
        urls.push_back(v.url);
    }
    options.push_back("Back");

    while (true) {
        int selected = menu->showMenuBlocking("Select Version", options, keyboard);
        if (selected < 0 || selected >= urls.size()) return; // Back or cancelled

        String targetUrl = urls[selected];
        String targetVer = manifest.versions[selected].version;
        
        // Confirmation
        std::vector<String> confirmOpts = {"Yes, Flash it", "No"};
        if (menu->showMenuBlocking("Flash v" + targetVer + "?", confirmOpts, keyboard) == 0) {
            ota.updateFromUrl(targetUrl, UPDATE_ROOT_CA);
            // If update fails, it returns here
            ui.drawMessage("Error", "Update Failed");
            delay(2000);
        }
    }
}

void App::enterDeepSleep() {
    Serial.println("Shutting down...");
    
    ui.drawShutdownScreen();

    delay(1000);

    pinMode(BOARD_BOOT_PIN, INPUT_PULLUP);
    while(digitalRead(BOARD_BOOT_PIN) == LOW) {
        delay(50);
    }

    display.getDisplay().powerOff();
    
    digitalWrite(BOARD_KEYBOARD_LED, LOW);
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
    keyboard.loop();
    if (keyboard.getSystemEvent() == SYS_EVENT_SLEEP) {
        enterDeepSleep();
    }
}

void App::requestRefresh() {
    refreshPending = true;
}

void App::drawTerminalScreen() {
    // Thread-safe display update with mutex
    if (displayMutex && xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100))) {
        // Construct Title for status bar
        String title = "";
        if (WiFi.status() == WL_CONNECTED) {
            title = WiFi.SSID();
            if (title.length() > 8) title = title.substring(0, 8);
        } else {
            title = "Offline";
        }
        
        if (sshClient && sshClient->isConnected()) {
            title += " > ";
            String host = sshClient->getConnectedHost();
            if (host.length() > 10) host = host.substring(0, 10);
            title += host;
        }

        ui.drawTerminal(terminal, title, power.getPercentage(), power.isCharging(), WiFi.status() == WL_CONNECTED);
        terminal.clearUpdateFlag();
        
        xSemaphoreGive(displayMutex);
    }
}

void App::showHelpScreen() {
    if (displayMutex && xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100))) {
        ui.drawHelpScreen();
        xSemaphoreGive(displayMutex);
    }
    
    while(true) {
        if(keyboard.isKeyPressed()) {
             keyboard.getKeyChar(); 
             break;
        }
        delay(50);
    }
    drawTerminalScreen();
}

void App::connectToServer(const String& host, int port, const String& user, const String& pass, const String& name) {
    terminal.clear();
    terminal.appendString(("Connecting to " + name + "...\n").c_str());
    
    // unique_ptr automatically deletes the old object when assigned a new one
    sshClient.reset(new SSHClient(terminal, keyboard));
    
    if (!sshClient) {
        terminal.appendString("ERROR: Failed to allocate SSH client\n");
        return;
    }
    // Use lambdas for callbacks
    sshClient->setRefreshCallback([this]() { this->requestRefresh(); });
    sshClient->setHelpCallback([this]() { this->showHelpScreen(); });

    if (WiFi.status() != WL_CONNECTED) {
            wifi.setIdleCallback([this]() { 
                keyboard.loop();
                if (keyboard.getSystemEvent() == SYS_EVENT_SLEEP) {
                    enterDeepSleep();
                }
            });
            if (!wifi.connect()) {
                menu->showMessageBlocking("Error", "WiFi Failed", keyboard);
                return;
            }
    }

    String privKey = security.getSSHKey();
    const char* keyData = (privKey.length() > 0) ? privKey.c_str() : nullptr;

    if (sshClient->connectSSH(host.c_str(), port, user.c_str(), pass.c_str(), keyData)) {
        currentState = STATE_TERMINAL;
    } else {
            // Append instruction to terminal log
            terminal.appendString("\n[Press any key to Exit]");
            
            // Ensure the last log messages are visible
            drawTerminalScreen();
            
            // 1. Clear any buffered/lingering keystrokes from the selection action
            unsigned long clearStart = millis();
            while (millis() - clearStart < 500) { 
                if (keyboard.isKeyPressed()) keyboard.getKeyChar();
                delay(10);
            }
            
            // 2. Wait for a NEW explicit key press
            while(!keyboard.isKeyPressed()) {
                delay(10);
            }
            keyboard.getKeyChar(); // consume key

            // Optional: Don't show the "Error" popup if the log was informative enough?
            // But keeping it for consistency.
            menu->showMessageBlocking("Error", "SSH Failed", keyboard);
    }
}

void App::handleSavedServers() {
    while (true) {
        std::vector<String> items;
        for (const auto& s : serverManager.getServers()) {
            items.push_back(s.name);
        }
        items.push_back("[ Add New Server ]");

        int choice = menu->showMenuBlocking("Saved Servers", items, keyboard);
        
        if (choice < 0) return; // Back
        
        if (choice == items.size() - 1) {
            ServerConfig newSrv;
            String p = "22";
            if (menu->textInputBlocking("Name", newSrv.name, keyboard) && 
                menu->textInputBlocking("Host", newSrv.host, keyboard) &&
                menu->textInputBlocking("User", newSrv.user, keyboard) &&
                menu->textInputBlocking("Port", p, keyboard) &&
                menu->textInputBlocking("Password", newSrv.password, keyboard)) {
                    newSrv.port = p.toInt();
                    serverManager.addServer(newSrv);
            }
        } else {
            ServerConfig selected = serverManager.getServer(choice);
            std::vector<String> actions = {"Connect", "Edit", "Delete"};
            int action = menu->showMenuBlocking(selected.name, actions, keyboard);

            if (action == 0) { // Connect
                connectToServer(selected.host, selected.port, selected.user, selected.password, selected.name);
                return;
            } else if (action == 1) { // Edit
                 String p = String(selected.port);
                 menu->textInputBlocking("Name", selected.name, keyboard);
                 menu->textInputBlocking("Host", selected.host, keyboard);
                 menu->textInputBlocking("User", selected.user, keyboard);
                 menu->textInputBlocking("Port", p, keyboard);
                 selected.port = p.toInt();
                 menu->textInputBlocking("Password", selected.password, keyboard);
                 serverManager.updateServer(choice, selected);
            } else if (action == 2) { // Delete
                 std::vector<String> yn = {"No", "Yes"};
                 if (menu->showMenuBlocking("Delete?", yn, keyboard) == 1) {
                    serverManager.removeServer(choice);
                 }
            }
        }
    }
}

void App::handleQuickConnect() {
     ServerConfig s;
     s.name = "Quick Connect";
     s.port = 22;
     String p = "22";
     
     if (menu->textInputBlocking("Host / IP", s.host, keyboard) &&
         menu->textInputBlocking("Port", p, keyboard) &&
         menu->textInputBlocking("User", s.user, keyboard) &&
         menu->textInputBlocking("Password", s.password, keyboard, true)) {
             s.port = p.toInt();
             connectToServer(s.host, s.port, s.user, s.password, s.name);
     }
}

void App::handleSettings() {
    while(true) {
        std::vector<String> items = {
            "Change PIN",
            "WiFi Network",
            "Storage & Keys",
            "System Update",
            "System Info",
            "Battery Info",
            "Back"
        };
        int choice = menu->showMenuBlocking("Settings", items, keyboard);
        
        if (choice == 0) {
            handleChangePin();
        } 
        else if (choice == 1) {
             wifi.setIdleCallback([this]() { this->checkSystemInput(); });
             wifi.manage();
        } 
        else if (choice == 2) {
             handleStorage();
        }
        else if (choice == 3) {
             handleSystemUpdate();
        }
        else if (choice == 4) {
            String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "Disconnected";
            String bat = String(power.getPercentage()) + "% (" + String(power.getVoltage()) + "V)";
            String ram = String(ESP.getFreeHeap() / 1024) + " KB";
            
            ui.drawSystemInfo(ip, bat, ram, WiFi.macAddress());
            
            keyboard.clearBuffer();
            while(!keyboard.isKeyPressed()) {
                if (wifi.getIdleCallback()) wifi.getIdleCallback()(); 
                delay(50);
            }
            keyboard.getKeyChar(); // consume the key
        }
        else if (choice == 5) {
            handleBatteryInfo();
        }
        else {
            return;
        }
    }
}

void App::handleChangePin() {
    String newPin = "";
    
    // 1. Enter New PIN
    while (true) {
        ui.drawPinEntry("CHANGE PIN", "Enter New PIN:", newPin);
        while(!keyboard.available()) { delay(10); checkSystemInput(); }
        char key = keyboard.getKeyChar();
        if (key == '\n' || key == '\r') { if (newPin.length() > 0) break; }
        else if (key == 0x08) { if (newPin.length() > 0) newPin.remove(newPin.length()-1); }
        else if (key >= 32 && key <= 126) newPin += key;
    }
    
    // 2. Confirm
    String confirmPin = "";
    while (true) {
        ui.drawPinEntry("CHANGE PIN", "Confirm New PIN:", confirmPin);
        while(!keyboard.available()) { delay(10); checkSystemInput(); }
        char key = keyboard.getKeyChar();
        if (key == '\n' || key == '\r') { if (confirmPin.length() > 0) break; }
        else if (key == 0x08) { if (confirmPin.length() > 0) confirmPin.remove(confirmPin.length()-1); }
        else if (key >= 32 && key <= 126) confirmPin += key;
    }
    
    if (newPin != confirmPin) {
        ui.drawMessage("Error", "PIN Mismatch");
        return;
    }
    
    // 3. Re-Encrypt
    ui.drawMessage("Processing", "Re-encrypting data...");
    
    // Decrypt SSH key with old PIN before changing it
    String sshKey = security.getSSHKey();

    security.changePin(newPin); // Updates AES key
    
    serverManager.reEncryptAll();
    wifi.reEncryptAll();
    
    // Re-save SSH key with new PIN
    if (sshKey.length() > 0) {
        security.saveSSHKey(sshKey);
    }

    ui.drawMessage("Success", "PIN Changed!");
}

void App::unlockSystem() {
    String pin = "";
    display.setRefreshMode(true); 

    ui.drawPinEntry("SECURE BOOT", "Enter PIN:", "", false);

    while (true) {
        char key = keyboard.getKeyChar();
        if (key > 0) {
            bool changed = false;
            bool enter = false;
            if (key == '\n' || key == '\r') { if (pin.length() > 0) enter = true; } 
            else if (key == 0x08) { if (pin.length() > 0) { pin.remove(pin.length() - 1); changed = true; } } 
            else if (key >= 32 && key <= 126) { pin += key; changed = true; }
            
            if (enter) {
                 ui.drawMessage("Verifying", "Please wait...");
                 if (security.authenticate(pin)) break; 
                 else { 
                    pin = ""; 
                    ui.drawPinEntry("ACCESS DENIED", "Try Again:", "", true); 
                    delay(500); 
                 }
            } else if (changed) {
                 ui.drawPinEntry("SECURE BOOT", "Enter PIN:", pin, false);
            }
        }
        delay(10);
    }
    
    ui.drawMessage("UNLOCKED", "System Ready");
    delay(1000); 
    display.setRefreshMode(false); 
}
void App::handleStorage() {
    // 1. Automatically activate USB Mode on entry
    bool usbActive = storage.startUSBMode();
    
    // Lambda for checking eject in blocking menu
    auto storageIdle = [this]() {
        this->checkSystemInput();
        if (storage.isEjectRequested()) {
            storage.clearEjectRequest();
             ui.drawMessage("DISCONNECTED", "Safe to remove");
             delay(1000);
             exitStorageMode();
        }
    };

    if (usbActive) {
        ui.drawMessage("USB Active", "Connect to PC\nCopy id_rsa");
        delay(1500); 
    } else {
        ui.drawMessage("Warning", "USB Init Failed");
        delay(1000);
    }

    while(true) {
        std::vector<String> items = {
            "Scan USB Disk",
            "Import from SD",
            "Back"
        };
        
        String title = storage.isUSBActive() ? "Storage (USB ON)" : "Storage (No USB)";
        int choice = menu->showMenuBlocking(title, items, keyboard, storageIdle);
        
        if (choice == 0) { // Scan USB
             ui.drawMessage("Scanning...", "Checking Disk...");
             String key = storage.scanRAMDiskForKey();
             
             if (key.length() > 20 && key.startsWith("-----BEGIN")) {
                 security.saveSSHKey(key);
                 ui.drawMessage("Success", "Key Imported!");
                 delay(1500);
                 
                 exitStorageMode();
                 return; // Exit menu/Restart
             } else {
                 ui.drawMessage("Failed", "Key not found.\nTry copying again.");
                 delay(2000);
             }
             
        } else if (choice == 1) { // SD Import
             String key = storage.readSSHKey("/id_rsa");
             if (key.length() > 20 && key.startsWith("-----BEGIN")) {
                 security.saveSSHKey(key);
                 ui.drawMessage("Success", "Key Imported!");
                 delay(1000);
             } else {
                 ui.drawMessage("Error", "Invalid/Missing Key");
                 delay(1000);
             }
        } else { // Back
            // 3. Deactivate on exit
            if (usbActive) {
                exitStorageMode();
            }
            return;
        }
    }
}

void App::exitStorageMode() {
    storage.stopUSBMode();
    
    // Restart for full USB Stack reset
    ui.drawMessage("Restarting...", "Switching Mode");
    delay(1000);
    ESP.restart();
}

void App::handleBatteryInfo() {
    // 0. Use new helper to clear buffer
    keyboard.clearBuffer(500);

    while (true) {
        BatteryStatus status = power.getStatus();

        // --- Render using new UILayout System ---
        ui.render([&](U8G2_FOR_ADAFRUIT_GFX& u8g2) {
             UILayout layout(ui, "BATTERY STATUS");
             
             char buf[64];
             
             sprintf(buf, "Src: %s", status.powerSource.c_str());
             layout.addText(buf);
             
             sprintf(buf, "Bat: %d%% (%.2fV)", status.percentage, status.voltage);
             layout.addText(buf);
             
             if (status.voltage > 0) {                 
                 sprintf(buf, "Cur: %d mA", status.currentMa);
                 layout.addText(buf);

                 sprintf(buf, "Cap: %d / %d", status.remainingCap, status.fullCap);
                 layout.addText(buf);
                 
                 sprintf(buf, "Hlth: %d%% Cyc: %d", status.soh, status.cycles);
                 layout.addText(buf);

                 sprintf(buf, "Tmp: %.1f C", status.temperature);
                 layout.addText(buf);
             } 
             
             layout.addFooter("[ Press Key to Exit ]");
        });
        
        // 500ms Refresh Rate
        unsigned long start = millis();
        while (millis() - start < 500) {
            if (keyboard.isKeyPressed()) {
                keyboard.getKeyChar(); // consume
                return;
            }
            delay(10);
        }
    }
}

