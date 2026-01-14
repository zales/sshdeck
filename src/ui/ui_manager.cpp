#include "ui/ui_manager.h"
#include "terminal_emulator.h"
#include "config.h"

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

void UIManager::drawAutoConnectScreen(const String& ssid, int remainingSeconds, int batteryPercent, bool isCharging) {
    display.setRefreshMode(true); // Partial refresh for countdown
    render([&](U8G2_FOR_ADAFRUIT_GFX& u8g2) {
        display.getDisplay().fillScreen(GxEPD_WHITE);
        
        drawStatusBar("Wifi Setup", false, batteryPercent, isCharging);
        
        u8g2.setForegroundColor(GxEPD_BLACK);
        u8g2.setBackgroundColor(GxEPD_WHITE);
        u8g2.setFont(u8g2_font_helvB10_tr);
        
        u8g2.setCursor(10, 50);
        u8g2.print("Auto-Connecting...");

        u8g2.setFont(u8g2_font_helvR12_tr);
        u8g2.setCursor(10, 80);
        u8g2.print(ssid);
        
        u8g2.setFont(u8g2_font_helvR10_tr);
        u8g2.setCursor(10, 120);
        u8g2.print("Start in: " + String(remainingSeconds) + "s");
        
        u8g2.setCursor(10, 150);
        u8g2.print("Press 'q' or 'Mic+Q' to cancel");
    });
}

void UIManager::drawScanningScreen(int batteryPercent, bool isCharging) {
    display.setRefreshMode(false); // Full refresh for scanning
    render([&](U8G2_FOR_ADAFRUIT_GFX& u8g2) {
         display.getDisplay().fillScreen(GxEPD_WHITE);
         
         drawStatusBar("Network Scan", false, batteryPercent, isCharging);
         
         u8g2.setFont(u8g2_font_helvB12_tr);
         u8g2.setForegroundColor(GxEPD_BLACK);
         u8g2.setBackgroundColor(GxEPD_WHITE);
         
         // Center text
         String title = "Scanning...";
         int tw = u8g2.getUTF8Width(title.c_str());
         u8g2.setCursor((width() - tw) / 2, 120);
         u8g2.print(title);
    });
}

void UIManager::drawConnectingScreen(const String& ssid, const String& password, int batteryPercent, bool isCharging) {
    display.setRefreshMode(false);
    render([&](U8G2_FOR_ADAFRUIT_GFX& u8g2) {
         display.getDisplay().fillScreen(GxEPD_WHITE);
         
         drawStatusBar("Connecting...", false, batteryPercent, isCharging);

         u8g2.setFont(u8g2_font_helvB12_tr);
         u8g2.setForegroundColor(GxEPD_BLACK);
         u8g2.setBackgroundColor(GxEPD_WHITE);
         
         u8g2.setCursor(10, 80);
         String msg = "Connecting to:\n" + ssid;
         u8g2.print(msg);
         
         u8g2.setFont(u8g2_font_helvR10_tr);
         u8g2.setCursor(10, 120);
         u8g2.print("Password: " + String(password.length() > 0 ? "***" : "Open"));
    });
}

