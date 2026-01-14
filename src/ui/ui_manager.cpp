#include "ui/ui_manager.h"

UIManager::UIManager(DisplayManager& disp) : display(disp) {}

int UIManager::width() const { return display.getWidth(); }
int UIManager::height() const { return display.getHeight(); }

U8G2_FOR_ADAFRUIT_GFX& UIManager::getFonts() { return display.getFonts(); }


void UIManager::drawCenteredText(int y, const String& text, const uint8_t* font) {
    auto& u8g2 = display.getFonts();
    u8g2.setFont(font);
    int w = u8g2.getUTF8Width(text.c_str());
    u8g2.setCursor((width() - w) / 2, y);
    u8g2.print(text);
}

void UIManager::drawTitleBar(const String& title) {
    auto& u8g2 = display.getFonts();
    display.fillRect(0, 0, width(), 30, GxEPD_BLACK);
    u8g2.setForegroundColor(GxEPD_WHITE);
    u8g2.setBackgroundColor(GxEPD_BLACK);
    drawCenteredText(22, title, u8g2_font_helvB14_tr);
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);
}

void UIManager::drawPinEntry(const String& title, const String& subtitle, const String& entry, bool isWrong) {
    display.setRefreshMode(true); // Ensure partial update mode
    render([&](U8G2_FOR_ADAFRUIT_GFX& u8g2) {
        drawHeader(title);

        // Subtitle
        u8g2.setForegroundColor(GxEPD_BLACK);
        u8g2.setBackgroundColor(GxEPD_WHITE);
        drawCenteredText(60, subtitle, u8g2_font_helvR12_tr);

        // Box for PIN
        int boxW = width() - 40;
        int boxH = 40;
        int boxX = 20;
        int boxY = 80;

        display.fillRect(boxX, boxY, boxW, boxH, GxEPD_BLACK);
        display.fillRect(boxX + 2, boxY + 2, boxW - 4, boxH - 4, GxEPD_WHITE);

        // Masked PIN "*"
        u8g2.setFont(u8g2_font_courB18_tr);
        String mask = "";
        for(int i = 0; i < entry.length(); i++) mask += "*";
        
        int maskW = u8g2.getUTF8Width(mask.c_str());
        u8g2.setCursor((width() - maskW) / 2, boxY + 28);
        u8g2.print(mask);
        
        // Footer hint
        if (isWrong) {
             drawCenteredText(150, "INCORRECT PIN", u8g2_font_helvB10_tr);
        }
    });
}

void UIManager::drawMessage(const String& title, const String& message) {
    display.setRefreshMode(false);
    render([&](U8G2_FOR_ADAFRUIT_GFX& u8g2) {
        drawHeader(title);
        
        // Center the message roughly
        int msgY = height() / 2;
        drawCenteredText(msgY, message, u8g2_font_helvR12_tr);
        
        drawFooter("Press Key");
    });
}

void UIManager::drawSystemInfo(const String& ip, const String& bat, const String& ram, const String& mac) {
    setRefreshMode(true); // Partial refresh for consistency
    render([&](U8G2_FOR_ADAFRUIT_GFX& u8g2) {
        UILayout layout(*this, "System Info");
        
        layout.addItem("IP:", ip);
        layout.addItem("BAT:", bat);
        layout.addItem("RAM:", ram);
        layout.addItem("MAC:", mac);
        
        layout.addFooter("Press Key to Close");
    });
}

void UIManager::drawShutdownScreen() {
    display.setRefreshMode(false);
    render([&](U8G2_FOR_ADAFRUIT_GFX& u8g2) {
        int w = width();
        int h = height();
        
        // Stylish black band
        int bandH = 80;
        int bandY = (h - bandH) / 2;
        display.fillRect(0, bandY, w, bandH, GxEPD_BLACK);
        
        u8g2.setForegroundColor(GxEPD_WHITE);
        u8g2.setBackgroundColor(GxEPD_BLACK);
        
        // Big Text inside band
        u8g2.setFont(u8g2_font_logisoso42_tr);
        int tw = u8g2.getUTF8Width("SshDeck");
        u8g2.setCursor((w - tw)/2, bandY + 55);
        u8g2.print("SshDeck");
        
        u8g2.setForegroundColor(GxEPD_BLACK);
        u8g2.setBackgroundColor(GxEPD_WHITE);
        
        drawCenteredText(bandY + bandH + 30, "System Halted", u8g2_font_helvB10_tr);
    });
}

void UIManager::drawBootScreen(const String& line1, const String& line2) {
    display.setRefreshMode(false);
    render([&](U8G2_FOR_ADAFRUIT_GFX& u8g2) {
        int w = width();
        int h = height();
        
        // Stylish black band
        int bandH = 80;
        int bandY = (h - bandH) / 2;
        display.fillRect(0, bandY, w, bandH, GxEPD_BLACK);
        
        u8g2.setForegroundColor(GxEPD_WHITE);
        u8g2.setBackgroundColor(GxEPD_BLACK);
        
        // Big Text inside band
        u8g2.setFont(u8g2_font_logisoso42_tr);
        int tw = u8g2.getUTF8Width(line1.c_str());
        u8g2.setCursor((w - tw)/2, bandY + 55);
        u8g2.print(line1);
        
        u8g2.setForegroundColor(GxEPD_BLACK);
        u8g2.setBackgroundColor(GxEPD_WHITE);
        
        drawCenteredText(bandY + bandH + 30, line2, u8g2_font_helvB10_tr);
    });
}

