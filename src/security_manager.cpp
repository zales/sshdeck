#include "security_manager.h"
#include <mbedtls/sha256.h>
#include <mbedtls/base64.h>
#include <cstring>

// Fixed IV for simplicity in this project (16 bytes)
// In high security ctx, this should be random and stored with ciphertext
static const unsigned char FIXED_IV[16] = { 
    0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF,
    0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF 
};

SecurityManager::SecurityManager() : keyValid(false) {
    memset(aesKey, 0, sizeof(aesKey));
}

void SecurityManager::begin() {
    prefs.begin("tdeck-sec", false);
}

void SecurityManager::setKeyFromPin(const String& pin) {
    // Hash PIN to get 32-byte key (SHA-256)
    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, 0); // 0 = SHA-256
    mbedtls_sha256_update(&sha_ctx, (const unsigned char*)pin.c_str(), pin.length());
    mbedtls_sha256_finish(&sha_ctx, aesKey);
    mbedtls_sha256_free(&sha_ctx);
    
    // We don't set keyValid=true here yet, only after auth check
}

bool SecurityManager::authenticate(const String& pin) {
    if (pin.length() == 0) return false;

    setKeyFromPin(pin);
    keyValid = true; // Temporarily assume valid to try decrypt/encrypt

    String challenge = prefs.getString("challenge", "");
    
    if (challenge.isEmpty()) {
        // First run! Set this PIN as the master PIN
        changePin(pin);
        return true;
    } else {
        // Validation run
        String decrypted = decrypt(challenge);
        if (decrypted == "VALID") {
            return true;
        } else {
            keyValid = false; // Revoke key
            return false;
        }
    }
}

void SecurityManager::changePin(const String& newPin) {
    setKeyFromPin(newPin);
    keyValid = true;
    
    // Save new validation token
    String validationToken = "VALID";
    String encrypted = encrypt(validationToken);
    prefs.putString("challenge", encrypted);
}

String SecurityManager::encrypt(const String& plainText) {
    if (!keyValid || plainText.isEmpty()) return plainText;

    // 1. Generate Random IV
    unsigned char iv[16];
    for (int i = 0; i < 16; i++) {
        iv[i] = (unsigned char)(esp_random() & 0xFF);
    }

    // 2. Prepare Buffer with Padding (PKCS#7)
    int len = plainText.length();
    int pad = 16 - (len % 16);
    int paddedLen = len + pad;
    
    // Allocate buffer for IV + Encrypted Content
    // Structure: [IV (16 bytes)] [Ciphertext (N bytes)]
    int totalLen = 16 + paddedLen;
    unsigned char* outputBuf = new unsigned char[totalLen];
    
    // Copy IV to start of output
    memcpy(outputBuf, iv, 16);
    
    // Prepare input padded buffer
    unsigned char* inputBuf = new unsigned char[paddedLen];
    memcpy(inputBuf, plainText.c_str(), len);
    for (int i = 0; i < pad; i++) {
        inputBuf[len + i] = (unsigned char)pad;
    }

    // 3. Encrypt AES-256-CBC
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, aesKey, 256);
    // Encrypt into output buffer offset by 16 bytes
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, paddedLen, iv, inputBuf, outputBuf + 16);
    mbedtls_aes_free(&aes);

    delete[] inputBuf;

    // 4. Base64 Encode Full Bundle
    size_t dlen = 0;
    mbedtls_base64_encode(NULL, 0, &dlen, outputBuf, totalLen); 
    
    unsigned char* b64Buf = new unsigned char[dlen + 1];
    size_t olen = 0;
    mbedtls_base64_encode(b64Buf, dlen + 1, &olen, outputBuf, totalLen);
    b64Buf[olen] = '\0';

    String result = String((char*)b64Buf);

    delete[] outputBuf;
    delete[] b64Buf;

    return result;
}

String SecurityManager::decrypt(const String& cipherText) {
    if (!keyValid || cipherText.isEmpty()) return "";

    // 1. Base64 Decode
    size_t len = cipherText.length();
    
    // Calculate required buffer size: (len * 3) / 4 + safety
    size_t bufSize = (len * 3 / 4) + 16; 
    unsigned char* encBuf = new unsigned char[bufSize];
    size_t olen = 0;
    
    if (mbedtls_base64_decode(encBuf, bufSize, &olen, (const unsigned char*)cipherText.c_str(), len) != 0) {
        delete[] encBuf;
        return ""; // Decode failed
    }

    // Check minimum length (IV + at least 1 block)
    if (olen < 32) {
        delete[] encBuf;
        return "";
    }

    // 2. Extract IV
    unsigned char iv[16];
    memcpy(iv, encBuf, 16); // First 16 bytes are IV

    // 3. Decrypt AES-256-CBC (Skipping first 16 bytes of IV)
    int cipherLen = olen - 16;
    unsigned char* decBuf = new unsigned char[cipherLen];

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, aesKey, 256);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, cipherLen, iv, encBuf + 16, decBuf);
    mbedtls_aes_free(&aes);

    // 4. Remove Padding (PKCS#7)
    int pad = decBuf[cipherLen - 1];
    if (pad > 0 && pad <= 16) {
        decBuf[cipherLen - pad] = '\0';
        String result = String((char*)decBuf);
        delete[] encBuf;
        delete[] decBuf;
        return result;
    }

    delete[] encBuf;
    delete[] decBuf;
    return "";
}
#include "security_manager.h"

void SecurityManager::saveSSHKey(const String& key) {
    if (key.length() == 0) {
        prefs.remove("ssh_priv_key");
        return;
    }
    String enc = encrypt(key);
    prefs.putString("ssh_priv_key", enc);
}

String SecurityManager::getSSHKey() {
    String enc = prefs.getString("ssh_priv_key", "");
    if (enc.length() == 0) return "";
    return decrypt(enc);
}