void UIManager::drawMenu(const String& title, const std::vector<String>& items, int selectedIndex) {
    setRefreshMode(true); // Partial usually for menu nav
    render([&](U8G2_FOR_ADAFRUIT_GFX& u8g2) {
        int w = display.getWidth();
        int h = display.getHeight();
        
        drawHeader(title);
        
        u8g2.setForegroundColor(GxEPD_BLACK);
        u8g2.setBackgroundColor(GxEPD_WHITE);
        u8g2.setFont(u8g2_font_helvR12_tr);
        
        int startY = 32; 
        int lineH = 22;
        int maxItems = (h - startY - 20) / lineH; 
        
        int offset = 0;
        if (items.size() > maxItems) {
            if (selectedIndex >= maxItems) {
                offset = selectedIndex - maxItems + 1;
            }
        }
        
        for (int i = 0; i < maxItems; i++) {
            int idx = i + offset;
            if (idx >= items.size()) break;
            
            int y = startY + (i) * lineH;
            
            // Highlight selection
            if (idx == selectedIndex) {
                 display.fillRect(0, y, w, lineH, GxEPD_BLACK);
                 u8g2.setForegroundColor(GxEPD_WHITE);
                 u8g2.setBackgroundColor(GxEPD_BLACK);
                 
                 // Little adjust for text vertical align in filled box
                 u8g2.setCursor(5, y + 16); 
            } else {
                 u8g2.setForegroundColor(GxEPD_BLACK);
                 u8g2.setBackgroundColor(GxEPD_WHITE);
                 u8g2.setCursor(5, y + 16);
            }
            
            u8g2.print(items[idx]);
        }
        
        // Scroll Indicators
        u8g2.setForegroundColor(GxEPD_BLACK);
        u8g2.setBackgroundColor(GxEPD_WHITE);
        u8g2.setFont(u8g2_font_profont12_tf);
        if (offset > 0) {
             u8g2.setCursor(w - 10, startY);
             u8g2.print("^");
        }
        if (offset + maxItems < items.size()) {
             u8g2.setCursor(w - 10, h - 5);
             u8g2.print("v");
        }
    });
}

void UIManager::drawInputScreen(const String& title, const String& currentText, bool isPassword, bool textOnly) {
    if (textOnly) {
        int w = display.getWidth();
        int boxW = w - 20;
        // Set partial window to just the input box area (plus small margin)
        display.setPartialWindow(10, 50, boxW, 32); 
    } else {
        setRefreshMode(true);
    }

    render([&](U8G2_FOR_ADAFRUIT_GFX& u8g2) {
        int w = display.getWidth();
        int h = display.getHeight();
        
        if (!textOnly) {
            // Title
            display.fillRect(0, 0, w, 24, GxEPD_BLACK);
            u8g2.setForegroundColor(GxEPD_WHITE);
            u8g2.setBackgroundColor(GxEPD_BLACK);
            u8g2.setFont(u8g2_font_helvB12_tr);
            u8g2.setCursor(5, 18);
            u8g2.print(title);
    
            // Footer
            int footerH = 16;
            display.fillRect(0, h - footerH, w, footerH, GxEPD_BLACK);
            u8g2.setForegroundColor(GxEPD_WHITE);
            u8g2.setBackgroundColor(GxEPD_BLACK);
            u8g2.setFont(u8g2_font_profont12_tf); 
            u8g2.setCursor(5, h - 4);
            u8g2.print("Type: Keys | Enter: OK | Esc: Cancel");
        }
        
        // Input Box - Always drawn
        u8g2.setForegroundColor(GxEPD_BLACK);
        u8g2.setBackgroundColor(GxEPD_WHITE);
        int boxW = w - 20;
        
        // Clear box area
        display.fillRect(10, 50, boxW, 30, GxEPD_WHITE);
        
        // Draw border
        display.fillRect(10, 50, boxW, 2, GxEPD_BLACK);
        display.fillRect(10, 80, boxW, 2, GxEPD_BLACK);
        display.fillRect(10, 50, 2, 32, GxEPD_BLACK);
        display.fillRect(10 + boxW - 2, 50, 2, 32, GxEPD_BLACK);
    
        u8g2.setFont(u8g2_font_helvR12_tr);
        u8g2.setCursor(15, 72);
        
        String displayStr = currentText;
        if (isPassword) {
             displayStr = "";
             for(int i=0; i<currentText.length(); i++) displayStr += "*";
        }
        
        u8g2.print(displayStr);
        u8g2.print("_"); 
    });
}

