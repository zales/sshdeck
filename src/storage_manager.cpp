#include "storage_manager.h"
#include "board_def.h"
#include "USB.h"
#include "USBMSC.h"

// RAM Disk Configuration
// 128KB Disk (256 sectors). 
#define RAM_DISK_SECTOR_SIZE 512
#define RAM_DISK_SECTOR_COUNT 256

static uint8_t* ramDiskBuffer = nullptr;
USBMSC MSC;
bool mscStarted = false;

static volatile bool ejectRequested = false;

// Callback for Eject/Start/Stop events
static bool onStartStop(uint8_t power_condition, bool start, bool load_eject) {
    if (load_eject || !start) {
        ejectRequested = true;
    }
    return true;
}

static void formatRAMDisk() {
    if (!ramDiskBuffer) return;
    
    // Clear buffer first
    memset(ramDiskBuffer, 0, RAM_DISK_SECTOR_COUNT * RAM_DISK_SECTOR_SIZE);

    // 1. Boot Sector (LBA 0)
    uint8_t* bs = ramDiskBuffer;
    
    // Jump Code
    bs[0] = 0xEB; bs[1] = 0x3C; bs[2] = 0x90; // JMP SHORT 3C, NOP
    // OEM Name
    memcpy(&bs[3], "MSDOS5.0", 8);
    // Bytes Per Sector: 512
    bs[11] = 0x00; bs[12] = 0x02;
    // Sectors Per Cluster: 1
    bs[13] = 0x01;
    // Reserved Sectors: 1
    bs[14] = 0x01; bs[15] = 0x00;
    // Number of FATs: 2
    bs[16] = 0x02;
    // Root Entries: 64 
    bs[17] = 0x40; bs[18] = 0x00;
    // Total Sectors (16-bit): 256
    bs[19] = 0x00; bs[20] = 0x01; // 0x0100 = 256
    // Media Descriptor: 0xF8 (Fixed disk)
    bs[21] = 0xF8;
    // Sectors Per FAT: 1
    bs[22] = 0x01; bs[23] = 0x00;
    // Sectors Per Track: 1
    bs[24] = 0x01; bs[25] = 0x00;
    // Heads: 1
    bs[26] = 0x01; bs[27] = 0x00;
    // Drive Number
    bs[36] = 0x80;
    // Boot Signature
    bs[38] = 0x29;
    // Volume ID
    bs[39] = 0x12; bs[40] = 0x34; bs[41] = 0x56; bs[42] = 0x78;
    // Volume Label
    memcpy(&bs[43], "KEY_DISK   ", 11);
    // File System Type
    memcpy(&bs[54], "FAT12   ", 8);
    // Boot Signature
    bs[510] = 0x55; bs[511] = 0xAA;

    // 2. FAT 1 (LBA 1)
    uint8_t* fat1 = ramDiskBuffer + RAM_DISK_SECTOR_SIZE;
    fat1[0] = 0xF8;
    fat1[1] = 0xFF;
    fat1[2] = 0xFF; // FAT12 marker

    // 3. FAT 2 (LBA 2) - Copy of FAT 1
    uint8_t* fat2 = ramDiskBuffer + (2 * RAM_DISK_SECTOR_SIZE);
    memcpy(fat2, fat1, RAM_DISK_SECTOR_SIZE);
}


static int32_t onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    if (!ramDiskBuffer) return -1;
    
    // Calculate address
    uint64_t addr = (uint64_t)lba * RAM_DISK_SECTOR_SIZE + offset;
    
    // Boundary check
    if (addr >= RAM_DISK_SECTOR_COUNT * RAM_DISK_SECTOR_SIZE) {
        // Entirely out of bounds
        memset(buffer, 0, bufsize);
        return bufsize; // Pretend success with zeros
    }
    
    uint32_t available = (RAM_DISK_SECTOR_COUNT * RAM_DISK_SECTOR_SIZE) - addr;
    if (bufsize > available) {
        // Partial read
        memcpy(buffer, ramDiskBuffer + addr, available);
        memset((uint8_t*)buffer + available, 0, bufsize - available);
        return bufsize;
    }

    memcpy(buffer, ramDiskBuffer + addr, bufsize);
    return bufsize;
}

