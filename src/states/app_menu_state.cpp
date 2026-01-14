#include "states/app_menu_state.h"
#include "app.h"

void AppMenuState::enter(App& app) {
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
    int processed = 0;
    while(app.keyboard.available() && processed < 5) {
        InputEvent event = app.pollInputs();
        
        // Only draw on the LAST event of the batch
        bool isLast = !app.keyboard.available() || processed >= 4;
        bool suppressDraw = !isLast;
        
        if (app.menu->handleInput(event, suppressDraw)) {
             if (suppressDraw) needsRedraw = true;
        }
        processed++;
    }
    
    // Explicit redraw if we suppressed it during batch processing
    if (needsRedraw) {
        app.menu->draw();
    }
    
    // If menu becomes idle (and no callback handled it), force Main Menu
    if (!app.menu->isRunning()) {
        app.handleMainMenu();
    }
}

void AppMenuState::exit(App& app) {
    // Cleanup if needed
}
