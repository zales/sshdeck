#pragma once
#include "display_manager.h"
#include "keyboard_manager.h"
#include <vector>
#include <functional>

class MenuSystem {
public:
    MenuSystem(DisplayManager& display, KeyboardManager& keyboard);
    
    void setIdleCallback(std::function<void()> callback);

    // Shows a menu list and returns index. -1 for ESC.
    int showMenu(const String& title, const std::vector<String>& items);
    
    // Shows a prompt and fills result. Returns true if Enter pressed, false if ESC.
    bool textInput(const String& title, String& result, bool isPassword = false);
    
    void drawMessage(const String& title, const String& msg);

private:
    DisplayManager& display;
    KeyboardManager& keyboard;
    std::function<void()> idleCallback;
    
    void renderMenu(const String& title, const std::vector<String>& items, int selectedIndex);
    void renderInput(const String& title, const String& currentText);
};
