#include "ota_manager.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

OtaManager::OtaManager(DisplayManager& disp) : display(disp) {
}

UpdateManifest OtaManager::fetchManifest(const String& manifestUrl, const String& rootCaCert) {
    UpdateManifest result;
    result.latestVersion = "";
    
    if (WiFi.status() != WL_CONNECTED) {
        return result;
    }
    
    HTTPClient http;
    WiFiClientSecure* client = new WiFiClientSecure;
    
    if(rootCaCert.length() > 0) {
        client->setCACert(rootCaCert.c_str());
    } else {
        client->setInsecure();
    }
    client->setTimeout(10000); // 10s
    
    Serial.println("Fetching manifest: " + manifestUrl);
    
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    // Attempt 1: Secure
    if (http.begin(*client, manifestUrl)) {
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            Serial.println("Manifest payload: " + payload);
            
            // Parse JSON
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            
            if (!error) {
                result.latestVersion = doc["latest"].as<String>();
                JsonArray versions = doc["versions"];
                for (JsonObject v : versions) {
                    FirmwareVersion fv;
                    fv.version = v["version"].as<String>();
                    fv.date = v["date"].as<String>();
                    fv.url = v["url"].as<String>();
                    result.versions.push_back(fv);
                }
            } else {
                Serial.print("deserializeJson() failed: ");
                Serial.println(error.c_str());
            }

        } else {
            Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    } else {
        Serial.println("Unable to connect");
    }
    
    delete client;
    return result;
}

void OtaManager::begin() {
    ArduinoOTA.setHostname("t-deck-ota");

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
        else // U_SPIFFS
            type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type);
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

    ArduinoOTA.begin();
    Serial.println("OTA Ready");
}

void OtaManager::loop() {
    ArduinoOTA.handle();
}

bool OtaManager::updateFromUrl(const String& url, const String& rootCaCert) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("OTA: Wifi not connected");
        return false;
    }

    Serial.println("OTA: Starting update from " + url);
    drawProgress(0, "Connecting...");

    HTTPClient http;
    WiFiClientSecure *client = new WiFiClientSecure;
    if (!client) {
        Serial.println("OTA: Failed to allocate WiFiClientSecure");
        drawProgress(0, "Memory Error");
        delay(2000);
        return false;
    }

    if (client) {
        if (rootCaCert.length() > 0) {
            client->setCACert(rootCaCert.c_str());
        } else {
            client->setInsecure();
        }
        client->setTimeout(12000); // Increase timeout
    }

    // Attempt 1
    Serial.printf("OTA: Fetching %s\n", url.c_str());
    
    // Handle redirects?
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    if (client) {
        http.begin(*client, url);
    } else {
        http.begin(url);
    }
    
    const char * headerKeys[] = {"Content-Type"};
    http.collectHeaders(headerKeys, 1);

    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("OTA: HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
        drawProgress(0, "HTTP Error " + String(httpCode));
        delay(2000);
        http.end();
        if (client) delete client;
        return false;
    }

    int len = http.getSize();
    // String type = http.header("Content-Type"); 
    // Usually application/octet-stream

    if (len <= 0) {
        Serial.println("OTA: Content-Length not defined or zero");
        drawProgress(0, "Bad Cont-Len");
        delay(2000);
        http.end();
        if (client) delete client;
        return false;
    }

    bool canBegin = Update.begin(len);
    if (!canBegin) {
        Serial.println("OTA: Not enough space to begin OTA");
        drawProgress(0, "No Space");
        delay(2000);
        http.end();
        if (client) delete client;
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t written = 0;
    size_t total = len;
    uint8_t buff[128] = { 0 };
    
    int lastPercent = -1;

    while (http.connected() && (written < total)) {
        size_t size_avail = stream->available();
        if (size_avail) {
            int c = stream->readBytes(buff, ((size_avail > sizeof(buff)) ? sizeof(buff) : size_avail));
            size_t w = Update.write(buff, c);
            written += w;
            
             if (w != c) {
                 Serial.println("OTA: Write error!");
                 break;
             }

            // Update progress
            int percent = (written * 100) / total;
            if (percent != lastPercent) {
                lastPercent = percent;
                drawProgress(percent, "Downloading...");
                Serial.printf("OTA: %d%%\r", percent);
            }
        }
        delay(1);
    }

    if (written == total) {
        Serial.println("OTA: Written " + String(written) + " successfully");
        if (Update.end()) {
             Serial.println("OTA: Update done!");
             if (Update.isFinished()) {
                 drawProgress(100, "Success! Rebooting");
                 delay(2000);
                 Serial.println("OTA: Rebooting...");
                 esp_restart();
                 return true;
             } else {
                 Serial.println("OTA: Update not finished? Something went wrong");
             }
        } else {
             Serial.printf("OTA: Error #: %d\n", Update.getError());
        }
    } else {
        Serial.println("OTA: Written only : " + String(written) + "/" + String(total) + ". Retry?");
    }
    
    drawProgress(0, "Failed");
    delay(2000);
    http.end();
    if (client) delete client;
    return false;
}

