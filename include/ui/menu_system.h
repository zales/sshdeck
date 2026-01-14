#pragma once
#include "ui_manager.h"
#include "event_types.h"
#include <vector>
#include <functional>

enum MenuState {
    MENU_IDLE,
    MENU_LIST,
    MENU_INPUT,
    MENU_MESSAGE
};

struct MenuConfig {
    String title;
    std::vector<String> items;
    String inputText;
    String message;
    bool isPassword;
    int selected;
};

class MenuSystem {
public:
    MenuSystem(UIManager& ui);
    
    // Setup methods (prepare state, do not block)
    void showMenu(const String& title, const std::vector<String>& items);
    void showInput(const String& title, const String& initial = "", bool isPassword = false);
    void showMessage(const String& title, const String& msg);
    
    // The Loop methods
    // Returns true if state changed or redraw needed
    bool handleInput(InputEvent e); 
    void draw(); 

    // Result getters
    bool isRunning() const { return state != MENU_IDLE; }
    int getSelection() const { return config.selected; }
    String getInputResult() const { return config.inputText; }
    bool isConfirmed() const { return confirmed; } 

    // Reset to idle
    void stop() { state = MENU_IDLE; }

    // Blocking wrappers for legacy compatibility
    // These require passing KeyboardManager explicitly since MenuSystem is now decoupled
    int showMenuBlocking(const String& title, const std::vector<String>& items, class KeyboardManager& kb, std::function<void()> idleCb = nullptr);
    bool textInputBlocking(const String& title, String& result, class KeyboardManager& kb, bool isPassword = false, std::function<void()> idleCb = nullptr);
    void showMessageBlocking(const String& title, const String& msg, class KeyboardManager& kb);

private:
    UIManager& ui;
    MenuState state;
    MenuConfig config;
    bool confirmed;
};
