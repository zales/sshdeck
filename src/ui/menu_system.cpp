#include "ui/menu_system.h"
#include "board_def.h"

MenuSystem::MenuSystem(UIManager& u, KeyboardManager& k) 
    : ui(u), keyboard(k) {
}

void MenuSystem::setIdleCallback(std::function<void()> callback) {
    idleCallback = callback;
}

int MenuSystem::showMenu(const String& title, const std::vector<String>& items) {
    int selected = 0;
    bool needsRedraw = true;
    
    // Clear keyboard buffer
    keyboard.clearBuffer(); 
    
    while (true) {
        if (idleCallback) idleCallback();

        if (needsRedraw) {
            ui.setRefreshMode(true); // Partial refresh
            ui.render([&](U8G2_FOR_ADAFRUIT_GFX& u8g2) {
                renderMenu(title, items, selected, u8g2);
            });
            needsRedraw = false;
        }
        
        // Wait for input
        if (keyboard.available()) {
            char c = keyboard.getKeyChar();
            
            if (c == 0) continue; 

            bool handled = false;
            bool up = false;
            bool down = false;
            
            if (c == 'w') up = true;
            if (c == 's') down = true;
            if (c == 'w' && keyboard.isMicActive()) up = true;
            if (c == 's' && keyboard.isMicActive()) down = true;
            
            if (up) { 
                 selected--;
                 if (selected < 0) selected = items.size() - 1;
                 handled = true;
            }
            else if (down) { 
                 selected++;
                 if (selected >= items.size()) selected = 0;
                 handled = true;
            }
            
            if (c == '\n') { // Enter
                return selected;
            }
            
            if (c == 0x1B || c == 0x03 || c == 0x11) { // ESC, Ctrl+C, Ctrl+Q
                return -1;
            }
            
            if (handled) {
                needsRedraw = true;
            }
        }
        delay(10);
    }
}

void MenuSystem::renderMenu(const String& title, const std::vector<String>& items, int selectedIndex, U8G2_FOR_ADAFRUIT_GFX& u8g2) {
    // ui.render already clears and sets default colors
    
    int w = ui.width();
    int h = ui.height();
    
    // --- Header ---
    ui.drawHeader(title);
    
    // --- Items ---
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);
    u8g2.setFont(u8g2_font_helvR12_tr);
    
    int startY = 32; // Adjusted for smaller header (16px)
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
        
        int y = startY + (i * lineH) + 16;
        
        if (idx == selectedIndex) {
            ui.fillRect(2, startY + (i * lineH), w-4, lineH, GxEPD_BLACK);
            u8g2.setForegroundColor(GxEPD_WHITE);
            u8g2.setBackgroundColor(GxEPD_BLACK);
        } else {
            u8g2.setForegroundColor(GxEPD_BLACK);
            u8g2.setBackgroundColor(GxEPD_WHITE);
        }
        
        u8g2.setCursor(10, y);
        u8g2.print(items[idx]);
    }
    
    // --- Footer ---
    int footerH = 16;
    ui.fillRect(0, h - footerH, w, footerH, GxEPD_BLACK);
    u8g2.setForegroundColor(GxEPD_WHITE);
    u8g2.setBackgroundColor(GxEPD_BLACK);
    u8g2.setFont(u8g2_font_profont12_tr);
    u8g2.setCursor(5, h - 4);
    
    if (items.size() > maxItems) {
         u8g2.print("W/S:Scroll Ent:Sel Esc:Back");
    } else {
         u8g2.print("W/S:Move Ent:Sel Esc:Back");
    }
}

bool MenuSystem::textInput(const String& title, String& result, bool isPassword) {
    bool needsRedraw = true;
    
    keyboard.clearBuffer();
    
    while (true) {
        if (idleCallback) idleCallback();

        if (needsRedraw) {
            ui.setRefreshMode(true);
            ui.render([&](U8G2_FOR_ADAFRUIT_GFX& u8g2) {
                renderInput(title, result, u8g2);
            });
            needsRedraw = false;
        }
        
        if (keyboard.available()) {
            char c = keyboard.getKeyChar();
            
            if (c == '\n') return true;
            if (c == 0x1B || c == 0x03 || c == 0x11) return false; 
            
            if (c == 0x08) { // Backspace
                if (result.length() > 0) {
                    result.remove(result.length() - 1);
                    needsRedraw = true;
                }
            } else if (c >= 32 && c <= 126) {
                result += c;
                needsRedraw = true;
            }
        }
        delay(10);
    }
}

void MenuSystem::renderInput(const String& title, const String& currentText, U8G2_FOR_ADAFRUIT_GFX& u8g2) {
    int w = ui.width();
    int h = ui.height();
    
    // Title
    ui.fillRect(0, 0, w, 24, GxEPD_BLACK);
    u8g2.setForegroundColor(GxEPD_WHITE);
    u8g2.setBackgroundColor(GxEPD_BLACK);
    u8g2.setFont(u8g2_font_helvB12_tr);
    u8g2.setCursor(5, 18);
    u8g2.print(title);
    
    // Input Box
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);
    int boxW = w - 20;
    ui.fillRect(10, 50, boxW, 30, GxEPD_WHITE);
    // Draw border
    ui.fillRect(10, 50, boxW, 2, GxEPD_BLACK);      // Top
    ui.fillRect(10, 80, boxW, 2, GxEPD_BLACK);      // Bottom
    ui.fillRect(10, 50, 2, 32, GxEPD_BLACK);        // Left
    ui.fillRect(10 + boxW - 2, 50, 2, 32, GxEPD_BLACK); // Right

    u8g2.setFont(u8g2_font_helvR12_tr);
    u8g2.setCursor(15, 72);
    u8g2.print(currentText);
    u8g2.print("_"); 

    // Footer
    int footerH = 16;
    ui.fillRect(0, h - footerH, w, footerH, GxEPD_BLACK);
    u8g2.setForegroundColor(GxEPD_WHITE);
    u8g2.setBackgroundColor(GxEPD_BLACK);
    u8g2.setFont(u8g2_font_profont12_tr); 
    u8g2.setCursor(5, h - 4);
    u8g2.print("Type: Keys | Enter: OK | Esc: Cancel");
}

void MenuSystem::drawMessage(const String& title, const String& msg) {
    ui.drawMessage(title, msg);
    
    // Wait for key
    delay(500);
    while(true) {
        if (idleCallback) idleCallback();
        if(keyboard.isKeyPressed()) {
             keyboard.getKeyChar(); // consume
             break;
        }
        delay(50);
    }
}
