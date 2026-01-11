#include "wifi_setup.h"

extern void drawTerminalScreen();

WifiSetup::WifiSetup(TerminalEmulator& term, KeyboardManager& kb, DisplayManager& disp)
    : terminal(term), keyboard(kb), display(disp) {
}

void WifiSetup::loadCredentials() {
    Serial.println("Loading credentials...");
    preferences.begin("tdeck-wifi", true); // Read-only mode
    
    // Clear list
    for(int i=0; i<MAX_SAVED_NETWORKS; i++) {
        savedNetworks[i].ssid = "";
        savedNetworks[i].pass = "";
    }
    
    // Load up to MAX existing networks
    for(int i=0; i<MAX_SAVED_NETWORKS; i++) {
        String s = preferences.getString(("ssid_" + String(i)).c_str(), "");
        String p = preferences.getString(("pass_" + String(i)).c_str(), "");
        if (s.length() > 0) {
            savedNetworks[i].ssid = s;
            savedNetworks[i].pass = p;
            Serial.printf("Loaded [%d]: %s\n", i, s.c_str());
        }
    }
    
    // Fallback: If no indexed keys, check legacy keys and migrate to index 0
    if (savedNetworks[0].ssid.length() == 0) {
        String legacyW = preferences.getString("ssid", "");
        String legacyP = preferences.getString("pass", "");
        if (legacyW.length() > 0) {
             Serial.println("Migrating legacy credentials to slot 0");
             savedNetworks[0].ssid = legacyW;
             savedNetworks[0].pass = legacyP;
             // We don't save back immediately, we just use them in memory.
             // They will be saved properly if updated or on next save.
             // Actually good to save them now to avoid confusion later?
             // But we are in read-only mode here.
        }
    }

    lastUsedIndex = preferences.getInt("last_index", 0);
    if(lastUsedIndex < 0 || lastUsedIndex >= MAX_SAVED_NETWORKS) lastUsedIndex = 0;
    
    preferences.end();
}

void WifiSetup::saveCredentials(const String& ssid, const String& pass) {
    if (pass.length() == 0) return; // Don't save empty
    
    preferences.begin("tdeck-wifi", false); // Read-write
    
    // Check if SSID already exists
    int targetSlot = -1;
    for(int i=0; i<MAX_SAVED_NETWORKS; i++) {
        // We need to reload checking from memory or preferenes? 
        // We have local copy in savedNetworks[]
        if (savedNetworks[i].ssid == ssid) {
            targetSlot = i;
            break;
        }
    }
    
    // If not found, look for empty slot
    if (targetSlot == -1) {
        for(int i=0; i<MAX_SAVED_NETWORKS; i++) {
            if (savedNetworks[i].ssid.length() == 0) {
                targetSlot = i;
                break;
            }
        }
    }
    
    // If still not found (full), overwrite last used index? Or round robin?
    // Let's overwrite index 0 if full, unless index 0 is very recent?
    // Simplest: Round robin based on lastUsedIndex? No.
    // Let's just overwrite the OLDEST? We don't track age.
    // Let's overwrite the 'next' slot after lastUsedIndex?
    if (targetSlot == -1) {
        targetSlot = (lastUsedIndex + 1) % MAX_SAVED_NETWORKS;
    }
    
    // Save to slot
    preferences.putString(("ssid_" + String(targetSlot)).c_str(), ssid);
    preferences.putString(("pass_" + String(targetSlot)).c_str(), pass);
    
    // Update last index
    preferences.putInt("last_index", targetSlot);
    
    // Update local cache
    savedNetworks[targetSlot].ssid = ssid;
    savedNetworks[targetSlot].pass = pass;
    lastUsedIndex = targetSlot;
    
    Serial.printf("Saved to slot %d: %s\n", targetSlot, ssid.c_str());
    
    preferences.end();
}

