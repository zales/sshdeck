#include "controllers/settings_controller.h"
#include "ui/menu_system.h"
#include "power_manager.h"
#include "wifi_manager.h"
#include "storage_manager.h"
#include "ota_manager.h"
#include "security_manager.h"
#include "server_manager.h"
#include "touch_manager.h"
#include <WiFi.h>

SettingsController::SettingsController(ISystemContext& system) : _system(system) {
}

void SettingsController::showSettingsMenu() {
    std::vector<String> items = {
        "Change PIN",
        "WiFi Network",
        "Storage & Keys",
        "System Update",
        "System Info",
        "Battery Info",
        "Touch Test"
    };
    _system.menu()->showMenu("Settings", items, [this](int choice) {
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
             String bat = String(_system.power().getPercentage()) + "% (" + String(_system.power().getVoltage()) + "V)";
             String ram = String(ESP.getFreeHeap() / 1024) + " KB";
             String msg = "IP: " + ip + "\nBat: " + bat + "\nRAM: " + ram + "\nMAC: " + WiFi.macAddress();
             
             _system.menu()->showMessage("System Info", msg, [this](){ showSettingsMenu(); });
        }
        else if (choice == 5) {
            showBatteryInfo();
        }
        else if (choice == 6) {
            handleTouchTest();
        }
    }, [this]() {
        _system.handleMainMenu();
    });
}

void SettingsController::handleChangePin() {
    _system.menu()->showInput("CHANGE PIN", "", true, [this](String newPin) {
        if (newPin.length() == 0) {
            showSettingsMenu(); // Abort
            return;
        }
        _system.menu()->showInput("CONFIRM PIN", "", true, [this, newPin](String confirmPin) {
            if (newPin != confirmPin) {
                _system.menu()->showMessage("Error", "PIN Mismatch", [this](){ showSettingsMenu(); });
                return;
            }
            
            // Perform Encryption
            _system.menu()->showMessage("Processing", "Re-encrypting...");
            
            String sshKey = _system.security().getSSHKey();
        
            _system.security().changePin(newPin); // Updates AES key
            
            _system.serverManager().reEncryptAll();
            _system.wifi().reEncryptAll();
            
            if (sshKey.length() > 0) {
                _system.security().saveSSHKey(sshKey);
            }
        
            _system.menu()->showMessage("Success", "PIN Changed!", [this](){ showSettingsMenu(); });

        }, [this](){ showSettingsMenu(); });
    }, [this](){ showSettingsMenu(); });
}

void SettingsController::handleStorage() {
    // 1. Automatically activate USB Mode on entry
    bool usbActive = _system.storage().startUSBMode();
    
    auto showStorageMenu = [this, usbActive]() {
        std::vector<String> items = {
            "Scan USB Disk",
            "Import from SD"
        };
        
        String title = usbActive ? "Storage (USB ON)" : "Storage (No USB)";
        _system.menu()->showMenu(title, items, [this, usbActive](int choice) {
             if (choice == 0) { // Scan USB
                 _system.menu()->showMessage("Scanning...", "Checking Disk...", [this, usbActive](){ 
                     String key = _system.storage().scanRAMDiskForKey();
                     if (key.length() > 20 && key.startsWith("-----BEGIN")) {
                         _system.security().saveSSHKey(key);
                         _system.menu()->showMessage("Success", "Key Imported!", [this](){ exitStorageMode(); });
                     } else {
                         _system.menu()->showMessage("Failed", "Key not found.", [this](){ handleStorage(); /* recurses/refreshes */ });
                     }
                 });
             } else if (choice == 1) { // SD Import
                 String key = _system.storage().readSSHKey("/id_rsa");
                 if (key.length() > 20 && key.startsWith("-----BEGIN")) {
                     _system.security().saveSSHKey(key);
                     _system.menu()->showMessage("Success", "Key Imported!", [this](){ handleStorage(); });
                 } else {
                     _system.menu()->showMessage("Error", "Invalid/Missing Key", [this](){ handleStorage(); });
                 }
             }
        }, [this, usbActive]() {
            // Back
            if (usbActive) {
                exitStorageMode();
            } else {
                showSettingsMenu();
            }
        });

        if (usbActive) {
            _system.menu()->setOnLoop([this](){
                if (_system.storage().isEjectRequested()) {
                    _system.storage().clearEjectRequest();
                     _system.menu()->showMessage("DISCONNECTED", "Safe to remove", [this](){
                         exitStorageMode();
                     });
                }
            });
        }
    };

    if (usbActive) {
        _system.menu()->showMessage("USB Active", "Connect to PC\nCopy id_rsa", showStorageMenu);
    } else {
        _system.menu()->showMessage("Warning", "USB Init Failed", showStorageMenu);
    }
}

