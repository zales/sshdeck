#include "ui/menu_system.h"
#include "board_def.h"

MenuSystem::MenuSystem(DisplayManager& d, KeyboardManager& k) 
    : display(d), keyboard(k) {
}

void MenuSystem::setIdleCallback(std::function<void()> callback) {
    idleCallback = callback;
}

int MenuSystem::showMenu(const String& title, const std::vector<String>& items) {
    int selected = 0;
    bool needsRedraw = true;
    
    // Clear keyboard buffer
    while(keyboard.available()) keyboard.getKeyChar();
    
    while (true) {
        if (idleCallback) idleCallback();

        if (needsRedraw) {
            display.setRefreshMode(true); // Partial refresh for speed
            display.firstPage();
            do {
                renderMenu(title, items, selected);
            } while (display.nextPage());
            needsRedraw = false;
        }
        
        // Wait for input
        if (keyboard.available()) {
            char c = keyboard.getKeyChar();
            
            // Handle Navigation (Simulating with keys for now if no dedicated arrow keys mapped well)
            // Assuming 'w'/'s' or actual arrow mapping from keyboard manager
            // Check KeyboardManager mappings: 
            // w=Up, s=Down, Enter=Select, Backspace/Esc=Cancel
            
            if (c == 0) continue; // Modifier change or release

            bool handled = false;

            // Simple navigation mapping
            bool up = false;
            bool down = false;
            
            // W / S (Standard WASD)
            if (c == 'w') up = true;
            if (c == 's') down = true;
            
            // Mic + W/S (Ctrl) - consistent with Terminal navigation
            if (c == 'w' && keyboard.isMicActive()) up = true;
            if (c == 's' && keyboard.isMicActive()) down = true;
            
            // Alt + P/N (Legacy support removed for clarity)

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

void MenuSystem::renderMenu(const String& title, const std::vector<String>& items, int selectedIndex) {
    display.clear();
    
    int w = display.getWidth();
    int h = display.getHeight();
    
    U8G2_FOR_ADAFRUIT_GFX& u8g2 = display.getFonts();
    u8g2.setFontMode(1); // Transparent
    u8g2.setFontDirection(0);
    
    // --- Header ---
    display.fillRect(0, 0, w, 24, GxEPD_BLACK);
    u8g2.setForegroundColor(GxEPD_WHITE);
    u8g2.setBackgroundColor(GxEPD_BLACK);
    u8g2.setFont(u8g2_font_helvB12_tr);
    
    // Center Title
    int titleW = u8g2.getUTF8Width(title.c_str());
    int titleX = (w - titleW) / 2;
    if (titleX < 5) titleX = 5;
    u8g2.setCursor(titleX, 18);
    u8g2.print(title);
    
    // --- Items ---
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);
    u8g2.setFont(u8g2_font_helvR12_tr);
    
    int startY = 40;
    int lineH = 22;
    int maxItems = (h - startY - 20) / lineH; // Leave room for footer
    
    // Scroll window logic could be added here if items > maxItems
    // For now, let's assume simple sliding window if needed, or just basic clip
    // Basic Scrolling: Ensure selectedIndex is visible.
    
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
            display.fillRect(2, startY + (i * lineH), w-4, lineH, GxEPD_BLACK);
            u8g2.setForegroundColor(GxEPD_WHITE);
            u8g2.setBackgroundColor(GxEPD_BLACK);
        } else {
            u8g2.setForegroundColor(GxEPD_BLACK);
            u8g2.setBackgroundColor(GxEPD_WHITE);
        }
        
        u8g2.setCursor(10, y);
        u8g2.print(items[idx]); // Clip text if too long?
    }
    
    // --- Footer ---
    int footerH = 16;
    display.fillRect(0, h - footerH, w, footerH, GxEPD_BLACK);
    u8g2.setForegroundColor(GxEPD_WHITE);
    u8g2.setBackgroundColor(GxEPD_BLACK);
    u8g2.setFont(u8g2_font_profont12_tr); // Smaller font for footer
    u8g2.setCursor(5, h - 4);
    
    if (items.size() > maxItems) {
         u8g2.print("Scroll: W/S | Select: Enter | Back: Esc");
    } else {
         u8g2.print("Nav: W/S | Select: Enter | Back: Esc");
    }
}

