#pragma once

#include <Arduino.h>
#include <vector>
#include <Preferences.h>
#include "security_manager.h"

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
    void setSecurityManager(SecurityManager* sec);
    
    std::vector<ServerConfig> getServers() const;
    ServerConfig getServer(int index) const;
    void addServer(const ServerConfig& config);
    void updateServer(int index, const ServerConfig& config);
    void removeServer(int index);
    int getCount() const;
    
    void load();
    void save();
    // Helper to trigger re-encryption
    void reEncryptAll() { save(); }

private:
    std::vector<ServerConfig> servers;
    Preferences prefs;
    SecurityManager* security;
    
    void saveServerToPrefs(int index, const ServerConfig& config);
};
