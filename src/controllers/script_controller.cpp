#include "controllers/script_controller.h"
#include "ui/menu_system.h"
#include "storage_manager.h"
#include "server_manager.h"
#include "script_def.h"

ScriptController::ScriptController(ISystemContext& system) : _system(system) {}

void ScriptController::showScriptMenu() {
    auto scripts = _system.storage().getScripts();
    std::vector<String> items;
    for (const auto& s : scripts) {
        items.push_back(s.name);
    }
    items.push_back("+ Add New Script");

    _system.menu()->showMenu("Scripts", items, [this, scripts](int choice) {
        if (choice < scripts.size()) {
            showScriptOptions(choice);
        } else if (choice == scripts.size()) {
            // Add New
            createNewScript();
        }
    }, [this]() {
        _system.handleMainMenu();
    });
}

void ScriptController::showScriptOptions(int index) {
    auto scripts = _system.storage().getScripts();
    if (index >= scripts.size()) return;
    
    Script s = scripts[index];
    std::vector<String> actions = {"Run", "Edit", "Delete"};
    
    _system.menu()->showMenu(s.name, actions, [this, index, s](int choice) {
        if (choice == 0) {
            runScript(index);
        } else if (choice == 1) {
            editScript(index);
        } else if (choice == 2) {
            deleteScript(index);
        }
    }, [this]() {
        showScriptMenu();
    });
}

void ScriptController::runScript(int index) {
    selectServerForScript(index);
}

void ScriptController::selectServerForScript(int scriptIndex) {
    auto servers = _system.serverManager().getServers();
    std::vector<String> items;
    for (const auto& sv : servers) {
        items.push_back(sv.name);
    }
    items.push_back("+ Connect New"); // Add option to connect to a new server on the fly

    _system.menu()->showMenu("Select Server", items, [this, servers, scriptIndex](int choice) {
        if (choice >= 0 && choice < servers.size()) {
            auto server = servers[choice];
            auto scripts = _system.storage().getScripts();
            if (scriptIndex < scripts.size()) {
                 String cmd = scripts[scriptIndex].command;
                 // Connect and run
                 _system.connectToServer(server.host, server.port, server.user, server.password, server.name, cmd);
            }
        } else if (choice == servers.size()) {
            // "Connect New" selected - Reuse Quick Connect logic roughly but with script passing
             auto s = std::make_shared<ServerConfig>();
             s->name = "Quick Connect";
             s->port = 22;
             
             _system.menu()->showInput("Host", "", false, [this, s, scriptIndex](String val) {
                 s->host = val;
                 _system.menu()->showInput("Port", "22", false, [this, s, scriptIndex](String val) {
                     s->port = val.toInt();
                     _system.menu()->showInput("User", "", false, [this, s, scriptIndex](String val) {
                         s->user = val;
                         _system.menu()->showInput("Password", "", true, [this, s, scriptIndex](String val) {
                             s->password = val;
                             
                             auto scripts = _system.storage().getScripts();
                             String cmd = "";
                             if (scriptIndex < scripts.size()) cmd = scripts[scriptIndex].command;

                             // Connect and run
                             _system.connectToServer(s->host, s->port, s->user, s->password, s->name, cmd);
                             
                         }, [this, scriptIndex](){ selectServerForScript(scriptIndex); });
                     }, [this, scriptIndex](){ selectServerForScript(scriptIndex); });
                 }, [this, scriptIndex](){ selectServerForScript(scriptIndex); });
             }, [this, scriptIndex](){ selectServerForScript(scriptIndex); });
        }
    }, [this, scriptIndex]() {
        showScriptOptions(scriptIndex);
    });
}


void ScriptController::createNewScript() {
    _system.menu()->showInput("Script Name", "", false, [this](String name) {
        if (name.length() == 0) {
            showScriptMenu();
            return;
        }
        
        _system.menu()->showInput("Command", "", false, [this, name](String cmd) {
             Script s;
             s.name = name;
             s.command = cmd;
             _system.storage().addScript(s);
             showScriptMenu();
        }, [this](){
            showScriptMenu();
        });
        
    }, [this]() {
        showScriptMenu();
    });
}

void ScriptController::editScript(int index) {
    auto scripts = _system.storage().getScripts();
    if (index >= scripts.size()) return;
    Script s = scripts[index];

    _system.menu()->showInput("Edit Name", s.name, false, [this, index, s](String name) {
         _system.menu()->showInput("Edit Command", s.command, false, [this, index, name](String cmd) {
             Script newS;
             newS.name = name;
             newS.command = cmd;
             _system.storage().updateScript(index, newS);
             showScriptMenu();
         }, [this, index](){
             showScriptOptions(index);
         });
    }, [this, index]() {
        showScriptOptions(index);
    });
}

void ScriptController::deleteScript(int index) {
    // Confirmation? For now just delete
    _system.storage().deleteScript(index);
    showScriptMenu();
}
