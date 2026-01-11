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
    display.firstPage();
    do {
        display.clear();
        drawTitleBar(title);

        auto& u8g2 = display.getFonts();
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

    } while (display.nextPage());
}

void UIManager::drawMessage(const String& title, const String& message) {
    display.setRefreshMode(false);
    display.firstPage();
    do {
        display.clear();
        drawTitleBar(title);
        auto& u8g2 = display.getFonts();
        
        // Center the message roughly
        int msgY = height() / 2;
        drawCenteredText(msgY, message, u8g2_font_helvR12_tr);
        
        // Footer
        u8g2.setFont(u8g2_font_profont12_tr);
        int footerH = 16;
        display.fillRect(0, height() - footerH, width(), footerH, GxEPD_BLACK);
        u8g2.setForegroundColor(GxEPD_WHITE);
        u8g2.setBackgroundColor(GxEPD_BLACK);
        u8g2.setCursor(5, height() - 4);
        u8g2.print("Press Key");
        
    } while (display.nextPage());
}

void UIManager::drawSystemInfo(const String& ip, const String& bat, const String& ram, const String& mac) {
    display.setRefreshMode(false);
    display.firstPage();
    do {
        display.clear();
        drawTitleBar("System Info");
        auto& u8g2 = display.getFonts();
        
        u8g2.setForegroundColor(GxEPD_BLACK);
        u8g2.setBackgroundColor(GxEPD_WHITE);
        u8g2.setFont(u8g2_font_helvR10_tr);
        
        int y = 60;
        int x = 10;
        int dy = 25;
        
        u8g2.setCursor(x, y); u8g2.print("IP:  " + ip); y += dy;
        u8g2.setCursor(x, y); u8g2.print("BAT: " + bat); y += dy;
        u8g2.setCursor(x, y); u8g2.print("RAM: " + ram); y += dy;
        u8g2.setCursor(x, y); u8g2.print("MAC: " + mac); y += dy;
        
        int footerH = 16;
        display.fillRect(0, height() - footerH, width(), footerH, GxEPD_BLACK);
        u8g2.setForegroundColor(GxEPD_WHITE);
        u8g2.setBackgroundColor(GxEPD_BLACK);
        u8g2.setFont(u8g2_font_profont12_tr); 
        u8g2.setCursor(5, height() - 4);
        u8g2.print("Press Key to Close");
        
    } while (display.nextPage());
}

void UIManager::drawShutdownScreen() {
    display.setRefreshMode(false);
    display.firstPage();
    do {
        display.clear();
        int w = width();
        int h = height();
        
        auto& u8g2 = display.getFonts();
        display.fillRect(0, 0, w, h, GxEPD_WHITE);
        
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

    } while (display.nextPage());
}

void UIManager::drawBootScreen(const String& line1, const String& line2) {
    display.setRefreshMode(false);
    display.firstPage();
    do {
        display.clear();
        int w = width();
        int h = height();
        
        auto& u8g2 = display.getFonts();
        display.fillRect(0, 0, w, h, GxEPD_WHITE);
        
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
        
        drawCenteredText(bandY + bandH + 30, line2, u8g2_font_helvB10_tr);
    } while (display.nextPage());
}

void UIManager::drawStatusBar(bool wifiConnected, const String& wifiSSID, 
                              bool sshConnected, const String& sshHost, 
                              int batteryPercent, float batteryVolts) {
    // This is typically drawn as part of the terminal screen, but we can reuse the logic
    // Implementation left simple for now or integrated into 'Common'
}
void UIManager::updateBootStatus(const String& status) {
    display.setRefreshMode(true); // Partial refresh
    display.firstPage();
    do {
        display.clear();
        int w = width();
        int h = height();
        
        auto& u8g2 = display.getFonts();
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
    } while (display.nextPage());
}
