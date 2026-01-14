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
            ui.drawMenu(title, items, selected);
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


bool MenuSystem::textInput(const String& title, String& result, bool isPassword) {
    bool needsRedraw = true;
    
    keyboard.clearBuffer();
    
    while (true) {
        if (idleCallback) idleCallback();

        if (needsRedraw) {
            ui.drawInputScreen(title, result, isPassword);
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
