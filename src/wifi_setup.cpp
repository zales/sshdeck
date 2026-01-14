#if 0
#include "wifi_setup.h"
#include "ui/menu_system.h" 
#include <vector>
#include <functional>

extern void drawTerminalScreen();

WifiSetup::WifiSetup(TerminalEmulator& term, KeyboardManager& kb, DisplayManager& disp)
    : terminal(term), keyboard(kb), display(disp) {
}

void WifiSetup::setIdleCallback(std::function<void()> cb) {
    idleCallback = cb;
}

void WifiSetup::loadCredentials() {
    preferences.begin("tdeck-wifi", true); // RO
    maxSavedIndex = preferences.getInt("count", 0);
    
    // Safety clamp
    if (maxSavedIndex > MAX_SAVED_NETWORKS) maxSavedIndex = MAX_SAVED_NETWORKS; 
    
    for (int i=0; i<maxSavedIndex; i++) {
        String s = preferences.getString(("ssid" + String(i)).c_str(), "");
        String p = preferences.getString(("pass" + String(i)).c_str(), "");
        savedNetworks[i].ssid = s;
        savedNetworks[i].pass = p;
    }
    
    lastUsedIndex = preferences.getInt("last_index", -1);
    preferences.end();
}

void WifiSetup::saveCredentials(const String& ssid, const String& pass) {
    preferences.begin("tdeck-wifi", false); // RW
    
    // Check if exists
    int existingIndex = -1;
    for(int i=0; i<maxSavedIndex; i++) {
        if (savedNetworks[i].ssid == ssid) {
            existingIndex = i;
            break;
        }
    }
    
    if (existingIndex != -1) {
        // Update password
        savedNetworks[existingIndex].pass = pass;
        preferences.putString(("pass" + String(existingIndex)).c_str(), pass);
    } else {
        // Add new
        if (maxSavedIndex < MAX_SAVED_NETWORKS) {
            savedNetworks[maxSavedIndex].ssid = ssid;
            savedNetworks[maxSavedIndex].pass = pass;
            preferences.putString(("ssid" + String(maxSavedIndex)).c_str(), ssid);
            preferences.putString(("pass" + String(maxSavedIndex)).c_str(), pass);
            maxSavedIndex++;
            preferences.putInt("count", maxSavedIndex);
        } else {
             // Full, replace oldest (circular)? Or just last?
             int idx = MAX_SAVED_NETWORKS - 1;
             savedNetworks[idx].ssid = ssid;
             savedNetworks[idx].pass = pass;
             preferences.putString(("ssid" + String(idx)).c_str(), ssid);
             preferences.putString(("pass" + String(idx)).c_str(), pass);
        }
    }
    preferences.end();
}

void WifiSetup::refreshScreen() {
    drawTerminalScreen();
}

// Helper to enter text using the MenuSystem's UI style
String WifiSetup::readInput(const String& prompt, bool passwordMask) {
    UIManager ui(display);
    MenuSystem menu(ui, keyboard);
    menu.setIdleCallback(idleCallback);
    String result = "";
    if (menu.textInput(prompt, result, passwordMask)) {
        return result;
    }
    return "";
}

