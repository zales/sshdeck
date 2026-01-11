#include "display_manager.h"
#include "board_def.h"
#include <SPI.h>

DisplayManager::DisplayManager() 
    : display(GxEPD2_310_GDEQ031T10(BOARD_EPD_CS, BOARD_EPD_DC, BOARD_EPD_RST, BOARD_EPD_BUSY)) {
}

bool DisplayManager::begin() {
    // Initialize SPI for display
    SPI.begin(BOARD_EPD_SCK, -1, BOARD_EPD_MOSI, BOARD_EPD_CS);
    
    display.init(115200, true, 2, false);
    display.setRotation(DISPLAY_ROTATION);
    
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
