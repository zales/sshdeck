#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <mbedtls/aes.h>
#include <mbedtls/md.h>

class SecurityManager {
public:
    SecurityManager();
    
    void begin();

    // Derive session key from PIN and verify against stored challenge.
    // Returns true if PIN is correct (or if this is a fresh setup).
    bool authenticate(const String& pin);

    // Change PIN (and re-encrypt validation token)
    // Does NOT handle re-encrypting data, caller must do that!
    void changePin(const String& newPin);

    String encrypt(const String& plainText);
    String decrypt(const String& cipherText);

    void saveSSHKey(const String& key);
    String getSSHKey();

    bool isKeySet() const { return keyValid; }

private:
    bool keyValid;
    unsigned char aesKey[32]; // 256-bit key
    Preferences prefs;
    
    void setKeyFromPin(const String& pin);
};
