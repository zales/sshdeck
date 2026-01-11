#include "storage_manager.h"
#include "board_def.h"

StorageManager::StorageManager() : isMounted(false) {}

bool StorageManager::begin() {
    // SPI bus is shared, ensure CS is high before init if needed, 
    // but SD.begin handles low-level
    pinMode(BOARD_SD_CS, OUTPUT);
    return SD.begin(BOARD_SD_CS, SPI);
}

bool StorageManager::startUSBMode() {
    // USB MSC with SPI SD card is not supported in this configuration currently.
    // Use SD card reader on PC to transfer files.
    return false;
}

void StorageManager::stopUSBMode() {
    isMounted = false;
}

String StorageManager::readSSHKey(const String& filename) {
    if (!begin()) return "";
    
    if (SD.exists(filename)) {
        File f = SD.open(filename);
        if (f) {
            String s = f.readString();
            f.close();
            // Basic cleanup of key
            s.trim();
            return s;
        }
    }
    return "";
}
