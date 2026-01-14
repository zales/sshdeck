#pragma once
#include "app_state.h"

class AppTerminalState : public AppState {
public:
    void enter(App& app) override;
    void update(App& app) override;
    void onRefresh(App& app) override;
    void exit(App& app) override;
};
