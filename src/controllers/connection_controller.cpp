#include "controllers/connection_controller.h"
#include "ui/menu_system.h"
#include "server_manager.h"

ConnectionController::ConnectionController(ISystemContext& system) : _system(system) {
}

void ConnectionController::showSavedServers() {
    std::vector<String> items;
    auto servers = _system.serverManager().getServers();
    for (const auto& s : servers) {
        items.push_back(s.name);
    }
    items.push_back("[ Add New Server ]");

    _system.menu()->showMenu("Saved Servers", items, [this, servers](int choice) {
        if (choice >= (int)servers.size()) {
            // Add New Server Wizard
            auto newSrv = std::make_shared<ServerConfig>();
            newSrv->port = 22;
            
            _system.menu()->showInput("Name", "", false, [this, newSrv](String val) {
                newSrv->name = val;
                _system.menu()->showInput("Host", "", false, [this, newSrv](String val) {
                    newSrv->host = val;
                    _system.menu()->showInput("User", "", false, [this, newSrv](String val) {
                        newSrv->user = val;
                        _system.menu()->showInput("Port", "22", false, [this, newSrv](String val) {
                            newSrv->port = val.toInt();
                            _system.menu()->showInput("Password", "", true, [this, newSrv](String val) {
                                newSrv->password = val;
                                _system.serverManager().addServer(*newSrv);
                                showSavedServers();
                            }, [this](){ showSavedServers(); });
                        }, [this](){ showSavedServers(); });
                    }, [this](){ showSavedServers(); });
                }, [this](){ showSavedServers(); });
            }, [this](){ showSavedServers(); });
            
        } else {
            // Selected existing server
            ServerConfig selected = servers[choice];
            std::vector<String> actions = {"Connect", "Edit", "Delete"};
            
            // Nested Menu for Actions
            _system.menu()->showMenu(selected.name, actions, [this, choice, selected](int action) {
                if (action == 0) { // Connect
                    _system.connectToServer(selected.host, selected.port, selected.user, selected.password, selected.name);
                } else if (action == 1) { // Edit
                    auto srv = std::make_shared<ServerConfig>(selected);
                    _system.menu()->showInput("Name", srv->name, false, [this, choice, srv](String val) {
                        srv->name = val;
                        _system.menu()->showInput("Host", srv->host, false, [this, choice, srv](String val) {
                            srv->host = val;
                            _system.menu()->showInput("User", srv->user, false, [this, choice, srv](String val) {
                                srv->user = val;
                                _system.menu()->showInput("Port", String(srv->port), false, [this, choice, srv](String val) {
                                    srv->port = val.toInt();
                                    _system.menu()->showInput("Password", srv->password, true, [this, choice, srv](String val) {
                                        srv->password = val;
                                        _system.serverManager().updateServer(choice, *srv);
                                        showSavedServers();
                                    }, [this](){ showSavedServers(); });
                                }, [this](){ showSavedServers(); });
                            }, [this](){ showSavedServers(); });
                        }, [this](){ showSavedServers(); });
                    }, [this](){ showSavedServers(); });
                } else if (action == 2) { // Delete
                     std::vector<String> yn = {"No", "Yes"};
                     _system.menu()->showMenu("Delete?", yn, [this, choice](int ynChoice) {
                        if (ynChoice == 1) {
                            _system.serverManager().removeServer(choice);
                        }
                        showSavedServers();
                     }, [this](){ showSavedServers(); });
                }
            }, [this](){ showSavedServers(); });
        }
    }, [this]() {
        _system.handleMainMenu();
    });
}

void ConnectionController::showQuickConnect() {
     auto s = std::make_shared<ServerConfig>();
     s->name = "Quick Connect";
     s->port = 22;
     
     _system.menu()->showInput("Host / IP", s->host, false, [this, s](String val) {
         s->host = val;
         _system.menu()->showInput("Port", "22", false, [this, s](String val) {
             s->port = val.toInt();
             _system.menu()->showInput("User", s->user, false, [this, s](String val) {
                 s->user = val;
                 _system.menu()->showInput("Password", s->password, true, [this, s](String val) {
                     s->password = val;
                     _system.connectToServer(s->host, s->port, s->user, s->password, s->name);
                 }, [this](){ _system.handleMainMenu(); });
             }, [this](){ _system.handleMainMenu(); });
         }, [this](){ _system.handleMainMenu(); });
     }, [this](){ _system.handleMainMenu(); });
}
