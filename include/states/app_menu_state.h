#pragma once
#include "app_state.h"

class AppMenuState : public AppState {
public:
    void enter(App& app) override;
    void update(App& app) override;
    void exit(App& app) override;
};
