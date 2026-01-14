#pragma once

#include <Arduino.h>
#include <memory>

// Forward declarations of managers
class DisplayManager;
class KeyboardManager;
class UIManager;
class TerminalEmulator;
class PowerManager;
class ServerManager;
class StorageManager;
class WifiManager;
class MenuSystem;
class SSHClient;
class SecurityManager;
class OtaManager;

class ISystemContext {
public:
    virtual ~ISystemContext() = default;

    // Subsystem Getters
    virtual DisplayManager& display() = 0;
    virtual KeyboardManager& keyboard() = 0;
    virtual UIManager& ui() = 0;
    virtual TerminalEmulator& terminal() = 0;
    virtual PowerManager& power() = 0;
    virtual ServerManager& serverManager() = 0;
    virtual StorageManager& storage() = 0;
    virtual WifiManager& wifi() = 0;
    virtual MenuSystem* menu() = 0;
    virtual SSHClient* sshClient() = 0;
    virtual SecurityManager& security() = 0;
    virtual OtaManager& ota() = 0;

    // Logic Handlers required by controllers
    virtual void handleMainMenu() = 0;
    virtual void connectToServer(const String& host, int port, const String& user, const String& pass, const String& name) = 0;
};