void SettingsController::exitStorageMode() {
    _system.storage().stopUSBMode();
    
    // Restart for full USB Stack reset
    _system.ui().drawMessage("Restarting...", "Switching Mode");
    delay(1000);
    ESP.restart();
}

void SettingsController::showBatteryInfo() {
    auto getBatteryMsg = [this]() -> String {
         BatteryStatus status = _system.power().getStatus();
         String msg = "";
         msg += "Src: " + status.powerSource + "\n";
         msg += "Bat: " + String(status.percentage) + "% " + String(status.voltage) + "V\n";
         if (status.voltage > 0) { // If fuel gauge active
             msg += "Cur: " + String(status.currentMa) + " mA\n";
             msg += "Cap: " + String(status.remainingCap) + " / " + String(status.fullCap) + "\n";
             msg += "Tmp: " + String(status.temperature, 1) + " C";
         }
         return msg;
    };

    // Initial Full Refresh
    _system.menu()->showMessage("BATTERY STATUS", getBatteryMsg(), [this](){ showSettingsMenu(); });

    // Loop for Partial Updates
    _system.menu()->setOnLoop([this, getBatteryMsg](){
        static unsigned long lastUp = 0;
        if (millis() - lastUp > 1000) {
            lastUp = millis();
            _system.menu()->updateMessage(getBatteryMsg());
        }
    });
}

void SettingsController::handleTouchTest() {
    String initMsg = "Model: " + String(_system.touch().getModelName()) +
                     "\nChip ID: 0x" + String(_system.touch().getChipId(), HEX) +
                     "\nINT pin: " + String(digitalRead(BOARD_TOUCH_INT)) +
                     "\n\nTouch the screen...\nPress key to exit.";
    _system.menu()->showMessage("TOUCH TEST", initMsg, [this](){ showSettingsMenu(); });
    
    _system.menu()->setOnLoop([this](){
        static unsigned long lastUp = 0;
        static String lastGesture = "none";
        static int lastX = 0, lastY = 0;
        static int eventCount = 0;
        
        if (_system.touch().available()) {
            TouchEvent te = _system.touch().read();
            if (te.touched || te.gesture != GESTURE_NONE) {
                eventCount++;
                if (te.x != 0 || te.y != 0) {
                    lastX = te.x;
                    lastY = te.y;
                }
            }
            
            switch (te.gesture) {
                case GESTURE_SWIPE_UP:    lastGesture = "SWIPE UP"; break;
                case GESTURE_SWIPE_DOWN:  lastGesture = "SWIPE DOWN"; break;
                case GESTURE_SWIPE_LEFT:  lastGesture = "SWIPE LEFT"; break;
                case GESTURE_SWIPE_RIGHT: lastGesture = "SWIPE RIGHT"; break;
                case GESTURE_SINGLE_TAP:  lastGesture = "TAP"; break;
                case GESTURE_LONG_PRESS:  lastGesture = "LONG PRESS"; break;
                default: break;
            }
        }
        
        if (millis() - lastUp > 500) {
            lastUp = millis();
            String msg = "Model: " + String(_system.touch().getModelName()) + "\n";
            msg += "Chip ID: 0x" + String(_system.touch().getChipId(), HEX) + "\n";
            msg += "Events: " + String(eventCount) + "\n";
            msg += "Gesture: " + lastGesture + "\n";
            msg += "X:" + String(lastX) + " Y:" + String(lastY) +
                   " INT:" + String(digitalRead(BOARD_TOUCH_INT));
            _system.menu()->updateMessage(msg);
        }
    });
}

