#include "security_manager.h"
#include <mbedtls/sha256.h>
#include <mbedtls/base64.h>
#include <mbedtls/pkcs5.h>
#include <cstring>

// Legacy hardcoded salt for backwards compatibility with existing devices
static const unsigned char LEGACY_PBKDF2_SALT[16] = { 
    0x5A, 0x45, 0x52, 0x4F, 0x53, 0x41, 0x4C, 0x54,
    0x44, 0x45, 0x43, 0x4B, 0x50, 0x52, 0x4F, 0x31
};

// PBKDF2 iterations (10,000 is minimum recommended, higher = slower but more secure)
static const int PBKDF2_ITERATIONS = 10000;

SecurityManager::SecurityManager() : keyValid(false) {
    memset(aesKey, 0, sizeof(aesKey));
    memset(pbkdf2Salt, 0, sizeof(pbkdf2Salt));
}

void SecurityManager::begin() {
    prefs.begin("tdeck-sec", false);
    
    // Load or generate per-device salt
    if (prefs.isKey("salt")) {
        prefs.getBytes("salt", pbkdf2Salt, sizeof(pbkdf2Salt));
    } else {
        if (!prefs.isKey("challenge")) {
            // Fresh install - generate unique random salt
            for (int i = 0; i < 16; i++) {
                pbkdf2Salt[i] = (unsigned char)(esp_random() & 0xFF);
            }
        } else {
            // Legacy device with existing data - use hardcoded salt for compatibility
            memcpy(pbkdf2Salt, LEGACY_PBKDF2_SALT, 16);
        }
        prefs.putBytes("salt", pbkdf2Salt, sizeof(pbkdf2Salt));
    }
}

void SecurityManager::setKeyFromPin(const String& pin) {
    // Derive key from PIN using PBKDF2-HMAC-SHA256 (industry standard)
    // This is MUCH more secure than MD5 or plain SHA256
    mbedtls_md_context_t md_ctx;
    mbedtls_md_init(&md_ctx);
    
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_setup(&md_ctx, md_info, 1); // 1 = HMAC
    
    mbedtls_pkcs5_pbkdf2_hmac(&md_ctx,
                              (const unsigned char*)pin.c_str(), pin.length(),
                              pbkdf2Salt, sizeof(pbkdf2Salt),
                              PBKDF2_ITERATIONS,
                              32, // Output 256-bit key
                              aesKey);
    
    mbedtls_md_free(&md_ctx);
    
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
    
    // Use unique_ptr for automatic cleanup - prevents memory leaks!
    int totalLen = 16 + paddedLen;
    std::unique_ptr<unsigned char[]> outputBuf(new unsigned char[totalLen]);
    std::unique_ptr<unsigned char[]> inputBuf(new unsigned char[paddedLen]);
    
    // Copy IV to start of output
    memcpy(outputBuf.get(), iv, 16);
    
    // Prepare input padded buffer
    memcpy(inputBuf.get(), plainText.c_str(), len);
    for (int i = 0; i < pad; i++) {
        inputBuf[len + i] = (unsigned char)pad;
    }

    // 3. Encrypt AES-256-CBC
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    int ret = mbedtls_aes_setkey_enc(&aes, aesKey, 256);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return ""; // Encryption setup failed
    }
    
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, paddedLen, iv, inputBuf.get(), outputBuf.get() + 16);
    mbedtls_aes_free(&aes);

    // 4. Base64 Encode Full Bundle
    size_t dlen = 0;
    mbedtls_base64_encode(NULL, 0, &dlen, outputBuf.get(), totalLen); 
    
    std::unique_ptr<unsigned char[]> b64Buf(new unsigned char[dlen + 1]);
    size_t olen = 0;
    ret = mbedtls_base64_encode(b64Buf.get(), dlen + 1, &olen, outputBuf.get(), totalLen);
    if (ret != 0) {
        return ""; // Base64 encoding failed
    }
    b64Buf[olen] = '\0';

    String result = String((char*)b64Buf.get());
    // unique_ptr automatically deletes all buffers here
    return result;
}

String SecurityManager::decrypt(const String& cipherText) {
    if (!keyValid || cipherText.isEmpty()) return "";

    // 1. Base64 Decode
    size_t len = cipherText.length();
    
    // Calculate required buffer size: (len * 3) / 4 + safety
    size_t bufSize = (len * 3 / 4) + 16; 
    std::unique_ptr<unsigned char[]> encBuf(new unsigned char[bufSize]);
    size_t olen = 0;
    
    if (mbedtls_base64_decode(encBuf.get(), bufSize, &olen, (const unsigned char*)cipherText.c_str(), len) != 0) {
        return ""; // Decode failed - unique_ptr auto-cleans
    }

    // Check minimum length (IV + at least 1 block)
    if (olen < 32) {
        return ""; // unique_ptr auto-cleans
    }

    // 2. Extract IV
    unsigned char iv[16];
    memcpy(iv, encBuf.get(), 16); // First 16 bytes are IV

    // 3. Decrypt AES-256-CBC (Skipping first 16 bytes of IV)
    int cipherLen = olen - 16;
    std::unique_ptr<unsigned char[]> decBuf(new unsigned char[cipherLen]);

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    int ret = mbedtls_aes_setkey_dec(&aes, aesKey, 256);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return ""; // Decryption setup failed
    }
    
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, cipherLen, iv, encBuf.get() + 16, decBuf.get());
    mbedtls_aes_free(&aes);

    // 4. Remove Padding (PKCS#7) with validation
    int pad = decBuf[cipherLen - 1];
    if (pad > 0 && pad <= 16 && cipherLen >= pad) {
        // Validate padding bytes (all should be equal to pad value)
        bool validPadding = true;
        for (int i = 0; i < pad; i++) {
            if (decBuf[cipherLen - 1 - i] != pad) {
                validPadding = false;
                break;
            }
        }
        
        if (validPadding) {
            decBuf[cipherLen - pad] = '\0';
            String result = String((char*)decBuf.get());
            return result;
        }
    }

    return ""; // Invalid padding or decryption failed
}

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
