#include "states/app_locked_state.h"
#include "states/app_menu_state.h"
#include "app.h"

AppLockedState::AppLockedState() : _pin("") {
}

void AppLockedState::enter(App& app) {
    app.display().setRefreshMode(true); // Enable fast refresh for typing
    redraw(app);
}

void AppLockedState::exit(App& app) {
    app.ui().drawMessage("UNLOCKED", "System Ready");
    delay(1000); // Brief pause to show success
    app.display().setRefreshMode(false);
}

void AppLockedState::update(App& app) {
    // Non-blocking input check
    InputEvent ev = app.pollInputs();
    
    if (ev.type == EVENT_KEY_PRESS) {
        char key = ev.key;
        bool changed = false;
        bool enter = false;

        if (key == '\n' || key == '\r') {
            if (_pin.length() > 0) enter = true;
        } 
        else if (key == 0x08) { // Backspace
            if (_pin.length() > 0) {
                _pin.remove(_pin.length() - 1);
                changed = true;
            }
        } 
        else if (key >= 32 && key <= 126) {
            _pin += key;
            changed = true;
        }

        if (enter) {
            app.ui().drawMessage("Verifying", "Please wait...");
            if (app.security().authenticate(_pin)) {
                // Success - Transition to Main Menu
                app.changeState(new AppMenuState());
            } else {
                // Failure
                _pin = "";
                redraw(app, true);
                delay(500); // Brief pause for error
                redraw(app);
            }
        } else if (changed) {
            redraw(app);
        }
    }
}

void AppLockedState::onRefresh(App& app) {
    redraw(app);
}

void AppLockedState::redraw(App& app, bool error) {
    if (error) {
        app.ui().drawPinEntry("ACCESS DENIED", "Try Again:", "", true);
    } else {
        app.ui().drawPinEntry("SECURE BOOT", "Enter PIN:", _pin, false);
    }
}
