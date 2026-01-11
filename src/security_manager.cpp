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
    if (!keyValid || plainText.isEmpty()) return plainText; // Fallback or empty

    // 1. Prepare Buffer with Padding (PKCS#7)
    int len = plainText.length();
    int pad = 16 - (len % 16);
    int paddedLen = len + pad;
    
    unsigned char* inputBuf = new unsigned char[paddedLen];
    memcpy(inputBuf, plainText.c_str(), len);
    for (int i = 0; i < pad; i++) {
        inputBuf[len + i] = (unsigned char)pad;
    }

    // 2. Encrypt AES-256-CBC
    unsigned char* outputBuf = new unsigned char[paddedLen];
    unsigned char iv[16];
    memcpy(iv, FIXED_IV, 16); // Setup IV (needs to be reset every op)

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, aesKey, 256);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, paddedLen, iv, inputBuf, outputBuf);
    mbedtls_aes_free(&aes);

    // 3. Base64 Encode
    size_t dlen = 0;
    mbedtls_base64_encode(NULL, 0, &dlen, outputBuf, paddedLen); // Get size
    
    unsigned char* b64Buf = new unsigned char[dlen + 1];
    size_t olen = 0;
    mbedtls_base64_encode(b64Buf, dlen + 1, &olen, outputBuf, paddedLen);
    b64Buf[olen] = '\0';

    String result = String((char*)b64Buf);

    delete[] inputBuf;
    delete[] outputBuf;
    delete[] b64Buf;

    return result;
}

String SecurityManager::decrypt(const String& cipherText) {
    if (!keyValid || cipherText.isEmpty()) return cipherText;

    // 1. Base64 Decode
    size_t dlen = 0;
    unsigned char* inputData = (unsigned char*)cipherText.c_str();
    size_t inputLen = cipherText.length();

    mbedtls_base64_decode(NULL, 0, &dlen, inputData, inputLen); // Get size
    unsigned char* encBuf = new unsigned char[dlen];
    size_t olen = 0;
    
    if (mbedtls_base64_decode(encBuf, dlen, &olen, inputData, inputLen) != 0) {
        delete[] encBuf;
        return ""; // Decode failed
    }

    // 2. Decrypt AES-256-CBC
    unsigned char* decBuf = new unsigned char[olen];
    unsigned char iv[16];
    memcpy(iv, FIXED_IV, 16);

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, aesKey, 256);
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, olen, iv, encBuf, decBuf);
    mbedtls_aes_free(&aes);

    // 3. Remove Padding (PKCS#7)
    int pad = decBuf[olen - 1];
    if (pad > 0 && pad <= 16) {
        // Basic sanity check on padding
        decBuf[olen - pad] = '\0';
        String result = String((char*)decBuf);
        delete[] encBuf;
        delete[] decBuf;
        return result;
    }

    // Invalid padding? Return raw or empty.
    delete[] encBuf;
    delete[] decBuf;
    return "";
}
