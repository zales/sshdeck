#include "states/app_terminal_state.h"
#include "states/app_menu_state.h"
#include "app.h"

void AppTerminalState::enter(App& app) {
    // Nothing special on enter, connection is handled by App::connectToServer for now
    app.drawTerminalScreen(false); // Full refresh on enter
}

void AppTerminalState::update(App& app) {
    // 1. Process ALL pending inputs at once to speed up typing
    // We check for keyboard availability directly to drain the buffer
    int processed = 0;
    while(app.keyboard.available() && processed < 5) {
        InputEvent event = app.pollInputs();

        if (event.type == EVENT_NONE) break;

        if (event.type == EVENT_SYSTEM && event.systemCode == SYS_EVENT_SLEEP) {
            app.enterDeepSleep();
            return;
        }

        if (app.sshClient && app.sshClient->isConnected()) {
            if (event.type == EVENT_KEY_PRESS && event.key != 0) {
                app.sshClient->write(event.key); // Send to network buffer
            }
        }
        processed++;
    }

    // Pass one phantom event if loop needs it, or just rely on the fact pollInputs was called
    // actually, pollInputs handles SYS events too, so we should be careful.
    // Ideally we should process the *result* of pollInputs.
    // But pollInputs calls keyboard.loop().
    // Let's stick to the while loop above for fast typing, assuming polls are cheap.

    if (app.sshClient && app.sshClient->isConnected()) {
        // 2. Read available SSH data (bulk read)
        app.sshClient->process();
        
        bool forceRedraw = false;
        // Battery Charging Animation (every 1 sec)
        // Note: access to lastAniUpdate needs to be public or via getter/setter if we want to be strict,
        // but for now we made App members public.
        
        // Use a static here or move battery logic to App::loop completely?
        // Let's keep specific state logic here.
        static unsigned long lastAniUpdate = 0;

        if (app.power.isCharging() && (millis() - lastAniUpdate > 1000)) {
            lastAniUpdate = millis();
            forceRedraw = true;
        }

        if (app.terminal.needsUpdate() || forceRedraw) {
             // 3. Debounce Redraw
             // If data just arrived, wait a tiny bit to see if more is coming (Nagle-like)
             // But don't block. Just skip this frame if it's too soon since last meaningful data, UNLESS buffer is full.
             // Actually, for E-Paper, we just want to ensure we don't draw 5 times for "hello".
             
             // Simple logic: Only draw if enough time passed since LAST DRAW
             // App::drawTerminalScreen (wrapped in app.h but implemented in app.cpp) usually handles mutex,
             // but we need to control the frequency here.
             
             // Let's assume DISPLAY_UPDATE_INTERVAL_MS in config.h is our friend.
             // The original code checked: (millis() - lastScreenRefresh > DISPLAY_UPDATE_INTERVAL_MS)
             // But lastScreenRefresh was private in App.
             
             // We can check it via a new public getter or just rely on the fact that
             // we processed ALL data above. Since we drained input and output buffers,
             // drawing NOW is the best we can do for this loop iteration.
             
             app.drawTerminalScreen();
        }

        if (!app.sshClient->isConnected()) {
            app.ui.drawMessage("Disconnected", "Session Ended");
            delay(1000); // Give user time to see
            app.changeState(new AppMenuState());
        }
    } else {
        app.changeState(new AppMenuState());
    }
    
    delay(5); // Small yield
}

void AppTerminalState::onRefresh(App& app) {
    app.drawTerminalScreen();
}

void AppTerminalState::exit(App& app) {
    // Cleanup connection if still active?
    // Usually we keep connection unless explicitly closed.
}
