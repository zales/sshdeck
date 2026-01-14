#include "ui/menu_system.h"
#include "board_def.h"

MenuSystem::MenuSystem(UIManager& u) 
    : ui(u), state(MENU_IDLE) {
}

void MenuSystem::showMenu(const String& title, const std::vector<String>& items, std::function<void(int)> onSelect, std::function<void()> onBack) {
    config.title = title;
    config.items = items;
    config.selected = 0;
    config.onSelect = onSelect;
    config.onBack = onBack;
    config.onLoop = nullptr; // Clear previous loop handlers
    state = MENU_LIST;
    draw();
}

void MenuSystem::showInput(const String& title, const String& initial, bool isPassword, std::function<void(String)> onInput, std::function<void()> onBack) {
    config.title = title;
    config.inputText = initial;
    config.isPassword = isPassword;
    config.onInput = onInput;
    config.onBack = onBack;
    config.onLoop = nullptr; // Clear previous loop handlers
    state = MENU_INPUT;
    draw();
}

void MenuSystem::showMessage(const String& title, const String& msg, std::function<void()> onDismiss) {
    config.title = title;
    config.message = msg;
    config.onDismiss = onDismiss;
    config.onLoop = nullptr; // Clear previous loop handlers
    state = MENU_MESSAGE;
    draw();
}

void MenuSystem::updateMessage(const String& msg) {
    if (state != MENU_MESSAGE) return;
    config.message = msg;
    draw(true); // Partial refresh
}

void MenuSystem::reset() {
    state = MENU_IDLE;
    // Clear callbacks to break cycles
    config.onSelect = nullptr;
    config.onInput = nullptr;
    config.onDismiss = nullptr;
    config.onBack = nullptr;
    config.onLoop = nullptr;
}

void MenuSystem::draw(bool partial) {
    switch (state) {
        case MENU_LIST:
            ui.drawMenu(config.title, config.items, config.selected);
            break;
        case MENU_INPUT:
            ui.drawInputScreen(config.title, config.inputText, config.isPassword, partial);
            break;
        case MENU_MESSAGE:
            ui.drawMessage(config.title, config.message, partial);
            break;
        case MENU_IDLE:
        default:
            break;
    }
}

bool MenuSystem::handleInput(InputEvent e, bool suppressDraw) {
    if (state == MENU_IDLE) return false;
    
    // Ignore non-keypress events for now
    if (e.type != EVENT_KEY_PRESS) return false;
    
    char c = e.key;
    if (c == 0) return false;

    if (state == MENU_LIST) {
        bool changed = false;
        
        bool up = (c == 'w'); 
        bool down = (c == 's'); 
        
        if (up) {
            config.selected--;
            if (config.selected < 0) config.selected = config.items.size() - 1;
            changed = true;
        } else if (down) {
            config.selected++;
            if (config.selected >= (int)config.items.size()) config.selected = 0;
            changed = true;
        } else if (c == '\n') { // Enter
            if (config.onSelect) config.onSelect(config.selected);
            // Warning: onSelect might trigger a new state transition (e.g. showMenu), so 'state' changes.
            // But usually we just invoke and don't reset state here if we want to stay open, 
            // OR the callback calls another showMenu.
            // If the callback calls showMenu, state becomes MENU_LIST again.
            // If the callback calls nothing? We should probably stay or exit?
            // "ShowMenu" usually implies a stack or a new view.
            return true; 
        } else if (c == 0x1B || c == 0x03 || c == 0x11) { // ESC
            if (config.onBack) config.onBack();
            return true;
        }
        
        if (changed) {
            if (!suppressDraw) draw();
            return true;
        }
    } 
    else if (state == MENU_INPUT) {
        bool changed = false;
        if (c == '\n') {
            if (config.onInput) config.onInput(config.inputText);
            return true;
        } else if (c == 0x1B || c == 0x03 || c == 0x11) {
            if (config.onBack) config.onBack();
            return true;
        } else if (c == 0x08) { // Backspace
            if (config.inputText.length() > 0) {
                config.inputText.remove(config.inputText.length() - 1);
                changed = true;
            }
        } else if (c >= 32 && c <= 126) {
            config.inputText += c;
            changed = true;
        }

        if (changed) {
            if (!suppressDraw) draw(true);
            return true;
        }
    }
    else if (state == MENU_MESSAGE) {
        // Any key dismisses
        if (config.onDismiss) config.onDismiss();
        return true;
    }

    return false;
}
