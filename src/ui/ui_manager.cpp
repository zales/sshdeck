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
    display.setRefreshMode(true); // Partial refresh for responsive PIN typing
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

void UIManager::drawMessage(const String& title, const String& message, bool partial) {
    display.setRefreshMode(partial);
    render([&](U8G2_FOR_ADAFRUIT_GFX& u8g2) {
        drawHeader(title);
        
        // Center the message roughly
        int msgY = height() / 2 - 20; // Adjusted for multi-line
        
        u8g2.setFont(u8g2_font_helvR12_tr);
        u8g2.setForegroundColor(GxEPD_BLACK);
        u8g2.setBackgroundColor(GxEPD_WHITE);
        
        // Split by newlines
        int y = msgY;
        int lineHeight = 20;
        
        String temp = message;
        while (temp.length() > 0) {
            int idx = temp.indexOf('\n');
            String line = (idx == -1) ? temp : temp.substring(0, idx);
            
            int w = u8g2.getUTF8Width(line.c_str());
            u8g2.setCursor((width() - w)/2, y);
            u8g2.print(line);
            
            y += lineHeight;
            if (idx == -1) break;
            temp = temp.substring(idx + 1);
        }
    
        drawFooter("Press Key");
    });
}

void UIManager::drawSystemInfo(const String& ip, const String& bat, const String& ram, const String& mac) {
    setRefreshMode(true); // Partial refresh
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

void UIManager::drawMenu(const String& title, const std::vector<String>& items, int selectedIndex, bool navOnly) {
    
    const int itemsStartY = 32;
    
    if (navOnly) {
        // Navigation only: restrict partial window to items area, excluding the header.
        // This prevents the black header bar from fading due to repeated partial refreshes —
        // e-ink partial waveforms don't fully drive particles, so repeated partials weaken contrast.
        display.setPartialWindow(0, itemsStartY, display.getWidth(), display.getHeight() - itemsStartY);
    } else {
        // Initial draw or sub-menu transition: full-screen partial refresh.
        // After fullClean() in state enter, the controller is properly reset and partial
        // refresh works correctly on a clean white canvas. Using partial here (vs full)
        // is faster (~700ms vs ~1100ms) and the header renders with good contrast
        // because the old→new transition is white→black (maximum drive).
        setRefreshMode(true);
    }
    
    render([&](U8G2_FOR_ADAFRUIT_GFX& u8g2) {
        int w = display.getWidth();
        int h = display.getHeight();
        
        // Always draw header into buffer (GxEPD2 firstPage clears buffer to white).
        // When navOnly=true with partial window below header, the header drawing goes
        // into the buffer but is clipped out by the display driver — no e-ink refresh occurs.
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

// Helper to clamp values
template<typename T>
T clamp(T val, T min, T max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

bool UIManager::_headerContentChanged(const String& title, int bat, bool charging, bool wifi) {
    if (title != _lastHeaderTitle || bat != _lastHeaderBat ||
        charging != _lastHeaderCharging || wifi != _lastHeaderWifi) {
        _lastHeaderTitle = title;
        _lastHeaderBat = bat;
        _lastHeaderCharging = charging;
        _lastHeaderWifi = wifi;
        return true;
    }
    return false;
}

void UIManager::drawTerminal(const TerminalEmulator& term, const String& statusTitle, int batteryPercent, bool isCharging, bool wifiConnected, bool partial) {
    
    // Geometry constants
    const int headerH = 16;  // Height of status bar
    const int termYStart = 24;
    const int termLineH = 10;
    const int termCharW = 6;
    
    int winX = 0;
    int winY = 0;
    int winW = display.getWidth();
    int winH = display.getHeight();
    
    if (partial) {
        int startRow = -1, endRow = -1;
        bool termDirty = term.getDirtyRange(startRow, endRow);
        bool headerDirty = _headerContentChanged(statusTitle, batteryPercent, isCharging, wifiConnected);
        
        // If absolutely nothing changed, skip the entire refresh.
        // This saves ~700ms of blocking time and avoids any e-ink degradation.
        if (!termDirty && !headerDirty) {
            return;
        }
        
        // Calculate partial window based on what actually needs refreshing.
        // Key principle: EXCLUDE the header from partial window when its content hasn't
        // changed. E-ink partial LUT waveforms don't have a perfect "hold" — even
        // unchanged black pixels receive a small voltage that degrades them over time.
        // By only including the header when it truly changed, we prevent fading.
        int y1, y2;
        
        if (termDirty && headerDirty) {
            // Both changed — single window from header top to last dirty row
            y1 = 0;
            y2 = termYStart + (endRow * termLineH) + termLineH;
        } else if (headerDirty) {
            // Only header changed (e.g. battery % updated) — narrow header-only window
            y1 = 0;
            y2 = headerH;
        } else {
            // Only terminal content changed — window below header
            y1 = termYStart + (startRow * termLineH) - 8;
            y2 = termYStart + (endRow * termLineH) + termLineH;
        }
        
        y1 = clamp(y1, 0, (int)display.getHeight());
        y2 = clamp(y2, 0, (int)display.getHeight());
        
        winY = y1;
        winH = y2 - y1;
        if (winH <= 0) winH = termLineH;
        
        display.setPartialWindow(winX, winY, winW, winH);
    } else {
        display.setRefreshMode(false); // Full Refresh
        // Sync header state on full refresh so future partials start from known baseline
        _headerContentChanged(statusTitle, batteryPercent, isCharging, wifiConnected);
    }

    render([&](U8G2_FOR_ADAFRUIT_GFX& u8g2) {
        
        // Always draw status bar because render() might clear the buffer
        // Note: If partial window is set to NOT include the status bar area (y < 16),
        // this draw call will be clipped/ignored by the display driver efficiently.
        // So it is safe to act as if we are drawing the whole screen.
        drawStatusBar(statusTitle, wifiConnected, batteryPercent, isCharging);

        u8g2.setFont(u8g2_font_6x10_tf);
        
        // Terminal Content
        for (int row = 0; row < TERM_ROWS; row++) {
            // We draw all rows. The clipping of the Partial Window takes care of optimization.
            
            const char* line = term.getLine(row);
            int py = termYStart + (row * termLineH);
            
            // Optimization inside the loop:
            // Check if this row is even visible within our current Partial Window
            // We can skip drawing commands for rows totally outside the window to save SPI/Buffer operations.
            // If partial mode only covers rows 10-12, row 0 draws are wasted CPU cycles.
            if (partial) {
                 int rowTop = py - 8;
                 int rowBottom = py + termLineH;
                 if (rowBottom < winY || rowTop > (winY + winH)) {
                     continue;
                 }
            }

            // Clear the line area first to avoid artifacts
            display.fillRect(0, py - 8, display.getWidth(), termLineH, GxEPD_WHITE);
            
            if (line[0] != '\0') {
                int col = 0;
                while (line[col] != '\0' && col < TERM_COLS) {
                    bool inv = term.getAttr(row, col).inverse;
                    int startCol = col;
                    char buf[TERM_COLS + 1];
                    int bi = 0;
                    
                    // Batch consecutive characters with the same inverse attribute
                    while (col < TERM_COLS && line[col] != '\0' && term.getAttr(row, col).inverse == inv) {
                        buf[bi++] = line[col++];
                    }
                    buf[bi] = '\0';
                    
                    int x = startCol * termCharW;
                    int y = py;
                    
                    if (inv) {
                        display.fillRect(x, y - 8, bi * termCharW, termLineH, GxEPD_BLACK);
                        u8g2.setForegroundColor(GxEPD_WHITE);
                        u8g2.setBackgroundColor(GxEPD_BLACK);
                    } else {
                        u8g2.setForegroundColor(GxEPD_BLACK);
                        u8g2.setBackgroundColor(GxEPD_WHITE);
                    }
                    u8g2.setCursor(x, y);
                    u8g2.print(buf);
                }
            }
        }
        if (term.isCursorVisible()) {
            int cx = term.getCursorX();
            int cy = term.getCursorY();
            if (cx >= 0 && cx < TERM_COLS && cy >= 0 && cy < TERM_ROWS) {
                int c_x_px = cx * termCharW;
                int c_y_px = termYStart + (cy * termLineH);
                
                // Cursor visibility check against partial window
                bool skipCursor = false;
                if (partial) {
                    if (c_y_px + termLineH < winY || c_y_px - 8 > (winY + winH)) skipCursor = true;
                }
                
                if (!skipCursor) {
                    char cursorChar = ' ';
                    const char* line = term.getLine(cy);
                    int len = 0;
                    while(line[len] != 0 && len < TERM_COLS) len++;
                    
                    if (cx < len) cursorChar = line[cx];
                    
                    display.fillRect(c_x_px, c_y_px - 8, termCharW, termLineH, GxEPD_BLACK);
                    u8g2.setForegroundColor(GxEPD_WHITE);
                    u8g2.setBackgroundColor(GxEPD_BLACK);
                    u8g2.setCursor(c_x_px, c_y_px);
                    u8g2.print(cursorChar);
                }
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
    // Note: firstPage() already calls fillScreen(WHITE) internally in GxEPD2,
    // so we don't need display.clear() here — it would be a redundant memset.
    do {
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
