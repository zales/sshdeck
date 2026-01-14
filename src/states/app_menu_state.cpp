#include "states/app_menu_state.h"
#include "app.h"

void AppMenuState::enter(App& app) {
    // Perform a Full Refresh Clear to remove ghosting from previous states (e.g. Terminal)
    app.display.lock();
    app.display.fullClean();
    app.display.unlock();

    if (app.menu) {
        app.handleMainMenu();
    }
}

void AppMenuState::update(App& app) {
    if (!app.menu) return;

    if (app.menu->getOnLoop()) {
        app.menu->getOnLoop()();
    }

    bool needsRedraw = false;
    unsigned long startTime = millis();
    // Time budget of 10ms for input processing to keep UI responsive
    // without blocking the loop for too long
    while(app.keyboard.available() && (millis() - startTime < 10)) {
        InputEvent event = app.pollInputs();
        
        if (event.type == EVENT_NONE) break;

        if (event.type == EVENT_SYSTEM && event.systemCode == SYS_EVENT_SLEEP) {
            app.enterDeepSleep();
            return;
        }
        
        // Only draw on the LAST event of the batch or if we ran out of time
        bool isLast = !app.keyboard.available();
        bool suppressDraw = !isLast;
        
        if (app.menu->handleInput(event, suppressDraw)) {
             if (suppressDraw) needsRedraw = true;
        }
    }
    
    // Explicit redraw if we suppressed it during batch processing
    if (needsRedraw) {
        app.display.lock();
        app.menu->draw();
        app.display.unlock();
    }
    
    // If menu becomes idle (and no callback handled it), force Main Menu
    if (!app.menu->isRunning()) {
        app.handleMainMenu();
    }
}

void AppMenuState::exit(App& app) {
    // Cleanup if needed
}