bool MenuSystem::textInput(const String& title, String& result, bool isPassword) {
    bool needsRedraw = true;
    
    while(keyboard.available()) keyboard.getKeyChar(); // Flush
    
    while (true) {
        if (idleCallback) idleCallback();

        if (needsRedraw) {
            display.setRefreshMode(true);
            display.firstPage();
            do {
                renderInput(title, result); // pass raw result so we can mask if needed render-side
            } while (display.nextPage());
            needsRedraw = false;
        }
        
        if (keyboard.available()) {
            char c = keyboard.getKeyChar();
            
            if (c == '\n') return true;
            // ESC (0x1B), Ctrl+C (0x03), Ctrl+Q (0x11)
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

void MenuSystem::renderInput(const String& title, const String& currentText) {
    display.clear();
    int w = display.getWidth();
    
    U8G2_FOR_ADAFRUIT_GFX& u8g2 = display.getFonts();
    
    // Title
    display.fillRect(0, 0, w, 24, GxEPD_BLACK);
    u8g2.setForegroundColor(GxEPD_WHITE);
    u8g2.setBackgroundColor(GxEPD_BLACK);
    u8g2.setFont(u8g2_font_helvB12_tr);
    u8g2.setCursor(5, 18);
    u8g2.print(title);
    
    // Input Box
    u8g2.setForegroundColor(GxEPD_BLACK);
    u8g2.setBackgroundColor(GxEPD_WHITE);
    int boxW = w - 20;
    display.fillRect(10, 50, boxW, 30, GxEPD_WHITE);
    // Draw border (simple rects)
    display.fillRect(10, 50, boxW, 2, GxEPD_BLACK);      // Top
    display.fillRect(10, 80, boxW, 2, GxEPD_BLACK);      // Bottom
    display.fillRect(10, 50, 2, 32, GxEPD_BLACK);        // Left
    display.fillRect(10 + boxW - 2, 50, 2, 32, GxEPD_BLACK); // Right

    u8g2.setFont(u8g2_font_helvR12_tr);
    u8g2.setCursor(15, 72);
    u8g2.print(currentText);
    u8g2.print("_"); // Cursor

    // Footer
    int h = display.getHeight();
    int footerH = 16;
    display.fillRect(0, h - footerH, w, footerH, GxEPD_BLACK);
    u8g2.setForegroundColor(GxEPD_WHITE);
    u8g2.setBackgroundColor(GxEPD_BLACK);
    u8g2.setFont(u8g2_font_profont12_tr); 
    u8g2.setCursor(5, h - 4);
    u8g2.print("Type: Keys | Enter: OK | Esc: Cancel");
}

void MenuSystem::drawMessage(const String& title, const String& msg) {
    display.setRefreshMode(false);
    display.firstPage();
    do {
         display.clear();
         int w = display.getWidth();
         int h = display.getHeight();
         U8G2_FOR_ADAFRUIT_GFX& u8g2 = display.getFonts();
         
         display.fillRect(0, 0, w, 24, GxEPD_BLACK);
         u8g2.setForegroundColor(GxEPD_WHITE);
         u8g2.setBackgroundColor(GxEPD_BLACK);
         u8g2.setFont(u8g2_font_helvB12_tr);
         u8g2.setCursor(5, 18);
         u8g2.print(title);
         
         u8g2.setForegroundColor(GxEPD_BLACK);
         u8g2.setBackgroundColor(GxEPD_WHITE);
         u8g2.setFont(u8g2_font_helvR12_tr);
         u8g2.setCursor(10, 50);
         u8g2.print(msg);

         // Footer
        int footerH = 16;
        display.fillRect(0, h - footerH, w, footerH, GxEPD_BLACK);
        u8g2.setForegroundColor(GxEPD_WHITE);
        u8g2.setBackgroundColor(GxEPD_BLACK);
        u8g2.setFont(u8g2_font_profont12_tr); 
        u8g2.setCursor(5, h - 4);
        u8g2.print("Press any key to close");

    } while(display.nextPage());
    delay(500); // Debounce
    
    // Wait for key
    while(true) {
        if (idleCallback) idleCallback();
        if(keyboard.isKeyPressed()) {
             keyboard.getKeyChar(); // consume
             break;
        }
        delay(50);
    }
}