bool WifiSetup::connect() {
    loadCredentials();
    UIManager ui(display);
    MenuSystem menu(ui, keyboard);
    menu.setIdleCallback(idleCallback);

    // Auto-connect attempt
    if (lastUsedIndex >= 0 && lastUsedIndex < MAX_SAVED_NETWORKS) {
        if (savedNetworks[lastUsedIndex].ssid.length() > 0) {
            unsigned long start = millis();
            bool abort = false;
            
            int lastRemain = -1;
            while (millis() - start < 3000) {
                int remain = 3 - (millis() - start)/1000;
                if (remain != lastRemain) {
                    lastRemain = remain;
                    display.setRefreshMode(true);
                    
                    display.getDisplay().firstPage();
                    do {
                        display.getDisplay().fillScreen(GxEPD_WHITE);
                        int w = display.getWidth();
                        auto& u8g2 = display.getFonts();
                        
                        // Title Bar
                        display.getDisplay().fillRect(0, 0, w, 24, GxEPD_BLACK);
                        u8g2.setForegroundColor(GxEPD_WHITE);
                        u8g2.setBackgroundColor(GxEPD_BLACK);
                        u8g2.setFont(u8g2_font_helvB12_tr);
                        u8g2.setCursor(5, 18);
                        u8g2.print("Wifi Setup");
                        
                        // Content
                        u8g2.setForegroundColor(GxEPD_BLACK);
                        u8g2.setBackgroundColor(GxEPD_WHITE);
                        u8g2.setFont(u8g2_font_helvB10_tr);
                        
                        u8g2.setCursor(10, 50);
                        u8g2.print("Auto-Connecting...");

                        u8g2.setFont(u8g2_font_helvR12_tr);
                        u8g2.setCursor(10, 80);
                        u8g2.print(savedNetworks[lastUsedIndex].ssid);
                        
                        // Countdown and Cancel instruction
                        u8g2.setFont(u8g2_font_helvR10_tr);
                        u8g2.setCursor(10, 120);
                        u8g2.print("Start in: " + String(remain) + "s");
                        
                        u8g2.setCursor(10, 150);
                        u8g2.print("Press 'q' or 'Mic+Q' to cancel");
                        
                    } while (display.getDisplay().nextPage());
                }
                
                if (keyboard.isKeyPressed()) {
                    char c = keyboard.getKeyChar();
                    // Cancel on 'q' or ESC (Mic+Q is ESC)
                    if (c == 'q' || c == 0x1B) { 
                        abort = true;
                        break;
                    }
                }
                delay(50);
            }
            
            if (!abort) {
                if (tryConnect(savedNetworks[lastUsedIndex].ssid, savedNetworks[lastUsedIndex].pass)) {
                    return true; 
                }
            }
        }
    }

    // Main Loop
    while (true) {
        std::vector<String> labels;
        std::vector<std::function<void()>> actions;

        // 1. Scan
        labels.push_back("Scan Networks");
        actions.push_back([this](){ scanAndSelect(); });

        // 2. Saved Networks
        for (int i=0; i<maxSavedIndex; i++) {
            if (savedNetworks[i].ssid.length() > 0) {
                String label = "Saved: " + savedNetworks[i].ssid;
                int idx = i; 
                labels.push_back(label);
                actions.push_back([this, idx](){ 
                    tryConnect(savedNetworks[idx].ssid, savedNetworks[idx].pass);
                });
            }
        }
        
        // 3. Exit
        labels.push_back("Exit (Offline)");
        actions.push_back([](){}); 

        int choice = menu.showMenu("WiFi Manager", labels);
        
        // Check connectivity first, maybe an action connected us
        if (WiFi.status() == WL_CONNECTED) {
             String currentSSID = WiFi.SSID();
             for(int i=0; i<maxSavedIndex; i++) {
                 if(savedNetworks[i].ssid == currentSSID) {
                      preferences.begin("tdeck-wifi", false);
                      preferences.putInt("last_index", i);
                      preferences.end();
                      break;
                 }
             }
            return true;
        }

        if (choice < 0) return false; // ESC
        
        // Execute Action
        if (choice < actions.size()) {
             actions[choice]();
        }
        
        // Final Check
        if (WiFi.status() == WL_CONNECTED) {
             // Save last index if needed
             String currentSSID = WiFi.SSID();
             for(int i=0; i<maxSavedIndex; i++) {
                 if(savedNetworks[i].ssid == currentSSID) {
                      preferences.begin("tdeck-wifi", false);
                      preferences.putInt("last_index", i);
                      preferences.end();
                      break;
                 }
             }
             return true;
        }
        
        // If Exit selected
        if (choice == labels.size() - 1) { 
            return false;
        }
    }
}