void WifiSetup::refreshScreen() {
    // Force update by setting flag
    // terminal.needsUpdate() returns the flag. We might need to force it if we just want to redraw
    // But drawTerminalScreen() checks the flag? 
    // In main.cpp:
    // if (terminal.needsUpdate()) { drawTerminalScreen(); }
    // But drawTerminalScreen() implementation clears the flag at the end.
    // If we call drawTerminalScreen directly, we don't need to check the flag, it just draws.
    // But drawTerminalScreen implementation assumes it acts on 'terminal'.
    
    drawTerminalScreen();
}


bool WifiSetup::connect() {
    loadCredentials();
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    // UI: List Saved Networks
    terminal.clear(); 
    terminal.appendString("=== WiFi Setup ===\n");
    terminal.appendString("Select Saved Network or SPACE to scan:\n");
    
    int rowOffset = 2; // Header lines
    int lineMap[MAX_SAVED_NETWORKS];
    for(int k=0; k<MAX_SAVED_NETWORKS; k++) lineMap[k] = -1;

    int validCount = 0;
    int currentLine = rowOffset;
    
    // Print saved networks
    for(int i=0; i<MAX_SAVED_NETWORKS; i++) {
        if (savedNetworks[i].ssid.length() > 0) {
            String line = "[" + String(i+1) + "] " + savedNetworks[i].ssid + "\n";
            terminal.appendString(line.c_str());
            
            if (currentLine - rowOffset < MAX_SAVED_NETWORKS) {
                lineMap[currentLine - rowOffset] = i;
            }
            currentLine++;
            validCount++;
        }
    }
    
    terminal.appendString("\nConnecting to last used in 5s...\n");
    refreshScreen();

    // Determine default connection (Last Used)
    int defaultIndex = -1;
    if (lastUsedIndex >= 0 && lastUsedIndex < MAX_SAVED_NETWORKS) {
        if (savedNetworks[lastUsedIndex].ssid.length() > 0) {
            defaultIndex = lastUsedIndex;
        }
    }
    // If no last used, pick first valid
    if (defaultIndex == -1 && validCount > 0) {
        for(int i=0; i<MAX_SAVED_NETWORKS; i++) {
            if (savedNetworks[i].ssid.length() > 0) {
                defaultIndex = i;
                break;
            }
        }
    }
    
    bool shouldScan = false;
    int selectedIndex = -1;
    
    unsigned long start = millis();
    // Wait for selection
    while (millis() - start < 5000) {
        // Keyboard: Space to scan
        if (keyboard.isKeyPressed()) {
            char c = keyboard.getKeyChar();
            if (c == ' ') { 
                shouldScan = true; 
                break; 
            }
        }
        
        // Touch: Select from list
        // int tx, ty;
        // if (touch.getPoint(tx, ty)) {
        //      // Map Y to Row (Approximate based on main.cpp layout: y_start=24, line_h=10)
        //      // Row 0 is roughly Y=[14..24]
        //      int row = (ty - 14) / 10;
             
        //      int mapIdx = row - rowOffset;
        //      if (mapIdx >= 0 && mapIdx < MAX_SAVED_NETWORKS) {
        //          int netIdx = lineMap[mapIdx];
        //          if (netIdx != -1) {
        //              selectedIndex = netIdx;
        //              terminal.appendString(("Selected: " + savedNetworks[selectedIndex].ssid + "\n").c_str());
        //              refreshScreen();
        //              break;
        //          }
        //      }
        // }
        delay(20);
    }
    
    // Handle Auto-Connect Timeout
    if (!shouldScan && selectedIndex == -1) {
        if (defaultIndex != -1) {
             selectedIndex = defaultIndex;
        } else {
             // No saved networks? Scan.
             shouldScan = true;
        }
    }
    
    // Execute Connection
    if (!shouldScan && selectedIndex != -1) {
        if (tryConnect(savedNetworks[selectedIndex].ssid, savedNetworks[selectedIndex].pass)) {
             // Update Last Used
             if (lastUsedIndex != selectedIndex) {
                 // Save 'last_index' only? 
                 preferences.begin("tdeck-wifi", false);
                 preferences.putInt("last_index", selectedIndex);
                 preferences.end();
                 lastUsedIndex = selectedIndex;
             }
             return true;
        } else {
            terminal.appendString("Connection Failed.\n");
            delay(1000);
            shouldScan = true; // Fallback to scan
        }
    }

    if (shouldScan) {
        scanAndSelect();
        return WiFi.status() == WL_CONNECTED;
    }
    
    return false;
}

