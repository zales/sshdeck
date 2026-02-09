#pragma once
#include "ui_manager.h"
#include "event_types.h"
#include "touch_manager.h"
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
    int scrollOffset; 
    
    // Callbacks
    std::function<void(int)> onSelect;       // For Lists
    std::function<void(String)> onInput;     // For Text Input
    std::function<void()> onDismiss;         // For Messages
    std::function<void()> onBack;            // When ESC/Back is pressed
    std::function<void()> onLoop;            // Called every loop iteration
};

class MenuSystem {
public:
    MenuSystem(UIManager& ui);
    
    // Setup methods
    void showMenu(const String& title, const std::vector<String>& items, std::function<void(int)> onSelect, std::function<void()> onBack = nullptr);
    void showInput(const String& title, const String& initial, bool isPassword, std::function<void(String)> onInput, std::function<void()> onBack = nullptr);
    void showMessage(const String& title, const String& msg, std::function<void()> onDismiss = nullptr);
    void updateMessage(const String& msg);
    
    // The Loop methods
    bool handleInput(InputEvent e, bool suppressDraw = false); 
    bool handleTouch(TouchEvent t, bool suppressDraw = false);
    void draw(bool partial = false, int prevSelected = -1);  
    void reset();

    // State getters
    bool isRunning() const { return state != MENU_IDLE; }
    
    void setOnLoop(std::function<void()> cb) { config.onLoop = cb; }
    std::function<void()> getOnLoop() const { return config.onLoop; }

private:
    UIManager& ui;
    MenuState state;
    MenuConfig config;
};