String OtaManager::checkUpdateAvailable(const String& binUrl, const String& currentVersion, const String& rootCaCert) {
    if (WiFi.status() != WL_CONNECTED) return "";

    String verUrl = binUrl;
    // Replace .bin with .txt, or append .txt if no extension?
    // Simple approach: look for last dot
    int dot = verUrl.lastIndexOf('.');
    if (dot > 0) {
        verUrl = verUrl.substring(0, dot) + ".txt";
    } else {
        verUrl += ".txt";
    }
    
    // Hardcoded override for specific naming convention if needed
    // e.g. https://sshdeck.zales.dev/version.txt

    Serial.println("OTA: Checking version at " + verUrl);
    
    HTTPClient http;
    WiFiClientSecure *client = nullptr;

    if (verUrl.startsWith("https://")) {
        client = new WiFiClientSecure;
        if (!client) {
            Serial.println("OTA: Failed to allocate WiFiClientSecure");
            return "";
        }
        
        if (rootCaCert.length() > 0) {
            client->setCACert(rootCaCert.c_str());
        } else {
            client->setInsecure();
        }
        http.begin(*client, verUrl);
    } else {
        http.begin(verUrl);
    }
    
    int httpCode = http.GET();
    String newVer = "";
    
    // Retry Insecure on SSL failure
    if (httpCode < 0 && rootCaCert.length() > 0) {
        Serial.println("Check SSL Fail. Retrying Insecure...");
        http.end();
        if (client) {
            delete client;
            client = nullptr;
        }
        
        client = new WiFiClientSecure;
        if (!client) {
            Serial.println("OTA: Failed to allocate WiFiClientSecure for retry");
            return "";
        }
        client->setInsecure();
        
        // Re-init connection
        if (verUrl.startsWith("https://")) {
             http.begin(*client, verUrl);
        } else {
             http.begin(verUrl);
        }
        httpCode = http.GET();
    }

    if (httpCode == HTTP_CODE_OK) {
        newVer = http.getString();
        newVer.trim(); // Remove whitespace/newlines
        Serial.println("OTA: Server version: " + newVer);
        
        // Simple string comparison or semver? 
        // For now, if strings differ, it's an update (or downgrade)
        if (newVer == currentVersion) {
            newVer = ""; // Same version
        }
    } else {
        Serial.printf("OTA: Version check failed %d\n", httpCode);
        if (client) {
             char e[64];
             // Some versions of WiFiClientSecure don't have lastError(), but usually they do.
             // If not available, this might cause a compile error, but let's try.
             // Serial.printf("SSL Error: %d\n", client->lastError(e, 64)); 
        }
    }
    
    http.end();
    if (client) delete client;
    return newVer;
}

void OtaManager::drawProgress(int percent, String info) {
    display.setRefreshMode(true); // Partial refresh for speed
    display.getDisplay().firstPage();
    do {
        display.getDisplay().fillScreen(GxEPD_WHITE);
        auto& u8g2 = display.getFonts();
        
        int w = display.getWidth();
        int h = display.getHeight();
        
        // Title
        u8g2.setFont(u8g2_font_helvB10_tr);
        u8g2.setForegroundColor(GxEPD_BLACK);
        u8g2.setBackgroundColor(GxEPD_WHITE);
        int tw = u8g2.getUTF8Width("System Update");
        u8g2.setCursor((w - tw) / 2, 30);
        u8g2.print("System Update");
        
        // Info
        u8g2.setFont(u8g2_font_6x10_tr);
        tw = u8g2.getUTF8Width(info.c_str());
        u8g2.setCursor((w - tw) / 2, 60);
        u8g2.print(info);
        
        // Bar
        int bw = w - 40;
        int bx = 20;
        int by = 80;
        int bh = 15;
        
        display.getDisplay().drawRect(bx, by, bw, bh, GxEPD_BLACK);
        
        int fillW = (percent * (bw - 4)) / 100;
        if (fillW > 0) {
            display.getDisplay().fillRect(bx + 2, by + 2, fillW, bh - 4, GxEPD_BLACK);
        }
        
    } while (display.getDisplay().nextPage());
}
