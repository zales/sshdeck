#include "ui/menu_system.h"
#include "board_def.h"
#include "keyboard_manager.h"

MenuSystem::MenuSystem(UIManager& u) 
    : ui(u), state(MENU_IDLE) {
}

void MenuSystem::showMenu(const String& title, const std::vector<String>& items, std::function<void(int)> onSelect, std::function<void()> onBack) {
    config.title = title;
    config.items = items;
    config.selected = -1; // Start with no selection for touch-first UX
    config.scrollOffset = 0;
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

void MenuSystem::draw(bool partial, int prevSelected) {
    switch (state) {
        case MENU_LIST:
            ui.drawMenu(config.title, config.items, config.selected, config.scrollOffset, partial, prevSelected, (bool)config.onBack);
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

bool MenuSystem::handleTouch(TouchEvent t, bool suppressDraw) {
    if (state == MENU_IDLE) return false;
    
    // Only react to completed gestures
    if (t.gesture == GESTURE_NONE) return false;
    
    int h = ui.height();
    const int startY = MENU_START_Y;
    int footerH = (config.onBack) ? MENU_FOOTER_H : 0;
    const int lineH = MENU_ITEM_H;
    
    // 1. Check footer (Back Button)
    if (t.gesture == GESTURE_SINGLE_TAP && t.y > h - footerH && footerH > 0) {
        // Bottom area tapped.
        // Used for BACK
        if (config.onBack) {
            if (KeyboardManager::getInstance()) KeyboardManager::getInstance()->triggerHaptic();
            config.onBack();
        }
        return true;
    }
    
    if (state == MENU_LIST) {
        int listH = h - startY - footerH;
        int maxItems = listH / lineH;

        int oldOffset = config.scrollOffset;
        
        // Dynamic scroll speed based on swipe distance (Kinetic-like)
        // Ensure at least 1 item step. Scaling factor: 1 item per lineH pixels swiped
        int itemsToScroll = 1;
        if (t.magnitude > 0) {
            itemsToScroll = t.magnitude / lineH; 
            if (t.magnitude % lineH > lineH/2) itemsToScroll++; // Round up
            if (itemsToScroll < 1) itemsToScroll = 1;
        }

        bool captured = false;

        // Handle Swipes for Scrolling (View Scroll)
        // Scroll Up (Swipe Up) -> View moves down (Offset Increases) -> See items below
        if (t.gesture == GESTURE_SWIPE_UP) {
             config.scrollOffset += itemsToScroll;
             captured = true;
        }
        // Scroll Down (Swipe Down) -> View moves up (Offset Decreases) -> See items above
        else if (t.gesture == GESTURE_SWIPE_DOWN) {
             config.scrollOffset -= itemsToScroll;
             captured = true;
        }

        if (captured) {
             // Bounds Checking
             if (config.items.size() > (size_t)maxItems) {
                 if (config.scrollOffset + maxItems > (int)config.items.size()) {
                     config.scrollOffset = config.items.size() - maxItems;
                 }
             } else {
                 config.scrollOffset = 0;
             }
             if (config.scrollOffset < 0) config.scrollOffset = 0;

             // Only redraw if valid scroll happened
             if (config.scrollOffset != oldOffset) {
                 draw(true);
             }
             return true;
        }
        
        int offset = config.scrollOffset; // Use stored offset
        
        // Item Click (Tap)
        if (t.gesture == GESTURE_SINGLE_TAP && t.y >= startY && t.y < startY + maxItems * lineH) {
            int clickedRow = (t.y - startY) / lineH;
            int finalIdx = offset + clickedRow;
            
            if (finalIdx >= 0 && finalIdx < (int)config.items.size()) {
                config.selected = finalIdx;
                // Execute Selection
                if (config.onSelect) {
                    if (KeyboardManager::getInstance()) KeyboardManager::getInstance()->triggerHaptic();
                    config.onSelect(config.selected);
                }
                return true;
            }
        }
    }
    else if (state == MENU_MESSAGE) {
        if (t.gesture == GESTURE_SINGLE_TAP) {
            if (config.onDismiss) {
                if (KeyboardManager::getInstance()) KeyboardManager::getInstance()->triggerHaptic();
                config.onDismiss();
            }
            return true;
        }
    }
    
    return false;
}

bool MenuSystem::handleInput(InputEvent e, bool suppressDraw) {
    if (state == MENU_IDLE) return false;
    
    // Ignore non-keypress events for now
    if (e.type != EVENT_KEY_PRESS) return false;
    
    char c = e.key;
    if (c == 0) return false;

    if (state == MENU_LIST) {
        bool changed = false;
        int maxItems = (ui.height() - MENU_START_Y - ((config.onBack) ? MENU_FOOTER_H : 0)) / MENU_ITEM_H;
        
        int oldSelected = config.selected;
        int oldOffset = config.scrollOffset;
        
        bool up = (c == 'w'); 
        bool down = (c == 's'); 
        
        if (up) {
            if (config.selected == -1) {
                config.selected = config.items.size() - 1;
            } else {
                config.selected--;
                if (config.selected < 0) config.selected = config.items.size() - 1;
            }
            changed = true;
        } else if (down) {
            if (config.selected == -1) {
                config.selected = 0;
            } else {
                config.selected++;
                if (config.selected >= (int)config.items.size()) config.selected = 0;
            }
            changed = true;
        } else if (c == '\n') { // Enter
            if (config.selected != -1) {
                if (config.onSelect) config.onSelect(config.selected);
            }
            return true; 
        } else if (c == 0x1B || c == 0x03 || c == 0x11) { // ESC
            if (config.onBack) config.onBack();
            return true;
        }
        
        // Ensure scrollOffset follows selection for Keyboard
        if (changed) {
            if (maxItems > 0 && config.selected >= 0) {
                if (config.selected < config.scrollOffset) {
                    config.scrollOffset = config.selected;
                } else if (config.selected >= config.scrollOffset + maxItems) {
                    config.scrollOffset = config.selected - maxItems + 1;
                }
            }
            if (!suppressDraw) {
                // Determine if we can use optimized draw
                // Only if offset didn't change!
                if (config.scrollOffset == oldOffset) {
                    draw(true, oldSelected);
                } else {
                    draw(true, -1); // changed offset = full redraw
                }
            }
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