void SettingsController::handleSystemUpdate() {
    if (WiFi.status() != WL_CONNECTED) {
        // Try auto-connecting to saved WiFi first
        auto saved = _system.wifi().getSavedNetworks();
        if (saved.empty()) {
            // No saved networks — go to WiFi menu, then retry update
            _system.menu()->showMessage("WiFi Required", "No saved networks.\nOpening WiFi setup...", nullptr);
            _system.menu()->setOnLoop([this](){
                _system.menu()->setOnLoop(nullptr);
                handleWifiForUpdate();
            });
            return;
        }
        _system.menu()->showMessage("Connecting WiFi", "Please wait...", nullptr);
        _system.menu()->setOnLoop([this](){
            _system.menu()->setOnLoop(nullptr);
            if (_system.wifi().connect()) {
                // Connected — proceed with update
                handleSystemUpdate();
            } else {
                // Auto-connect failed — offer WiFi menu
                handleWifiForUpdate();
            }
        });
        return;
    }
    
    // Show spinner/message then work
    String msg = String(APP_VERSION);
    if (!msg.startsWith("v")) msg = "v" + msg;
    _system.menu()->showMessage("Checking...", msg);
    
    // Perform work in next loop iteration
    _system.menu()->setOnLoop([this]() {
         _system.menu()->setOnLoop(nullptr); // Run once

         String url = UPDATE_SERVER_URL; 
         String manifestUrl = url;
         manifestUrl.replace("firmware.bin", "firmware.json");
         
         UpdateManifest manifest = _system.ota().fetchManifest(manifestUrl, UPDATE_ROOT_CA);
         
         if (manifest.versions.empty()) {
             // Fallback to legacy check
             String newVer = _system.ota().checkUpdateAvailable(url, APP_VERSION, UPDATE_ROOT_CA);
             if (newVer == "") {
                std::vector<String> opts = {"Reinstall?", "Cancel"};
                _system.menu()->showMenu("No update found", opts, [this, url](int c){
                    if(c==0) _system.ota().updateFromUrl(url, UPDATE_ROOT_CA);
                    else showSettingsMenu();
                }, [this](){ showSettingsMenu(); });
             } else {
                 _system.ota().updateFromUrl(url, UPDATE_ROOT_CA);
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
            
            _system.menu()->showMenu("Select Version", options, [this, versions](int selected){
                 if (selected < 0 || selected >= (int)versions->size()) return;
                 
                 String targetUrl = (*versions)[selected].url;
                 String targetVer = (*versions)[selected].version;
                 
                 std::vector<String> confirmOpts = {"Yes, Flash it", "No"};
                 _system.menu()->showMenu("Flash " + targetVer + "?", confirmOpts, [this, targetUrl](int c){
                     if(c==0) {
                         _system.ota().updateFromUrl(targetUrl, UPDATE_ROOT_CA);
                         // if update fails
                         _system.menu()->showMessage("Error", "Update Failed", [this](){ showSettingsMenu(); });
                     } else {
                         showSettingsMenu();
                     }
                 }, [this](){ handleSystemUpdate(); });
            }, [this](){ showSettingsMenu(); });
         }
    });
}

void SettingsController::handleWifiForUpdate() {
    std::vector<String> items = {"Scan Networks", "Saved Networks", "Manual Connect", "Cancel"};
    _system.menu()->showMenu("WiFi for Update", items, [this](int choice) {
        if (choice == 0) {
            // Scan
            _system.menu()->showMessage("WiFi", "Scanning...", nullptr);
            _system.menu()->setOnLoop([this](){
                _system.menu()->setOnLoop(nullptr);
                auto networks = _system.wifi().scan();
                if (networks.empty()) {
                    _system.menu()->showMessage("Scan", "No networks found", [this](){ handleWifiForUpdate(); });
                    return;
                }
                std::vector<String> labels;
                for (const auto& net : networks) {
                    String label = net.ssid;
                    while (label.length() < 14) label += " ";
                    if (label.length() > 14) label = label.substring(0, 14);
                    label += (net.secure ? "*" : " ");
                    label += " ";
                    if (net.rssi > -60) label += "[====]";
                    else if (net.rssi > -70) label += "[=== ]";
                    else if (net.rssi > -80) label += "[==  ]";
                    else label += "[=   ]";
                    labels.push_back(label);
                }
                _system.menu()->showMenu("Select Network", labels, [this, networks](int idx) {
                    if (idx < 0 || idx >= (int)networks.size()) return;
                    String ssid = networks[idx].ssid;
                    // Check for saved password
                    auto saved = _system.wifi().getSavedNetworks();
                    String existingPass = "";
                    for (auto& s : saved) {
                        if (s.ssid == ssid) { existingPass = s.pass; break; }
                    }
                    _system.menu()->showInput("Password", existingPass, true, [this, ssid](String pass) {
                        _system.menu()->showMessage("Connecting", "Please wait...", nullptr);
                        _system.menu()->setOnLoop([this, ssid, pass](){
                            _system.menu()->setOnLoop(nullptr);
                            if (_system.wifi().connectTo(ssid, pass)) {
                                _system.wifi().save(ssid, pass);
                                handleSystemUpdate(); // Proceed to update
                            } else {
                                _system.menu()->showMessage("Error", "Connection Failed", [this](){ handleWifiForUpdate(); });
                            }
                        });
                    }, [this](){ handleWifiForUpdate(); });
                }, [this](){ handleWifiForUpdate(); });
            });
        } else if (choice == 1) {
            // Saved networks
            auto saved = _system.wifi().getSavedNetworks();
            if (saved.empty()) {
                _system.menu()->showMessage("Saved", "No saved networks", [this](){ handleWifiForUpdate(); });
                return;
            }
            std::vector<String> labels;
            for (auto& s : saved) labels.push_back(s.ssid);
            _system.menu()->showMenu("Saved Networks", labels, [this, saved](int idx) {
                if (idx < 0 || idx >= (int)saved.size()) return;
                _system.menu()->showMessage("Connecting", "Please wait...", nullptr);
                _system.menu()->setOnLoop([this, saved, idx](){
                    _system.menu()->setOnLoop(nullptr);
                    if (_system.wifi().connectTo(saved[idx].ssid, saved[idx].pass)) {
                        handleSystemUpdate(); // Proceed to update
                    } else {
                        _system.menu()->showMessage("Error", "Failed", [this](){ handleWifiForUpdate(); });
                    }
                });
            }, [this](){ handleWifiForUpdate(); });
        } else if (choice == 2) {
            // Manual
            _system.menu()->showInput("SSID", "", false, [this](String ssid) {
                _system.menu()->showInput("Password", "", true, [this, ssid](String pass) {
                    _system.menu()->showMessage("Connecting", "Please wait...", nullptr);
                    _system.menu()->setOnLoop([this, ssid, pass](){
                        _system.menu()->setOnLoop(nullptr);
                        if (_system.wifi().connectTo(ssid, pass)) {
                            _system.wifi().save(ssid, pass);
                            handleSystemUpdate();
                        } else {
                            _system.menu()->showMessage("Error", "Connection Failed", [this](){ handleWifiForUpdate(); });
                        }
                    });
                }, [this](){ handleWifiForUpdate(); });
            }, [this](){ handleWifiForUpdate(); });
        } else {
            showSettingsMenu();
        }
    }, [this](){ showSettingsMenu(); });
}

void SettingsController::handleWifiMenu() {
    std::vector<String> items;
    
    bool connected = (WiFi.status() == WL_CONNECTED);
    if (connected) {
        items.push_back("Disconnect: " + WiFi.SSID());
    }
    items.push_back("Scan Networks");
    items.push_back("Saved Networks");
    items.push_back("Manual Connect");

    _system.menu()->showMenu("WiFi Manager", items, [this, connected](int choice) {
        // Adjust index based on dynamic items
        int scanIdx = connected ? 1 : 0;
        int savedIdx = connected ? 2 : 1;
        int manIdx = connected ? 3 : 2;

        if (connected && choice == 0) {
            WiFi.disconnect();
            _system.menu()->showMessage("WiFi", "Disconnected", [this](){ handleWifiMenu(); });
            return;
        }

        if (choice == scanIdx) {
            _system.menu()->showMessage("WiFi", "Scanning Networks...", nullptr);
             
            // Auto-start scan in next loop (no wait for key)
            _system.menu()->setOnLoop([this](){
                 _system.menu()->setOnLoop(nullptr); 
                 
                 // Small delay to ensure "Scanning..." is rendered if we came from another view
                 // (Though Loop architecture usually renders before next update/input poll)
                 
                 auto networks = _system.wifi().scan(); // returns vector<WifiScanResult>
                 
                 if (networks.empty()) {
                     _system.menu()->showMessage("Scan", "No networks found", [this](){ handleWifiMenu(); });
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
                 
                 _system.menu()->showMenu("Scan Results", labels, [this, networks](int idx) {
                         if (idx < 0 || idx >= (int)networks.size()) return;

                         String ssid = networks[idx].ssid;
                         
                         // Check if known
                         auto saved = _system.wifi().getSavedNetworks();
                         String existingPass = "";
                         for(auto& s : saved) {
                             if(s.ssid == ssid) {
                                 existingPass = s.pass;
                                 break;
                             }
                         }

                         _system.menu()->showInput("Password", existingPass, true, [this, ssid](String pass) {
                             _system.menu()->showMessage("Connecting", "Please wait...", nullptr);
                             _system.menu()->setOnLoop([this, ssid, pass](){
                                 _system.menu()->setOnLoop(nullptr);
                                 if (_system.wifi().connectTo(ssid, pass)) {
                                     _system.wifi().save(ssid, pass);
                                     _system.menu()->showMessage("Success", "Connected!", [this](){ handleWifiMenu(); });
                                 } else {
                                     _system.menu()->showMessage("Error", "Connection Failed", [this](){ handleWifiMenu(); });
                                 }
                             });
                         }, [this](){ handleWifiMenu(); });
                     }, [this](){ handleWifiMenu(); });
                 });
            }
        else if (choice == savedIdx) {
            auto saved = _system.wifi().getSavedNetworks();
            if (saved.empty()) {
                _system.menu()->showMessage("Saved", "No saved networks", [this](){ handleWifiMenu(); });
                return;
            }
            std::vector<String> labels;
            for(auto& s : saved) labels.push_back(s.ssid);
            
            _system.menu()->showMenu("Saved Networks", labels, [this, saved](int idx) {
                String ssid = saved[idx].ssid;
                std::vector<String> opt = {"Connect", "Forget"};
                _system.menu()->showMenu(ssid, opt, [this, ssid, idx](int action){
                    if(action == 0) { // Connect
                        _system.menu()->showMessage("Connecting", "Please wait...", nullptr);
                        _system.menu()->setOnLoop([this, ssid, idx](){
                             _system.menu()->setOnLoop(nullptr);
                             auto savedFresh = _system.wifi().getSavedNetworks();
                             if (idx < savedFresh.size()) {
                                 if(_system.wifi().connectTo(savedFresh[idx].ssid, savedFresh[idx].pass)) {
                                      _system.menu()->showMessage("Success", "Connected!", [this](){ handleWifiMenu(); });
                                 } else {
                                      _system.menu()->showMessage("Error", "Failed", [this](){ handleWifiMenu(); });
                                 }
                             } else {
                                 handleWifiMenu();
                             }
                        });
                    } else { // Forget
                        _system.wifi().forget(idx);
                        _system.menu()->showMessage("WiFi", "Network Forgotten", [this](){ handleWifiMenu(); });
                    }
                }, [this](){ handleWifiMenu(); });
            }, [this](){ handleWifiMenu(); });
        }
        else if (choice == manIdx) {
            _system.menu()->showInput("SSID", "", false, [this](String ssid){
                _system.menu()->showInput("Password", "", true, [this, ssid](String pass){
                     _system.menu()->showMessage("Connecting", "Please wait...", nullptr);
                     _system.menu()->setOnLoop([this, ssid, pass](){
                         _system.menu()->setOnLoop(nullptr);
                         if (_system.wifi().connectTo(ssid, pass)) {
                             _system.wifi().save(ssid, pass);
                             _system.menu()->showMessage("Success", "Connected!", [this](){ handleWifiMenu(); });
                         } else {
                             _system.menu()->showMessage("Error", "Connection Failed", [this](){ handleWifiMenu(); });
                         }
                     });
                }, [this](){ handleWifiMenu(); });
            }, [this](){ handleWifiMenu(); });
        }
    }, [this](){ showSettingsMenu(); });
}
