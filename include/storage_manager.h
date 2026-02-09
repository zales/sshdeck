#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <vector>
#include "script_def.h"

class StorageManager {
public:
    StorageManager();
    
    // Checks if SD card is available and initializes if needed
    bool begin();
    
    // Script Management
    // Loads from /scripts.json
    void loadScripts(); 
    void saveScripts();
    
    std::vector<Script>& getScripts();
    void addScript(const Script& script);
    void updateScript(int index, const Script& script);
    void deleteScript(int index);

    // Toggles USB MSC Mode
    // returns true if started, false if failed/stopped
    bool startUSBMode();
    void stopUSBMode();
    
    // Check/Clear external eject requests (from Host OS)
    bool isEjectRequested();
    void clearEjectRequest();

    // Scan the internal RAM disk for key data
    String scanRAMDiskForKey();

    // Key Management
    String readSSHKey(const String& filename = "/id_rsa");

    bool isUSBActive();
    
private:
    bool isMounted;
    std::vector<Script> _scripts;
};
