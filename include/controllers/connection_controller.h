#pragma once
#include <Arduino.h>
#include "system_context.h"

class ConnectionController {
public:
    explicit ConnectionController(ISystemContext& system);

    void showSavedServers();
    void showQuickConnect();

private:
    ISystemContext& _system;
};
