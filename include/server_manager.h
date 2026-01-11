#pragma once

#include <Arduino.h>
#include <vector>
#include <Preferences.h>

struct ServerConfig {
    String name;
    String host;
    int port;
    String user;
    String password;
};

class ServerManager {
public:
    ServerManager();
    void begin();
    
    std::vector<ServerConfig> getServers() const;
    ServerConfig getServer(int index) const;
    void addServer(const ServerConfig& config);
    void updateServer(int index, const ServerConfig& config);
    void removeServer(int index);
    int getCount() const;
    
    void load();
    void save();

private:
    std::vector<ServerConfig> servers;
    Preferences prefs;
    
    void saveServerToPrefs(int index, const ServerConfig& config);
};
