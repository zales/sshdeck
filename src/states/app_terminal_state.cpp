#include "states/app_terminal_state.h"
#include "states/app_menu_state.h"
#include "touch_manager.h"
#include "app.h"

void AppTerminalState::enter(App& app) {
    // Perform a Full Refresh Clear to remove ghosting from previous states
    app.display().lock();
    app.display().fullClean();
    app.display().unlock();
    
    // Nothing special on enter, connection is handled by App::connectToServer for now
    app.drawTerminalScreen(false); // Full refresh on enter
}

void AppTerminalState::update(App& app) {
    // 1. Process ALL pending inputs at once to speed up typing
    // We check for keyboard availability directly to drain the buffer
    unsigned long startTime = millis();
    while(app.keyboard().available() && (millis() - startTime < 10)) {
        InputEvent event = app.pollInputs();

        if (event.type == EVENT_NONE) break;

        if (event.type == EVENT_SYSTEM && event.systemCode == SYS_EVENT_SLEEP) {
            app.enterDeepSleep();
            return;
        }

        // Handle async state checking
        if (app.sshClient()) {
            if (app.sshClient()->getState() == SSHClient::CONNECTED) {
                // Any keypress while viewing history → return to live
                if (app.terminal().isViewingHistory()) {
                    app.terminal().scrollViewReset();
                }
                if (event.type == EVENT_KEY_PRESS && event.key != 0) {
                    app.sshClient()->write(event.key);
                }
            }
        }
    }

    // Pass one phantom event if loop needs it, or just rely on the fact pollInputs was called
    // actually, pollInputs handles SYS events too, so we should be careful.
    // Ideally we should process the *result* of pollInputs.
    // But pollInputs calls keyboard.loop().
    // Let's stick to the while loop above for fast typing, assuming polls are cheap.

    // 2. Process touch gestures for scrollback
    if (app.touch().available()) {
        TouchEvent te = app.touch().read();
        bool scrollChanged = false;
        
        // Calculate scroll amount based on swipe length (kinetic feel)
        // Base 5 lines, +1 line for every ~8px of extra swipe distance
        int lines = 5;
        if (te.magnitude > 20) {
            lines += (te.magnitude - 20) / 8;
        }
        if (lines > 30) lines = 30; // Limit to ~1 page per swipe
        
        if (te.gesture == GESTURE_SWIPE_DOWN) {
            // Swipe down on screen = scroll back into history
            app.terminal().lock();
            app.terminal().scrollViewUp(lines);
            app.terminal().unlock();
            scrollChanged = true;
        } else if (te.gesture == GESTURE_SWIPE_UP) {
            // Swipe up on screen = scroll forward toward live
            app.terminal().lock();
            app.terminal().scrollViewDown(lines);
            app.terminal().unlock();
            scrollChanged = true;
        } else if (te.gesture == GESTURE_SINGLE_TAP) {
            // Tap = return to live view
            app.terminal().lock();
            app.terminal().scrollViewReset();
            app.terminal().unlock();
            scrollChanged = true;
        }
        // Force immediate redraw after scroll gesture
        if (scrollChanged) {
            _lastDisplayUpdate = millis();
            app.drawTerminalScreen();
        }
    }

    if (app.sshClient()) {
         auto state = app.sshClient()->getState();
         
         if (state == SSHClient::CONNECTING) {
             // Throttle redraws during connection — partial refresh takes ~700ms,
             // so refreshing every 50ms is wasteful. Use 1s interval.
             if (millis() - _lastDisplayUpdate >= 1000) {
                 _lastDisplayUpdate = millis();
                 app.drawTerminalScreen();
             }
             delay(50);
             return;
         }
         
         if (state == SSHClient::FAILED) {
             app.ui().drawMessage("Error", app.sshClient()->getLastError());
             delay(2000);
             app.changeState(new AppMenuState());
             return;
         }
         
         if (state == SSHClient::CONNECTED) {
            // 2. Read available SSH data (bulk read)
            app.sshClient()->process();
            
            // Don't auto-reset scrollback — let user browse history
            // New data will be buffered and visible when user returns to live view
            
            bool forceRedraw = false;
            // Battery Charging Animation (every 1 sec)
            if (app.power().isCharging() && (millis() - _lastAniUpdate > 1000)) {
                _lastAniUpdate = millis();
                forceRedraw = true;
            }

            if ((app.terminal().needsUpdate() && !app.terminal().isViewingHistory()) || forceRedraw) {
                 // Debounce: don't refresh faster than DISPLAY_UPDATE_INTERVAL_MS
                 // E-ink partial refresh takes ~700ms, rapid redraws are wasteful
                 if (millis() - _lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL_MS) {
                     _lastDisplayUpdate = millis();
                     _partialRefreshCount++;
                     
                     // Periodic full refresh to combat ghosting accumulation
                     // Every 50 partial updates, force a full refresh cycle
                     if (_partialRefreshCount >= 50) {
                         _partialRefreshCount = 0;
                         app.drawTerminalScreen(false); // full refresh
                     } else {
                         app.drawTerminalScreen();
                     }
                 }
            }

            if (!app.sshClient()->isConnected()) {
                app.display().lock();
                app.ui().drawMessage("Disconnected", "Session Ended");
                app.display().unlock();
                
                delay(1000); // Give user time to see
                app.changeState(new AppMenuState());
            }
         } else {
            // DISCONNECTED
            app.changeState(new AppMenuState());
         }
    } else {
        app.changeState(new AppMenuState());
    }
    
    delay(10); // Yield to FreeRTOS scheduler, saves CPU power in idle
}

void AppTerminalState::onRefresh(App& app) {
    app.drawTerminalScreen();
}

void AppTerminalState::exit(App& app) {
    // Cleanup connection if still active?
    // Usually we keep connection unless explicitly closed.
}
