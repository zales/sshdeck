#pragma once

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "config.h"

/**
 * @brief Display Manager for E-Paper display
 * Handles all display operations including initialization and rendering
 */
class DisplayManager {
public:
    DisplayManager();
    
    bool begin();
    void clear();
    void update();
    void drawText(int x, int y, const char* text);
    void fillRect(int x, int y, int w, int h, uint16_t color);

    // Refresh control
    void setRefreshMode(bool partial);
    void setPartialWindow(int x, int y, int w, int h);
    void firstPage();
    bool nextPage();
    
    int getWidth() const;
    int getHeight() const;
    
    GxEPD2_BW<GxEPD2_310_GDEQ031T10, GxEPD2_310_GDEQ031T10::HEIGHT>& getDisplay();
    U8G2_FOR_ADAFRUIT_GFX& getFonts();
    
private:
    GxEPD2_BW<GxEPD2_310_GDEQ031T10, GxEPD2_310_GDEQ031T10::HEIGHT> display;
    U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;
};
