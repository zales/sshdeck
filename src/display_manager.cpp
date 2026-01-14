#include "display_manager.h"
#include "board_def.h"
#include <SPI.h>

DisplayManager::DisplayManager() 
    // We pass -1 for RST to force Software Reset. 
    // This avoids toggling Pin 4 (shared with LoRa) which might be causing 
    // a power glitch or conflict resulting in the display fading.
    : display(GxEPD2_310_GDEQ031T10(BOARD_EPD_CS, BOARD_EPD_DC, -1, BOARD_EPD_BUSY)) {
    _mutex = xSemaphoreCreateMutex();
}

bool DisplayManager::begin() {
    // 1. Initialize Control Pins for Peripherals sharing SPI to avoid conflicts
    // Matches logic from known working Arduino example
    pinMode(BOARD_LORA_CS, OUTPUT);  digitalWrite(BOARD_LORA_CS, HIGH);
    pinMode(BOARD_LORA_RST, OUTPUT); digitalWrite(BOARD_LORA_RST, HIGH); 
    pinMode(BOARD_SD_CS, OUTPUT);    digitalWrite(BOARD_SD_CS, HIGH);
    pinMode(BOARD_EPD_CS, OUTPUT);   digitalWrite(BOARD_EPD_CS, HIGH);

    // 2. Power ON
    // Just turn it ON, no fancy cycling which might disturb the controller
    pinMode(BOARD_POWERON, OUTPUT);
    digitalWrite(BOARD_POWERON, HIGH);
    delay(100); 

    // Initialize SPI
    // Note: Passing -1 for MISO as per official example, since EPD is write-only 
    // and we want to avoid input noise during display init.
    SPI.begin(BOARD_EPD_SCK, -1, BOARD_EPD_MOSI, BOARD_EPD_CS);
    
    // "True" for 2nd arg enables Serial debug output from GxEPD2 if needed
    // Revert to standard 2ms pulse as in valid example
    display.init(115200, true, 2, false); 
    
    display.setRotation(DISPLAY_ROTATION);
    
    // Initial Clear
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
    } while (display.nextPage());

    u8g2Fonts.begin(display);
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setFont(u8g2_font_6x10_tf);
    
    return true;
}

void DisplayManager::clear() {
    display.fillScreen(GxEPD_WHITE);
}

void DisplayManager::update() {
    display.nextPage();
}

void DisplayManager::drawText(int x, int y, const char* text) {
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    u8g2Fonts.setCursor(x, y);
    u8g2Fonts.print(text);
}

void DisplayManager::fillRect(int x, int y, int w, int h, uint16_t color) {
    display.fillRect(x, y, w, h, color);
}

void DisplayManager::setRefreshMode(bool partial) {
    if (partial) {
        display.setPartialWindow(0, 0, display.width(), display.height());
    } else {
        display.setFullWindow();
    }
}

void DisplayManager::setPartialWindow(int x, int y, int w, int h) {
    display.setPartialWindow(x, y, w, h);
}

void DisplayManager::firstPage() {
    display.firstPage();
}

bool DisplayManager::nextPage() {
    return display.nextPage();
}

int DisplayManager::getWidth() const {
    return display.width();
}

int DisplayManager::getHeight() const {
    return display.height();
}

GxEPD2_BW<GxEPD2_310_GDEQ031T10, GxEPD2_310_GDEQ031T10::HEIGHT>& DisplayManager::getDisplay() {
    return display;
}

U8G2_FOR_ADAFRUIT_GFX& DisplayManager::getFonts() {
    return u8g2Fonts;
}

void DisplayManager::lock() {
    if (_mutex) {
        xSemaphoreTake(_mutex, portMAX_DELAY);
    }
}

void DisplayManager::unlock() {
    if (_mutex) {
        xSemaphoreGive(_mutex);
    }
}