void UIManager::drawTerminal(const TerminalEmulator& term, const String& statusTitle, int batteryPercent, bool isCharging, bool wifiConnected) {
    render([&](U8G2_FOR_ADAFRUIT_GFX& u8g2) {
        drawStatusBar(statusTitle, wifiConnected, batteryPercent, isCharging);

        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.setFontMode(1);
        
        // Terminal Content
        int y_start = 24;
        int line_h = 10;
        int char_w = 6;
        
        for (int row = 0; row < TERM_ROWS; row++) {
            const char* line = term.getLine(row);
            if (line[0] != '\0') {
                for (int col = 0; line[col] != '\0' && col < TERM_COLS; col++) {
                    char ch = line[col];
                    bool inv = term.getAttr(row, col).inverse;
                    
                    int x = col * char_w;
                    int y = y_start + (row * line_h);
                    
                    if (inv) {
                        display.fillRect(x, y - 8, char_w, line_h, GxEPD_BLACK);
                        u8g2.setFontMode(1); 
                        u8g2.setForegroundColor(GxEPD_WHITE);
                        u8g2.setBackgroundColor(GxEPD_BLACK);
                    } else {
                        u8g2.setFontMode(1); 
                        u8g2.setForegroundColor(GxEPD_BLACK);
                        u8g2.setBackgroundColor(GxEPD_WHITE);
                    }
                    u8g2.setCursor(x, y);
                    u8g2.print(ch);
                }
            }
        }
        if (term.isCursorVisible()) {
            int cx = term.getCursorX();
            int cy = term.getCursorY();
            if (cx >= 0 && cx < TERM_COLS && cy >= 0 && cy < TERM_ROWS) {
                int c_x_px = cx * char_w;
                int c_y_px = y_start + (cy * line_h);
                char cursorChar = ' ';
                const char* line = term.getLine(cy);
                // Simple bounds check for line length to pick character under cursor
                // But getLine returns a fixed width buffer or null terminated?
                // Assuming it's valid.
                int len = 0;
                while(line[len] != 0 && len < TERM_COLS) len++;
                
                if (cx < len) cursorChar = line[cx];
                
                display.fillRect(c_x_px, c_y_px - 8, char_w, line_h, GxEPD_BLACK);
                u8g2.setFontMode(1);
                u8g2.setForegroundColor(GxEPD_WHITE);
                u8g2.setBackgroundColor(GxEPD_BLACK);
                u8g2.setCursor(c_x_px, c_y_px);
                u8g2.print(cursorChar);
            }
        }    
    });
}

void UIManager::drawHelpScreen() {
    setRefreshMode(false);
    render([&](U8G2_FOR_ADAFRUIT_GFX& u8g2) {
         int w = display.getWidth();
         int h = display.getHeight();
         
         // Header
         display.fillRect(0, 0, w, 24, GxEPD_BLACK);
         u8g2.setForegroundColor(GxEPD_WHITE);
         u8g2.setBackgroundColor(GxEPD_BLACK);
         u8g2.setFont(u8g2_font_helvB12_tr);
         u8g2.setCursor(5, 18);
         u8g2.print("Help: Shortcuts");
         
         // Content
         u8g2.setForegroundColor(GxEPD_BLACK);
         u8g2.setBackgroundColor(GxEPD_WHITE);
         u8g2.setFont(u8g2_font_helvR10_tr);
         
         int y = 40; int dy = 16;
         u8g2.setCursor(5, y); u8g2.print("Mic + W/A/S/D : Arrows"); y+=dy;
         u8g2.setCursor(5, y); u8g2.print("Mic + Q       : ESC"); y+=dy;
         u8g2.setCursor(5, y); u8g2.print("Mic + E       : TAB"); y+=dy;
         u8g2.setCursor(5, y); u8g2.print("Alt + 1-9     : F1-F9"); y+=dy;
         u8g2.setCursor(5, y); u8g2.print("Alt + B       : Backlight"); y+=dy;
         u8g2.setCursor(5, y); u8g2.print("Hold Side Btn : Sleep"); y+=dy;
         u8g2.setCursor(5, y); u8g2.print("Mic Key       : Ctrl"); y+=dy;
         y+=4;
         u8g2.setCursor(5, y); u8g2.print("Menu Nav:"); y+=dy;
         u8g2.setCursor(5, y); u8g2.print("Mic + W       : Up"); y+=dy;
         u8g2.setCursor(5, y); u8g2.print("Mic + S       : Down"); y+=dy;
         
         // Footer
         display.fillRect(0, h - 16, w, 16, GxEPD_BLACK);
         u8g2.setForegroundColor(GxEPD_WHITE);
         u8g2.setBackgroundColor(GxEPD_BLACK);
         u8g2.setFont(u8g2_font_profont12_tf); 
         u8g2.setCursor(5, h - 4);
         u8g2.print("Press Key to Close");
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
