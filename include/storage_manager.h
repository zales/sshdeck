#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SD.h>

class StorageManager {
public:
    StorageManager();
    
    // Checks if SD card is available and initializes if needed
    bool begin();
    
    // Toggles USB MSC Mode
    // returns true if started, false if failed/stopped
    bool startUSBMode();
    void stopUSBMode();

    // Key Management
    String readSSHKey(const String& filename = "/id_rsa");
    
private:
    bool isMounted;
};