static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    if (!ramDiskBuffer) return -1;

    uint64_t addr = (uint64_t)lba * RAM_DISK_SECTOR_SIZE + offset;
    
    if (addr >= RAM_DISK_SECTOR_COUNT * RAM_DISK_SECTOR_SIZE) {
        // Write out of bounds - just pretend we did it
        return bufsize; 
    }
    
    uint32_t available = (RAM_DISK_SECTOR_COUNT * RAM_DISK_SECTOR_SIZE) - addr;
    if (bufsize > available) {
        // Partial write
        memcpy(ramDiskBuffer + addr, buffer, available);
         return bufsize;
    }
    
    memcpy(ramDiskBuffer + addr, buffer, bufsize);
    return bufsize;
}

StorageManager::StorageManager() : isMounted(false) {}

bool StorageManager::begin() {
    pinMode(BOARD_SD_CS, OUTPUT);
    // SPI initialization is handled by SD.begin()
    return SD.begin(BOARD_SD_CS, SPI);
}

bool StorageManager::startUSBMode() {
    ejectRequested = false;
    // Always ensure RAM is allocated
    if (!ramDiskBuffer) {
        ramDiskBuffer = (uint8_t*)malloc(RAM_DISK_SECTOR_COUNT * RAM_DISK_SECTOR_SIZE);
        if (!ramDiskBuffer) return false;
        formatRAMDisk();
    }

    if (mscStarted) {
        MSC.mediaPresent(true);
        isMounted = true;
        return true;
    }

    MSC.vendorID("SshDisk");
    MSC.productID("Key Disk");
    MSC.productRevision("1.0");
    MSC.onRead(onRead);
    MSC.onWrite(onWrite);
    MSC.onStartStop(onStartStop);
    MSC.mediaPresent(true);
    
    // Start MSC
    if (!MSC.begin(RAM_DISK_SECTOR_COUNT, RAM_DISK_SECTOR_SIZE)) {
        return false;
    }
    
    USB.begin(); 
    mscStarted = true;
    isMounted = true;
    return true;
}

void StorageManager::stopUSBMode() {
    isMounted = false;
    MSC.mediaPresent(false);
    MSC.end();
    
    // Allow time for host to process detachment
    delay(200);

    // Free Memory safely
    if (ramDiskBuffer) {
        // Prevent race condition with USB ISR by nulling pointer first
        uint8_t* temp = ramDiskBuffer;
        ramDiskBuffer = nullptr; 
        
        // Small delay to let any pending USB callbacks finish
        delay(50); 
        
        free(temp);
    }
}

String StorageManager::scanRAMDiskForKey() {
    if (!ramDiskBuffer) return "";
    
    const char* beginTag = "-----BEGIN"; 
    const char* endTag = "-----END";
    
    size_t totalSize = RAM_DISK_SECTOR_COUNT * RAM_DISK_SECTOR_SIZE;
    
    for (size_t i = 0; i < totalSize - 50; i++) {
        if (ramDiskBuffer[i] == '-') { 
             if (memcmp(ramDiskBuffer + i, beginTag, strlen(beginTag)) == 0) {
                 // Found header
                 // Look for footer in reasonable proximity
                 for (size_t j = i + 50; j < totalSize - 10; j++) {
                     if (ramDiskBuffer[j] == '-') {
                         if (memcmp(ramDiskBuffer + j, endTag, strlen(endTag)) == 0) {
                             // Found footer. Find end of line.
                             // Footer e.g. "-----END RSA PRIVATE KEY-----" is ~29 chars.
                             // Previous limit of 20 cut it off! check up to 60 chars for safety.
                             size_t k = j + strlen(endTag);
                             while(k < totalSize && ramDiskBuffer[k] != '\n' && ramDiskBuffer[k] != '\r' && (k - j < 60)) k++;
                             
                             size_t len = k - i + 1;
                             char* keyBuf = (char*)malloc(len + 1);
                             if (keyBuf) {
                                memcpy(keyBuf, ramDiskBuffer + i, len);
                                keyBuf[len] = 0;
                                String result = String(keyBuf);
                                free(keyBuf);
                                return result;
                             }
                         }
                     }
                 }
             }
        }
    }
    return "";
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

bool StorageManager::isEjectRequested() {
    return ejectRequested;
}

void StorageManager::clearEjectRequest() {
    ejectRequested = false;
}

bool StorageManager::isUSBActive() {
    return isMounted;
}
