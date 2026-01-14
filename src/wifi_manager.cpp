#include "wifi_manager.h"
#include <vector>
#include <functional>
#include <algorithm>

WifiManager::WifiManager(TerminalEmulator& term, KeyboardManager& kb, UIManager& ui_mgr, PowerManager& pwr)
    : terminal(term), keyboard(kb), ui(ui_mgr), power(pwr), security(nullptr) {
}

void WifiManager::setSecurityManager(SecurityManager* sec) {
    security = sec;
}

void WifiManager::setIdleCallback(std::function<void()> cb) {
    idleCallback = cb;
}

void WifiManager::setRenderCallback(std::function<void()> cb) {
    renderCallback = cb;
}

void WifiManager::loadCredentials() {
    preferences.begin("tdeck-wifi", true); // RO
    maxSavedIndex = preferences.getInt("count", 0);
    
    // Safety clamp
    if (maxSavedIndex > MAX_SAVED_NETWORKS) maxSavedIndex = MAX_SAVED_NETWORKS; 
    
    for (int i=0; i<maxSavedIndex; i++) {
        String s = preferences.getString(("ssid" + String(i)).c_str(), "");
        String rawPass = preferences.getString(("pass" + String(i)).c_str(), "");
        String p = rawPass;
        
        if (security && !rawPass.isEmpty()) {
            String decrypted = security->decrypt(rawPass);
            // If decrypt fail but raw not empty -> assume legacy plaintext
            if (decrypted.isEmpty() && !rawPass.isEmpty()) {
                p = rawPass;
            } else {
                p = decrypted;
            }
        }
        
        savedNetworks[i].ssid = s;
        savedNetworks[i].pass = p;
    }
    
    lastUsedIndex = preferences.getInt("last_index", -1);
    preferences.end();
}

void WifiManager::saveCredentials(const String& ssid, const String& pass) {
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
        String storePass = security ? security->encrypt(pass) : pass;
        preferences.putString(("pass" + String(existingIndex)).c_str(), storePass.c_str());
    } else {
        // Add new
        if (maxSavedIndex < MAX_SAVED_NETWORKS) {
            savedNetworks[maxSavedIndex].ssid = ssid;
            savedNetworks[maxSavedIndex].pass = pass;
            preferences.putString(("ssid" + String(maxSavedIndex)).c_str(), ssid);
            
            String storePass = security ? security->encrypt(pass) : pass;
            preferences.putString(("pass" + String(maxSavedIndex)).c_str(), storePass.c_str());
            
            maxSavedIndex++;
            preferences.putInt("count", maxSavedIndex);
        } else {
             // Full, replace last (simplified)
             int idx = MAX_SAVED_NETWORKS - 1;
             savedNetworks[idx].ssid = ssid;
             savedNetworks[idx].pass = pass;
             preferences.putString(("ssid" + String(idx)).c_str(), ssid);
             
             String storePass = security ? security->encrypt(pass) : pass;
             preferences.putString(("pass" + String(idx)).c_str(), storePass.c_str());
        }
    }
    preferences.end();
}

void WifiManager::reEncryptAll() {
    loadCredentials();
    preferences.begin("tdeck-wifi", false);
    for(int i=0; i<maxSavedIndex; i++) {
         String p = savedNetworks[i].pass;
         String storePass = security ? security->encrypt(p) : p;
         preferences.putString(("pass" + String(i)).c_str(), storePass.c_str());
    }
    preferences.end();
}

// Replaced Logic with Public API
std::vector<WifiManager::WifiScanResult> WifiManager::scan() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    // Non-blocking scan? No, scanNetworks is blocking by default.
    int n = WiFi.scanNetworks();
    std::vector<WifiScanResult> res;
    
    if (n > 0) {
        for (int i = 0; i < n; i++) {
            String ssid = WiFi.SSID(i);
            if (ssid.length() == 0) continue;

            // Simple deduplication: Keep strongest
            bool found = false;
            for(auto& existing : res) {
                if(existing.ssid == ssid) {
                    found = true;
                    // If current is stronger, update
                    if(WiFi.RSSI(i) > existing.rssi) {
                         existing.rssi = WiFi.RSSI(i);
                         existing.secure = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
                    }
                    break;
                }
            }

            if (!found) {
                WifiScanResult r;
                r.ssid = ssid;
                r.rssi = WiFi.RSSI(i);
                r.secure = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
                res.push_back(r);
            }
        }
    }
    
    // Sort by RSSI descending
    std::sort(res.begin(), res.end(), [](const WifiScanResult& a, const WifiScanResult& b) {
        return a.rssi > b.rssi;
    });
    
    return res;
}

std::vector<WifiManager::WifiInfo> WifiManager::getSavedNetworks() {
    loadCredentials();
    std::vector<WifiInfo> res;
    for(int i=0; i<maxSavedIndex; i++) {
        res.push_back({savedNetworks[i].ssid, savedNetworks[i].pass});
    }
    return res;
}

bool WifiManager::connectTo(const String& ssid, const String& pass) {
    if (ssid.length() == 0) return false;

    // Feedback for user
    ui.drawConnectingScreen(ssid, pass, power.getPercentage(), power.isCharging());

    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("ssh-deck");
    
    WiFi.begin(ssid.c_str(), pass.c_str());
    
    unsigned long start = millis();
    while (millis() - start < 15000) { // 15s timeout
        if (WiFi.status() == WL_CONNECTED) {
            // Sync time for SSL verification
            configTime(0, 0, "pool.ntp.org", "time.nist.gov");
            return true;
        }
        
        // Call callbacks to keep UI alive (mostly feeding watchdogs if any)
        if (idleCallback) idleCallback();
        
        delay(100);
    }
    return false;
}

void WifiManager::save(const String& ssid, const String& pass) {
    saveCredentials(ssid, pass);
    // Remember last used
    for(int i=0; i<maxSavedIndex; i++) {
        if (savedNetworks[i].ssid == ssid) {
             preferences.begin("tdeck-wifi", false);
             preferences.putInt("last_index", i);
             preferences.end();
             break;
        }
    }
}

void WifiManager::forget(int index) {
    deleteCredential(index);
}


void WifiManager::deleteCredential(int index) {
    if (index < 0 || index >= maxSavedIndex) return;
    
    preferences.begin("tdeck-wifi", false);
    
    // Shift down
    for (int i = index; i < maxSavedIndex - 1; i++) {
        savedNetworks[i] = savedNetworks[i+1];
        preferences.putString(("ssid" + String(i)).c_str(), savedNetworks[i].ssid);
        preferences.putString(("pass" + String(i)).c_str(), savedNetworks[i].pass);
    }
    
    maxSavedIndex--;
    preferences.putInt("count", maxSavedIndex);
    preferences.end();
}

bool WifiManager::connect() {
    loadCredentials();
    if (lastUsedIndex >= 0 && lastUsedIndex < maxSavedIndex) {
        return connectTo(savedNetworks[lastUsedIndex].ssid, savedNetworks[lastUsedIndex].pass);
    }
    return false; // No auto-connect target
}