void UIManager::updateStatusState(int bat, bool charging, bool wifi) {
    currentBat = bat;
    currentCharging = charging;
    currentWifi = wifi;
}

void UIManager::drawStatusBar(const String& title, bool wifiConnected, int batteryPercent, bool isCharging) {
    updateStatusState(batteryPercent, isCharging, wifiConnected);
    drawHeader(title);
}

void UIManager::updateBootStatus(const String& status) {
    setRefreshMode(true); // Partial refresh
    render([&](U8G2_FOR_ADAFRUIT_GFX& u8g2) {
        int w = width();
        int h = height();
        
        display.fillRect(0, 0, w, h, GxEPD_WHITE);
        
        // Stylish black band (same as boot screen but using partial refresh context)
        int bandH = 80;
        int bandY = (h - bandH) / 2;
        display.fillRect(0, bandY, w, bandH, GxEPD_BLACK);
        
        u8g2.setForegroundColor(GxEPD_WHITE);
        u8g2.setBackgroundColor(GxEPD_BLACK);
        
        u8g2.setFont(u8g2_font_logisoso42_tr);
        int tw = u8g2.getUTF8Width("SshDeck");
        u8g2.setCursor((w - tw)/2, bandY + 55);
        u8g2.print("SshDeck");
        
        u8g2.setForegroundColor(GxEPD_BLACK);
        u8g2.setBackgroundColor(GxEPD_WHITE);
        
        drawCenteredText(bandY + bandH + 30, status, u8g2_font_helvB10_tr);
    });
}

void UIManager::render(std::function<void(U8G2_FOR_ADAFRUIT_GFX&)> drawCallback) {
    display.firstPage();
    do {
        display.clear(); // Always start with a clean slate
        
        // Reset defaults before callback
        auto& u8g2 = display.getFonts();
        u8g2.setFontMode(1);
        u8g2.setForegroundColor(GxEPD_BLACK);
        u8g2.setBackgroundColor(GxEPD_WHITE);
        
        // Execute User Drawing Logic
        if (drawCallback) {
            drawCallback(u8g2);
        }
    } while (display.nextPage());
}

// --- Primitives ---

void UIManager::drawHeader(const String& title) {
    auto& u8g2 = display.getFonts();
    int w = width();
    int h = 16;
    
    display.fillRect(0, 0, w, h, GxEPD_BLACK);
    
    u8g2.setFont(u8g2_font_profont12_tf);
    u8g2.setForegroundColor(GxEPD_WHITE);
    u8g2.setBackgroundColor(GxEPD_BLACK);
    
    // Left: Title
    u8g2.setCursor(2, h - 4);
    u8g2.print(title);
    
    // Right: Status
    String status = "";
    if (currentWifi) status += "W ";
    status += String(currentBat) + "%";
    if (currentCharging) status += "+";
    
    int sw = u8g2.getUTF8Width(status.c_str());
    u8g2.setCursor(w - sw - 2, h - 4);
    u8g2.print(status);
    
    // Reset
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);
}

void UIManager::drawFooter(const String& message) {
    auto& u8g2 = display.getFonts();
    int h = 14;
    // int y = height() - h;
    
    u8g2.setFont(u8g2_font_profont12_tf);
    u8g2.setCursor(2, height() - 3);
    u8g2.print(message);
}

void UIManager::drawTextLine(int x, int y, const String& text, const uint8_t* font, bool invert) {
    auto& u8g2 = display.getFonts();
    if (font) u8g2.setFont(font);
    else u8g2.setFont(u8g2_font_profont12_tf);
    
    if (invert) {
        u8g2.setForegroundColor(GxEPD_WHITE);
        u8g2.setBackgroundColor(GxEPD_BLACK);
    }
    
    u8g2.setCursor(x, y);
    u8g2.print(text);
    
    if (invert) {
        u8g2.setForegroundColor(GxEPD_BLACK);
        u8g2.setBackgroundColor(GxEPD_WHITE);
    }
}

void UIManager::drawLabelValue(int y, const String& label, const String& value) {
    auto& u8g2 = display.getFonts();
    u8g2.setFont(u8g2_font_profont12_tf);
    
    // Label Left
    u8g2.setCursor(0, y);
    u8g2.print(label);
    
    // Value Right
    int valX = 60; 
    u8g2.setCursor(valX, y);
    u8g2.print(value);
}

// --- Layout Helper ---

UILayout::UILayout(UIManager& mgr, const String& title) : ui(mgr), currentY(26) {
    ui.drawHeader(title);
}

void UILayout::addText(const String& text) {
    ui.drawTextLine(0, currentY, text);
    currentY += 10;
}

void UILayout::addItem(const String& label, const String& value) {
    ui.drawLabelValue(currentY, label, value);
    currentY += 10;
}

void UILayout::addFooter(const String& text) {
    ui.drawFooter(text);
}

void UILayout::space(int pixels) {
    currentY += pixels;
}

void UIManager::setRefreshMode(bool partial) {
    display.setRefreshMode(partial);
}

void UIManager::fillRect(int x, int y, int w, int h, uint16_t color) {
    display.fillRect(x, y, w, h, color);
}

void UIManager::drawRect(int x, int y, int w, int h, uint16_t color) {
    display.getDisplay().drawRect(x, y, w, h, color);
}

void UIManager::drawFastHLine(int x, int y, int w, uint16_t color) {
    display.getDisplay().drawFastHLine(x, y, w, color);
}
