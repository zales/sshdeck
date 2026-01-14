#pragma once
#include "app_state.h"
#include <Arduino.h>

class AppLockedState : public AppState {
public:
    AppLockedState();
    
    void enter(App& app) override;
    void update(App& app) override;
    void onRefresh(App& app) override;
    void exit(App& app) override;

private:
    String _pin;
    void redraw(App& app, bool error = false);
};
