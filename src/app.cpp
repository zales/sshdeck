#include "app.h"
#include <WiFi.h>
#include <Wire.h>
#include <driver/rtc_io.h>
#include "board_def.h"
#include "states/app_menu_state.h"
#include "states/app_terminal_state.h"

App::App() 
    : wifi(terminal, keyboard, ui, power), ui(display), ota(display), currentState(nullptr), lastAniUpdate(0), lastScreenRefresh(0), refreshPending(false) {
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
    
    // Initialize State Machine
    changeState(new AppMenuState());

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

void App::changeState(AppState* newState) {
    if (currentState) {
        currentState->exit(*this);
    }
    currentState.reset(newState);
    if (currentState) {
        currentState->enter(*this);
    }
}

void App::loop() {
    ota.loop();
    
    // Process pending refresh requests (debounced)
    if (refreshPending) {
        refreshPending = false;
        if (currentState) {
            currentState->onRefresh(*this);
        }
    }
    
    // Status update logic
    static unsigned long lastStatusUpdate = 0;
    if (millis() - lastStatusUpdate > 1000) {
        ui.updateStatusState(power.getPercentage(), power.isCharging(), WiFi.status() == WL_CONNECTED);
        lastStatusUpdate = millis();
    }
    
    // Delegate to Current State
    if (currentState) {
        currentState->update(*this);
    }
}

void App::handleMainMenu() {
    std::vector<String> items = {
        "Saved Servers",
        "Quick Connect",
        "Settings",
        "Power Off"
    };
    menu->showMenu("Main Menu", items, [this](int choice) {
        if (choice == 0) handleSavedServers();
        else if (choice == 1) handleQuickConnect();
        else if (choice == 2) handleSettings();
        else if (choice == 3) enterDeepSleep();
    });
}


void App::handleSystemUpdate() {
    if (WiFi.status() != WL_CONNECTED) {
        menu->showMessage("Error", "No WiFi Connection", [this](){ handleSettings(); });
        return;
    }
    
    // Show spinner/message then work
    String msg = String(APP_VERSION);
    if (!msg.startsWith("v")) msg = "v" + msg;
    menu->showMessage("Checking...", msg);
    
    // Perform work in next loop iteration
    menu->setOnLoop([this]() {
         menu->setOnLoop(nullptr); // Run once

         String url = UPDATE_SERVER_URL; 
         String manifestUrl = url;
         manifestUrl.replace("firmware.bin", "firmware.json");
         
         UpdateManifest manifest = ota.fetchManifest(manifestUrl, UPDATE_ROOT_CA);
         
         if (manifest.versions.empty()) {
             // Fallback to legacy check
             String newVer = ota.checkUpdateAvailable(url, APP_VERSION, UPDATE_ROOT_CA);
             if (newVer == "") {
                std::vector<String> opts = {"Reinstall?", "Cancel"};
                menu->showMenu("No update found", opts, [this, url](int c){
                    if(c==0) ota.updateFromUrl(url, UPDATE_ROOT_CA);
                    else handleSettings();
                }, [this](){ handleSettings(); });
             } else {
                 ota.updateFromUrl(url, UPDATE_ROOT_CA);
             }
         } else {
            // Show version selection menu
            auto versions = std::make_shared<std::vector<FirmwareVersion>>(manifest.versions);
            std::vector<String> options;
            std::vector<String> urls;
            
            for (const auto& v : *versions) {
                String label = v.version;
                if (v.version == String(APP_VERSION)) label += " (Curr)";
                if (v.version == manifest.latestVersion) label += " *";
                options.push_back(label);
            }
            
            menu->showMenu("Select Version", options, [this, versions](int selected){
                 if (selected < 0 || selected >= (int)versions->size()) return;
                 
                 String targetUrl = (*versions)[selected].url;
                 String targetVer = (*versions)[selected].version;
                 
                 std::vector<String> confirmOpts = {"Yes, Flash it", "No"};
                 menu->showMenu("Flash v" + targetVer + "?", confirmOpts, [this, targetUrl](int c){
                     if(c==0) {
                         ota.updateFromUrl(targetUrl, UPDATE_ROOT_CA);
                         // if update fails
                         menu->showMessage("Error", "Update Failed", [this](){ handleSettings(); });
                     } else {
                         handleSettings();
                     }
                 }, [this](){ handleSystemUpdate(); });
            }, [this](){ handleSettings(); });
         }
    });
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

void App::drawTerminalScreen(bool partial) {
    // Thread-safe display update with mutex
    display.lock();
    
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

    ui.drawTerminal(terminal, title, power.getPercentage(), power.isCharging(), WiFi.status() == WL_CONNECTED, partial);
    terminal.clearUpdateFlag();
    
    display.unlock();
}

void App::showHelpScreen() {
    display.lock();
    ui.drawHelpScreen();
    display.unlock();
    
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
    // Show connecting message first (non-blocking render, but next steps might block network)
    // We can't easily make LibSSH non-blocking in this codebase without major rewrite.
    // So we accept that the UI freezes during "Connecting..."
    
    ui.drawMessage("Connecting...", "To: " + name); 
    // Force display update since we might block immediately after
    
    // unique_ptr automatically deletes the old object when assigned a new one
    sshClient.reset(new SSHClient(terminal, keyboard));
    
    if (!sshClient) {
        menu->showMessage("Error", "Alloc Failed", [this](){ handleMainMenu(); });
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
                menu->showMessage("Error", "WiFi Failed", [this](){ handleMainMenu(); });
                return;
            }
    }

    String privKey = security.getSSHKey();
    const char* keyData = (privKey.length() > 0) ? privKey.c_str() : nullptr;

    if (sshClient->connectSSH(host.c_str(), port, user.c_str(), pass.c_str(), keyData)) {
        changeState(new AppTerminalState());
    } else {
         // Connection failed
         // Just show message, on dismiss go to main menu
         menu->showMessage("Error", "SSH Failed", [this](){ handleMainMenu(); });
    }
}

void App::handleSavedServers() {
    std::vector<String> items;
    auto servers = serverManager.getServers();
    for (const auto& s : servers) {
        items.push_back(s.name);
    }
    items.push_back("[ Add New Server ]");

    menu->showMenu("Saved Servers", items, [this, servers](int choice) {
        if (choice >= (int)servers.size()) {
            // Add New Server Wizard
            auto newSrv = std::make_shared<ServerConfig>();
            newSrv->port = 22;
            
            menu->showInput("Name", "", false, [this, newSrv](String val) {
                newSrv->name = val;
                menu->showInput("Host", "", false, [this, newSrv](String val) {
                    newSrv->host = val;
                    menu->showInput("User", "", false, [this, newSrv](String val) {
                        newSrv->user = val;
                        menu->showInput("Port", "22", false, [this, newSrv](String val) {
                            newSrv->port = val.toInt();
                            menu->showInput("Password", "", true, [this, newSrv](String val) {
                                newSrv->password = val;
                                serverManager.addServer(*newSrv);
                                handleSavedServers();
                            }, [this](){ handleSavedServers(); });
                        }, [this](){ handleSavedServers(); });
                    }, [this](){ handleSavedServers(); });
                }, [this](){ handleSavedServers(); });
            }, [this](){ handleSavedServers(); });
            
        } else {
            // Selected existing server
            ServerConfig selected = servers[choice];
            std::vector<String> actions = {"Connect", "Edit", "Delete"};
            
            // Nested Menu for Actions
            menu->showMenu(selected.name, actions, [this, choice, selected](int action) {
                if (action == 0) { // Connect
                    connectToServer(selected.host, selected.port, selected.user, selected.password, selected.name);
                } else if (action == 1) { // Edit
                    auto srv = std::make_shared<ServerConfig>(selected);
                    menu->showInput("Name", srv->name, false, [this, choice, srv](String val) {
                        srv->name = val;
                        menu->showInput("Host", srv->host, false, [this, choice, srv](String val) {
                            srv->host = val;
                            menu->showInput("User", srv->user, false, [this, choice, srv](String val) {
                                srv->user = val;
                                menu->showInput("Port", String(srv->port), false, [this, choice, srv](String val) {
                                    srv->port = val.toInt();
                                    menu->showInput("Password", srv->password, true, [this, choice, srv](String val) {
                                        srv->password = val;
                                        serverManager.updateServer(choice, *srv);
                                        handleSavedServers();
                                    }, [this](){ handleSavedServers(); });
                                }, [this](){ handleSavedServers(); });
                            }, [this](){ handleSavedServers(); });
                        }, [this](){ handleSavedServers(); });
                    }, [this](){ handleSavedServers(); });
                } else if (action == 2) { // Delete
                     std::vector<String> yn = {"No", "Yes"};
                     menu->showMenu("Delete?", yn, [this, choice](int ynChoice) {
                        if (ynChoice == 1) {
                            serverManager.removeServer(choice);
                        }
                        handleSavedServers();
                     }, [this](){ handleSavedServers(); });
                }
            }, [this](){ handleSavedServers(); });
        }
    }, [this]() {
        handleMainMenu();
    });
}

void App::handleQuickConnect() {
     auto s = std::make_shared<ServerConfig>();
     s->name = "Quick Connect";
     s->port = 22;
     
     menu->showInput("Host / IP", s->host, false, [this, s](String val) {
         s->host = val;
         menu->showInput("Port", "22", false, [this, s](String val) {
             s->port = val.toInt();
             menu->showInput("User", s->user, false, [this, s](String val) {
                 s->user = val;
                 menu->showInput("Password", s->password, true, [this, s](String val) {
                     s->password = val;
                     connectToServer(s->host, s->port, s->user, s->password, s->name);
                 }, [this](){ handleMainMenu(); });
             }, [this](){ handleMainMenu(); });
         }, [this](){ handleMainMenu(); });
     }, [this](){ handleMainMenu(); });
}

void App::handleSettings() {
    std::vector<String> items = {
        "Change PIN",
        "WiFi Network",
        "Storage & Keys",
        "System Update",
        "System Info",
        "Battery Info"
    };
    menu->showMenu("Settings", items, [this](int choice) {
        if (choice == 0) {
            handleChangePin();
        } 
        else if (choice == 1) {
             handleWifiMenu();
        } 
        else if (choice == 2) {
             handleStorage();
        }
        else if (choice == 3) {
             handleSystemUpdate();
        }
        else if (choice == 4) {
             // System Info
             String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "Disconnected";
             String bat = String(power.getPercentage()) + "% (" + String(power.getVoltage()) + "V)";
             String ram = String(ESP.getFreeHeap() / 1024) + " KB";
             String msg = "IP: " + ip + "\nBat: " + bat + "\nRAM: " + ram + "\nMAC: " + WiFi.macAddress();
             
             menu->showMessage("System Info", msg, [this](){ handleSettings(); });
        }
        else if (choice == 5) {
            handleBatteryInfo();
        }
    }, [this]() {
        handleMainMenu();
    });
}

void App::handleChangePin() {
    menu->showInput("CHANGE PIN", "", true, [this](String newPin) {
        if (newPin.length() == 0) {
            handleSettings(); // Abort
            return;
        }
        menu->showInput("CONFIRM PIN", "", true, [this, newPin](String confirmPin) {
            if (newPin != confirmPin) {
                menu->showMessage("Error", "PIN Mismatch", [this](){ handleSettings(); });
                return;
            }
            
            // Perform Encryption
            menu->showMessage("Processing", "Re-encrypting...");
            // Force a draw update immediately? 
            // In non-blocking, we might need to yield or run this in next loop.
            // But encryption is blocking anyway.
            
            // Decrypt SSH key with old PIN before changing it
            // Wait, we don't know the old PIN here? 
            // security.getSSHKey() uses internal cached key if unlocked?
            // Assuming security manager handles state.
            String sshKey = security.getSSHKey();
        
            security.changePin(newPin); // Updates AES key
            
            serverManager.reEncryptAll();
            wifi.reEncryptAll();
            
            if (sshKey.length() > 0) {
                security.saveSSHKey(sshKey);
            }
        
            menu->showMessage("Success", "PIN Changed!", [this](){ handleSettings(); });

        }, [this](){ handleSettings(); });
    }, [this](){ handleSettings(); });
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
    
    auto showStorageMenu = [this, usbActive]() {
        std::vector<String> items = {
            "Scan USB Disk",
            "Import from SD"
        };
        
        String title = usbActive ? "Storage (USB ON)" : "Storage (No USB)";
        menu->showMenu(title, items, [this, usbActive](int choice) {
             if (choice == 0) { // Scan USB
                 menu->showMessage("Scanning...", "Checking Disk...", [this, usbActive](){ 
                     String key = storage.scanRAMDiskForKey();
                     if (key.length() > 20 && key.startsWith("-----BEGIN")) {
                         security.saveSSHKey(key);
                         menu->showMessage("Success", "Key Imported!", [this](){ exitStorageMode(); });
                     } else {
                         menu->showMessage("Failed", "Key not found.", [this](){ handleStorage(); /* recurses/refreshes */ });
                     }
                 });
             } else if (choice == 1) { // SD Import
                 String key = storage.readSSHKey("/id_rsa");
                 if (key.length() > 20 && key.startsWith("-----BEGIN")) {
                     security.saveSSHKey(key);
                     menu->showMessage("Success", "Key Imported!", [this](){ handleStorage(); });
                 } else {
                     menu->showMessage("Error", "Invalid/Missing Key", [this](){ handleStorage(); });
                 }
             }
        }, [this, usbActive]() {
            // Back
            if (usbActive) {
                exitStorageMode();
            } else {
                handleSettings();
            }
        });

        if (usbActive) {
            menu->setOnLoop([this](){
                if (storage.isEjectRequested()) {
                    storage.clearEjectRequest();
                     menu->showMessage("DISCONNECTED", "Safe to remove", [this](){
                         exitStorageMode();
                     });
                }
            });
        }
    };

    if (usbActive) {
        menu->showMessage("USB Active", "Connect to PC\nCopy id_rsa", showStorageMenu);
    } else {
        menu->showMessage("Warning", "USB Init Failed", showStorageMenu);
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
    auto getBatteryMsg = [this]() -> String {
         BatteryStatus status = power.getStatus();
         String msg = "";
         msg += "Src: " + status.powerSource + "\n";
         msg += "Bat: " + String(status.percentage) + "% " + String(status.voltage) + "V\n";
         if (status.voltage > 0) { // If fuel gauge active
             msg += "Cur: " + String(status.currentMa) + " mA\n";
             msg += "Cap: " + String(status.remainingCap) + " / " + String(status.fullCap) + "\n";
             msg += "Hlth: " + String(status.soh) + "% Cyc: " + String(status.cycles) + "\n";
             msg += "Tmp: " + String(status.temperature, 1) + " C";
         }
         return msg;
    };

    // Initial Full Refresh
    menu->showMessage("BATTERY STATUS", getBatteryMsg(), [this](){ handleSettings(); });

    // Loop for Partial Updates
    menu->setOnLoop([this, getBatteryMsg](){
        static unsigned long lastUp = 0;
        if (millis() - lastUp > 1000) {
            lastUp = millis();
            menu->updateMessage(getBatteryMsg());
        }
    });
}

void App::handleWifiMenu() {
    std::vector<String> items;
    
    bool connected = (WiFi.status() == WL_CONNECTED);
    if (connected) {
        items.push_back("Disconnect: " + WiFi.SSID());
    }
    items.push_back("Scan Networks");
    items.push_back("Saved Networks");
    items.push_back("Manual Connect");

    menu->showMenu("WiFi Manager", items, [this, connected](int choice) {
        // Adjust index based on dynamic items
        int scanIdx = connected ? 1 : 0;
        int savedIdx = connected ? 2 : 1;
        int manIdx = connected ? 3 : 2;

        if (connected && choice == 0) {
            WiFi.disconnect();
            menu->showMessage("WiFi", "Disconnected", [this](){ handleWifiMenu(); });
            return;
        }

        if (choice == scanIdx) {
            menu->showMessage("WiFi", "Scanning Networks...", nullptr);
             
            // Auto-start scan in next loop (no wait for key)
            menu->setOnLoop([this](){
                 menu->setOnLoop(nullptr); 
                 
                 // Small delay to ensure "Scanning..." is rendered if we came from another view
                 // (Though Loop architecture usually renders before next update/input poll)
                 
                 auto networks = wifi.scan(); // returns vector<WifiScanResult>
                 
                 if (networks.empty()) {
                     menu->showMessage("Scan", "No networks found", [this](){ handleWifiMenu(); });
                     return;
                 }
                 
                 std::vector<String> labels;
                 for(const auto& net : networks) {
                     String label = net.ssid;
                     
                     // Pad SSID up to 14 chars
                     while(label.length() < 14) label += " ";
                     if(label.length() > 14) label = label.substring(0, 14);
                     
                     // Lock icon
                     label += (net.secure ? "*" : " ");
                     label += " ";
                     
                     // RSSI Bars
                     if (net.rssi > -60) label += "[====]";
                     else if (net.rssi > -70) label += "[=== ]";
                     else if (net.rssi > -80) label += "[==  ]";
                     else label += "[=   ]";
                     
                     labels.push_back(label);
                 }
                 
                 menu->showMenu("Scan Results", labels, [this, networks](int idx) {
                         if (idx < 0 || idx >= (int)networks.size()) return;

                         String ssid = networks[idx].ssid;
                         
                         // Check if known
                         auto saved = wifi.getSavedNetworks();
                         String existingPass = "";
                         for(auto& s : saved) {
                             if(s.ssid == ssid) {
                                 existingPass = s.pass;
                                 break;
                             }
                         }

                         menu->showInput("Password", existingPass, true, [this, ssid](String pass) {
                             menu->showMessage("Connecting", "Please wait...", nullptr);
                             menu->setOnLoop([this, ssid, pass](){
                                 menu->setOnLoop(nullptr);
                                 if (wifi.connectTo(ssid, pass)) {
                                     wifi.save(ssid, pass);
                                     menu->showMessage("Success", "Connected!", [this](){ handleWifiMenu(); });
                                 } else {
                                     menu->showMessage("Error", "Connection Failed", [this](){ handleWifiMenu(); });
                                 }
                             });
                         }, [this](){ handleWifiMenu(); });
                     }, [this](){ handleWifiMenu(); });
                 });
            }
        else if (choice == savedIdx) {
            auto saved = wifi.getSavedNetworks();
            if (saved.empty()) {
                menu->showMessage("Saved", "No saved networks", [this](){ handleWifiMenu(); });
                return;
            }
            std::vector<String> labels;
            for(auto& s : saved) labels.push_back(s.ssid);
            
            menu->showMenu("Saved Networks", labels, [this, saved](int idx) {
                String ssid = saved[idx].ssid;
                std::vector<String> opt = {"Connect", "Forget"};
                menu->showMenu(ssid, opt, [this, ssid, idx](int action){
                    if(action == 0) { // Connect
                        menu->showMessage("Connecting", "Please wait...", nullptr);
                        menu->setOnLoop([this, ssid, idx](){
                             menu->setOnLoop(nullptr);
                             auto savedFresh = wifi.getSavedNetworks();
                             if (idx < savedFresh.size()) {
                                 if(wifi.connectTo(savedFresh[idx].ssid, savedFresh[idx].pass)) {
                                      menu->showMessage("Success", "Connected!", [this](){ handleWifiMenu(); });
                                 } else {
                                      menu->showMessage("Error", "Failed", [this](){ handleWifiMenu(); });
                                 }
                             } else {
                                 handleWifiMenu();
                             }
                        });
                    } else { // Forget
                        wifi.forget(idx);
                        menu->showMessage("WiFi", "Network Forgotten", [this](){ handleWifiMenu(); });
                    }
                }, [this](){ handleWifiMenu(); });
            }, [this](){ handleWifiMenu(); });
        }
        else if (choice == manIdx) {
            menu->showInput("SSID", "", false, [this](String ssid){
                menu->showInput("Password", "", true, [this, ssid](String pass){
                     menu->showMessage("Connecting", "Please wait...", nullptr);
                     menu->setOnLoop([this, ssid, pass](){
                         menu->setOnLoop(nullptr);
                         if (wifi.connectTo(ssid, pass)) {
                             wifi.save(ssid, pass);
                             menu->showMessage("Success", "Connected!", [this](){ handleWifiMenu(); });
                         } else {
                             menu->showMessage("Error", "Connection Failed", [this](){ handleWifiMenu(); });
                         }
                     });
                }, [this](){ handleWifiMenu(); });
            }, [this](){ handleWifiMenu(); });
        }
    }, [this](){ handleSettings(); });
}