bool WifiSetup::tryConnect(const String& ssid, const String& pass) {
    terminal.appendString(("Connecting to " + ssid + "...\n").c_str());
    refreshScreen();
    
    WiFi.begin(ssid.c_str(), pass.c_str());
    
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 15000) { // 15s timeout
            return false;
        }
        delay(500);
        terminal.appendString(".");
        refreshScreen();
    }
    
    terminal.appendString(("\nConnected! IP: " + WiFi.localIP().toString() + "\n").c_str());
    refreshScreen();
    // Connected
    return true;
}

void WifiSetup::scanAndSelect() {
    terminal.appendString("\nScanning Networks...\n");
    refreshScreen();
    
    int n = WiFi.scanNetworks();
    if (n == 0) {
        terminal.appendString("No networks found.\n");
        refreshScreen();
        delay(2000);
        return; 
    }
    
    terminal.clear();
    terminal.appendString(("Found " + String(n) + " networks:\n").c_str());
    for (int i = 0; i < n && i < 9; ++i) { // Show top 9
        String msg = String(i + 1) + ": " + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + ")\n";
        terminal.appendString(msg.c_str());
    }
    terminal.appendString(("Select (1-" + String(min(n, 9)) + ") or r to rescan:\n").c_str());
    refreshScreen();
    
    // Wait for selection
    int selected = -1;
    while (selected == -1) {
        if (keyboard.isKeyPressed()) {
            char c = keyboard.getKeyChar();
            if (c >= '1' && c <= '9') {
                int index = c - '1';
                if (index < n) {
                    selected = index;
                }
            } else if (c == 'r' || c == 'R') {
                scanAndSelect(); // recursive retry
                return;
            }
        }
        delay(50);
    }
    
    String targetSSID = WiFi.SSID(selected);
    terminal.appendString(("\nSelected: " + targetSSID + "\n").c_str());
    
    // Check if we already have this SSID saved
    String passToUse = "";
    for(int i=0; i<MAX_SAVED_NETWORKS; i++) {
        if(savedNetworks[i].ssid == targetSSID) {
            passToUse = savedNetworks[i].pass;
            terminal.appendString("Using saved password.\n");
            break;
        }
    }
    
    if (passToUse.length() > 0) {
        // Ask if we want to use it or change it?
        // Simple: Just try to connect. If fails, ask again?
        // Let's Just use it.
    } else {
        terminal.appendString("Enter Password: ");
        refreshScreen();
        passToUse = readInput(false); // Unmasked
    }

    terminal.appendString("\nSaving & Connecting...\n");
    refreshScreen();
    
    saveCredentials(targetSSID, passToUse);
    
    if (!tryConnect(targetSSID, passToUse)) {
        terminal.appendString("Connection Failed.\nRetry? (y/n)");
        refreshScreen();
        // If wrong password, we might want to clear it or allow entry?
        // Just rescanning is safest loop.
        while(true) {
            if(keyboard.isKeyPressed()) {
                char c = keyboard.getKeyChar();
                if(c == 'y') { scanAndSelect(); return; }
                if(c == 'n') return;
            }
        }
    }
}

String WifiSetup::readInput(bool passwordMask) {
    String input = "";
    while (true) {
        if (keyboard.isKeyPressed()) {
            char c = keyboard.getKeyChar();
            
            if (c == 0) continue; // Ignore modifiers/non-char keys

            if (c == 0x0D || c == '\n') { // Enter
                return input;
            } else if (c == 0x08 || c == 127 || c == '\b') { // Backspace
                if (input.length() > 0) {
                    input.remove(input.length() - 1);
                    terminal.appendString("\b"); 
                    terminal.appendString(" ");
                    terminal.appendString("\b");
                }
            } else {
                input += c;
                if (passwordMask) {
                    terminal.appendString("*");
                } else {
                    terminal.appendString(String(c).c_str());
                }
            }
            refreshScreen();
        }
        delay(50);
    }
}
