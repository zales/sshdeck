#pragma once
#include <Arduino.h>
#include "system_context.h"

class SettingsController {
public:
    explicit SettingsController(ISystemContext& system);
    
    void showSettingsMenu();
    void showBatteryInfo();
    void handleChangePin();
    void handleStorage();
    void handleSystemUpdate();
    void handleWifiMenu();
    void handleWifiForUpdate();
    void exitStorageMode();

private:
    ISystemContext& _system;
};
