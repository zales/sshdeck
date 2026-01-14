#include "ui/menu_system.h"
#include "board_def.h"
#include "keyboard_manager.h" // Added for blocking wrappers

MenuSystem::MenuSystem(UIManager& u) 
    : ui(u), state(MENU_IDLE), confirmed(false) {
}

void MenuSystem::showMenu(const String& title, const std::vector<String>& items) {
    config.title = title;
    config.items = items;
    config.selected = 0;
    state = MENU_LIST;
    confirmed = false;
    draw();
}

void MenuSystem::showInput(const String& title, const String& initial, bool isPassword) {
    config.title = title;
    config.inputText = initial;
    config.isPassword = isPassword;
    state = MENU_INPUT;
    confirmed = false;
    draw();
}

void MenuSystem::showMessage(const String& title, const String& msg) {
    config.title = title;
    config.message = msg;
    state = MENU_MESSAGE;
    confirmed = false;
    draw();
}

void MenuSystem::draw() {
    switch (state) {
        case MENU_LIST:
            ui.drawMenu(config.title, config.items, config.selected);
            break;
        case MENU_INPUT:
            ui.drawInputScreen(config.title, config.inputText, config.isPassword);
            break;
        case MENU_MESSAGE:
            ui.drawMessage(config.title, config.message);
            break;
        case MENU_IDLE:
        default:
            break;
    }
}

bool MenuSystem::handleInput(InputEvent e) {
    if (state == MENU_IDLE) return false;
    
    // Ignore non-keypress events for now
    if (e.type != EVENT_KEY_PRESS) return false;
    
    char c = e.key;
    if (c == 0) return false;

    if (state == MENU_LIST) {
        bool changed = false;
        
        bool up = (c == 'w'); 
        bool down = (c == 's'); 
        // Additional mapping can be handled by caller or here
        
        if (up) {
            config.selected--;
            if (config.selected < 0) config.selected = config.items.size() - 1;
            changed = true;
        } else if (down) {
            config.selected++;
            if (config.selected >= (int)config.items.size()) config.selected = 0;
            changed = true;
        } else if (c == '\n') { // Enter
            confirmed = true;
            state = MENU_IDLE; 
            return true; 
        } else if (c == 0x1B || c == 0x03 || c == 0x11) { // ESC
            confirmed = false;
            config.selected = -1;
            state = MENU_IDLE;
            return true;
        }
        
        if (changed) {
            draw();
            return true;
        }
    } 
    else if (state == MENU_INPUT) {
        if (c == '\n') {
            confirmed = true;
            state = MENU_IDLE;
            return true;
        } else if (c == 0x1B || c == 0x03 || c == 0x11) {
            confirmed = false;
            state = MENU_IDLE;
            return true;
        } else if (c == 0x08) { // Backspace
            if (config.inputText.length() > 0) {
                config.inputText.remove(config.inputText.length() - 1);
                draw();
                return true;
            }
        } else if (c >= 32 && c <= 126) {
            config.inputText += c;
            draw();
            return true;
        }
    }
    else if (state == MENU_MESSAGE) {
        // Any key dismisses
        confirmed = true;
        state = MENU_IDLE;
        return true;
    }

    return false;
}

int MenuSystem::showMenuBlocking(const String& title, const std::vector<String>& items, KeyboardManager& kb, std::function<void()> idleCb) {
    showMenu(title, items);
    while (isRunning()) {
        // Poll keyboard
        kb.loop(); 
        if (idleCb) idleCb();
        if (kb.available()) {
            InputEvent e;
            e.type = EVENT_KEY_PRESS;
            e.key = kb.getKeyChar();
            handleInput(e);
        } else {
             delay(10);
        }
    }
    return isConfirmed() ? getSelection() : -1;
}

bool MenuSystem::textInputBlocking(const String& title, String& result, KeyboardManager& kb, bool isPassword, std::function<void()> idleCb) {
    showInput(title, result, isPassword);
    while (isRunning()) {
        kb.loop();
        if (idleCb) idleCb();
        if (kb.available()) {
            InputEvent e;
            e.type = EVENT_KEY_PRESS;
            e.key = kb.getKeyChar();
            handleInput(e);
        } else {
             delay(10);
        }
    }
    if (isConfirmed()) {
        result = getInputResult();
        return true;
    }
    return false;
}


void MenuSystem::showMessageBlocking(const String& title, const String& msg, KeyboardManager& kb) {
    showMessage(title, msg);
    delay(500); // Debounce initial press
    while (isRunning()) {
        kb.loop();
        if (kb.available()) {
            InputEvent e;
            e.type = EVENT_KEY_PRESS;
            e.key = kb.getKeyChar();
            handleInput(e);
        } else {
             delay(10);
        }
    }
}