bool WifiSetup::tryConnect(const String& ssid, const String& pass) {
    display.getDisplay().firstPage();
    do {
         display.getDisplay().fillScreen(GxEPD_WHITE);
         
         auto& u8g2 = display.getFonts();
         u8g2.setFont(u8g2_font_6x10_tr);
         u8g2.setForegroundColor(GxEPD_BLACK);
         u8g2.setBackgroundColor(GxEPD_WHITE);
         
         u8g2.setCursor(10, 30);
         u8g2.print("Connecting...");
         u8g2.setCursor(10, 45);
         u8g2.print(ssid.c_str());
    } while (display.getDisplay().nextPage());
    
    // Set Hostname before connecting (must be done before WiFi.begin() or mode() in some versions, 
    // but safe to do here)
    WiFi.setHostname("ssh-deck");
    WiFi.begin(ssid.c_str(), pass.c_str());
    
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 10000) { // 10s timeout
             return false;
        }
        delay(500);
    }
    return true;
}

void WifiSetup::scanAndSelect() {
    display.getDisplay().firstPage();
    do {
         display.getDisplay().fillScreen(GxEPD_WHITE);
         
         auto& u8g2 = display.getFonts();
         u8g2.setFont(u8g2_font_helvB12_tr);
         u8g2.setForegroundColor(GxEPD_BLACK);
         u8g2.setBackgroundColor(GxEPD_WHITE);
         
         // Center text
         int w = display.getWidth();
         String title = "Scanning...";
         int tw = u8g2.getUTF8Width(title.c_str());
         u8g2.setCursor((w - tw) / 2, 120);
         u8g2.print(title);
    } while (display.getDisplay().nextPage());
    
    int n = WiFi.scanNetworks();
    
    if (n == 0) {
        UIManager ui(display);
        MenuSystem menu(ui, keyboard);
        menu.drawMessage("Scan Results", "No Networks Found");
        return; 
    }
    
    // Sort and Deduplicate
    struct ScanResult {
        String ssid;
        int rssi;
        bool secure;
    };
    std::vector<ScanResult> results;
    
    for (int i=0; i<n; ++i) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;
        
        bool found = false;
        for(auto& r : results) {
            if (r.ssid == ssid) {
                if (WiFi.RSSI(i) > r.rssi) r.rssi = WiFi.RSSI(i); // Keep stronger
                found = true;
                break;
            }
        }
        
        if (!found) {
            bool sec = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
            results.push_back({ssid, (int)WiFi.RSSI(i), sec});
        }
    }
    
    // Sort by RSSI descending
    std::sort(results.begin(), results.end(), [](const ScanResult& a, const ScanResult& b) {
        return a.rssi > b.rssi;
    });
    
    UIManager ui(display);
    MenuSystem menu(ui, keyboard);
    menu.setIdleCallback(idleCallback);
    while (true) {
        std::vector<String> labels;
        for(const auto& r : results) {
            String l = r.ssid;
            // Pad for alignment if needed, or just append info
            // Signal Bars approximation
            String signal = (r.rssi > -60) ? "[====]" : (r.rssi > -70) ? "[=== ]" : (r.rssi > -80) ? "[==  ]" : "[=   ]";
            String lock = r.secure ? "*" : " ";
            labels.push_back(l + " " + lock + " " + signal);
        }
        labels.push_back("[ Rescan ]");
        
        int choice = menu.showMenu("Scan Results", labels);
        
        if (choice < 0) return; // ESC
        
        if (choice == labels.size() - 1) { // Rescan
            scanAndSelect();
            return;
        }
        
        if (choice < results.size()) {
            String ssid = results[choice].ssid;
            
            // Check if already saved
            String pass = "";
            bool known = false;
            for(int k=0; k<maxSavedIndex; k++) {
                if(savedNetworks[k].ssid == ssid) {
                    pass = savedNetworks[k].pass;
                    known = true;
                    break;
                }
            }
            
            if (!known && results[choice].secure) {
                if (!menu.textInput("Password", pass, true)) {
                    continue; // Cancelled password entry
                }
            }
            
            if (tryConnect(ssid, pass)) {
                saveCredentials(ssid, pass);
                menu.drawMessage("Success", "Connected!");
                // Update last index
                for(int k=0; k<maxSavedIndex; k++) {
                    if(savedNetworks[k].ssid == ssid) {
                        preferences.begin("tdeck-wifi", false);
                        preferences.putInt("last_index", k);
                        preferences.end();
                        break;
                    }
                }
                return;
            } else {
                menu.drawMessage("Error", "Connection Failed");
            }
        }
    }
}

