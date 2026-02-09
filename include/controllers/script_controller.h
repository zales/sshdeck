#pragma once
#include <Arduino.h>
#include "system_context.h"
#include <vector>

class ScriptController {
public:
    explicit ScriptController(ISystemContext& system);

    void showScriptMenu();

private:
    ISystemContext& _system;

    void createNewScript();
    void showScriptOptions(int index);
    void editScript(int index);
    void deleteScript(int index);
    void runScript(int index);
    void selectServerForScript(int scriptIndex);
};
