#include "server_manager.h"

ServerManager::ServerManager() : security(nullptr) {
}

void ServerManager::setSecurityManager(SecurityManager* sec) {
    security = sec;
}

void ServerManager::begin() {
    prefs.begin("ssh_servers", false);
    load();
}

void ServerManager::load() {
    servers.clear();
    int count = prefs.getInt("count", 0);
    
    for (int i = 0; i < count; i++) {
        ServerConfig s;
        String prefix = "s" + String(i) + "_";
        
        s.name = prefs.getString((prefix + "name").c_str(), "Server " + String(i+1));
        s.host = prefs.getString((prefix + "host").c_str(), "");
        s.port = prefs.getInt((prefix + "port").c_str(), 22);
        s.user = prefs.getString((prefix + "user").c_str(), "root");
        
        String rawPass = prefs.getString((prefix + "pass").c_str(), "");
        if (security && !rawPass.isEmpty()) {
            String decrypted = security->decrypt(rawPass);
            // If decryption fails (returns empty) but raw wasn't empty, assume legacy plaintext
            if (decrypted.isEmpty() && !rawPass.isEmpty()) {
                s.password = rawPass;
            } else {
                s.password = decrypted;
            }
        } else {
            s.password = rawPass;
        }
        
        if (s.host.length() > 0) {
            servers.push_back(s);
        }
    }
}

void ServerManager::save() {
    prefs.putInt("count", servers.size());
    
    for (int i = 0; i < servers.size(); i++) {
        saveServerToPrefs(i, servers[i]);
    }
}

void ServerManager::saveServerToPrefs(int index, const ServerConfig& config) {
    String prefix = "s" + String(index) + "_";
    prefs.putString((prefix + "name").c_str(), config.name);
    prefs.putString((prefix + "host").c_str(), config.host);
    prefs.putInt((prefix + "port").c_str(), config.port);
    prefs.putString((prefix + "user").c_str(), config.user);
    
    if (security) {
        prefs.putString((prefix + "pass").c_str(), security->encrypt(config.password));
    } else {
        prefs.putString((prefix + "pass").c_str(), config.password);
    }
}

void ServerManager::addServer(const ServerConfig& config) {
    servers.push_back(config);
    save(); // Simple auto-save implementation
}

void ServerManager::updateServer(int index, const ServerConfig& config) {
    if (index >= 0 && index < servers.size()) {
        servers[index] = config;
        save();
    }
}

void ServerManager::removeServer(int index) {
    if (index >= 0 && index < servers.size()) {
        servers.erase(servers.begin() + index);
        // We need to clear old preferences to avoid artifacts if we shrink the list
        // Nuke it and resave everything is easiest for small lists
        prefs.clear();
        save();
    }
}

std::vector<ServerConfig> ServerManager::getServers() const {
    return servers;
}

ServerConfig ServerManager::getServer(int index) const {
    if (index >= 0 && index < servers.size()) {
        return servers[index];
    }
    return ServerConfig();
}

int ServerManager::getCount() const {
    return servers.size();
}