String WifiSetup::readInput(bool passwordMask) {
    return readInput("Enter Value:", passwordMask);
}

void WifiSetup::deleteCredential(int index) {
    if (index < 0 || index >= maxSavedIndex) return;
    
    preferences.begin("tdeck-wifi", false);
    
    // Shift down
    for (int i = index; i < maxSavedIndex - 1; i++) {
        savedNetworks[i] = savedNetworks[i+1];
        preferences.putString(("ssid" + String(i)).c_str(), savedNetworks[i].ssid);
        preferences.putString(("pass" + String(i)).c_str(), savedNetworks[i].pass);
    }
    
    // Clear last one (optional, but good practice to avoid ghosts if count mismatch)
    // Actually just decrementing count is enough for logic, but let's be clean
    // NVS keys for the last one will just be overwritten next time.
    
    maxSavedIndex--;
    preferences.putInt("count", maxSavedIndex);
    preferences.end();
}

void WifiSetup::manage() {
    loadCredentials();
    UIManager ui(display);
    MenuSystem menu(ui, keyboard);
    menu.setIdleCallback(idleCallback);
    
    while(true) {
        std::vector<String> items;
        // Header Actions
        items.push_back("[ Scan Networks ]");
        items.push_back("[ Add Manually ]");
        
        // List Saved
        for(int i=0; i<maxSavedIndex; i++) {
            String label = savedNetworks[i].ssid;
            // Mark connected
            if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == label) {
                label = "> " + label + " (Active)";
            }
            items.push_back(label);
        }
        
        int choice = menu.showMenu("WiFi Manager", items);
        
        if (choice < 0) return; // ESC/Back
        
        if (choice == 0) {
            scanAndSelect();
            // Reload in case scan added something
            loadCredentials();
        }
        else if (choice == 1) {
            String s, p;
            if (menu.textInput("SSID", s)) {
                if (s.length() > 0) {
                     menu.textInput("Password", p, true); // Allow empty pass
                     if (tryConnect(s, p)) {
                         saveCredentials(s, p);
                         menu.drawMessage("Success", "Connected & Saved");
                     } else {
                         // Save anyway?
                         std::vector<String> yn = {"Yes", "No"};
                         if (menu.showMenu("Connect Failed. Save?", yn) == 0) {
                             saveCredentials(s, p);
                         }
                     }
                     loadCredentials();
                }
            }
        }
        else {
            // Saved Network Action
            int idx = choice - 2;
            if (idx >= 0 && idx < maxSavedIndex) {
                String sel = savedNetworks[idx].ssid;
                
                std::vector<String> acts;
                acts.push_back("Connect");
                acts.push_back("Edit Password");
                acts.push_back("Delete");
                
                int act = menu.showMenu(sel, acts);
                
                if (act == 0) { // Connect
                    if(tryConnect(savedNetworks[idx].ssid, savedNetworks[idx].pass)) {
                        menu.drawMessage("Success", "Connected");
                        // Save usage preference
                         preferences.begin("tdeck-wifi", false);
                         preferences.putInt("last_index", idx);
                         preferences.end();
                        return;
                    } else {
                        menu.drawMessage("Error", "Connect Failed");
                    }
                }
                else if (act == 1) { // Edit
                    String p = savedNetworks[idx].pass;
                    if (menu.textInput("New Password", p, true)) {
                        saveCredentials(sel, p);
                        loadCredentials();
                        menu.drawMessage("Saved", "Password Updated");
                    }
                }
                else if (act == 2) { // Delete
                    std::vector<String> yn = {"No", "Yes"};
                    if (menu.showMenu("Delete " + sel + "?", yn) == 1) {
                         deleteCredential(idx);
                         loadCredentials(); // Refresh memory just in case
                         // loadCredentials actually reads from NVS, deleteCredential updates arrays too but simplest to be in sync
                    }
                }
            }
        }
    }
}
#endif
