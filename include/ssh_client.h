#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <libssh/libssh.h>
#include <functional>
#include "config.h"
#include "terminal_emulator.h"
#include "keyboard_manager.h"

/**
 * @brief SSH Client Manager
 * Handles SSH connection, authentication, and I/O
 */
class SSHClient {
public:
    SSHClient(TerminalEmulator& terminal, KeyboardManager& keyboard);
    ~SSHClient();
    
    void setRefreshCallback(std::function<void()> callback);
    void setHelpCallback(std::function<void()> callback);

    bool connectSSH(const char* host, int port, const char* user, const char* password);
    void disconnect();
    
    bool isConnected() const { return ssh_connected; }
    const String& getConnectedHost() const { return connectedHost; }
    void process();  // Call in loop to handle SSH I/O
    
private:
    TerminalEmulator& terminal;
    KeyboardManager& keyboard;
    std::function<void()> onRefresh;
    std::function<void()> onHelp;
    
    ssh_session session;
    ssh_channel channel;
    bool ssh_connected;
    String connectedHost;
    
    unsigned long last_update;
    unsigned long last_mic_press_handled = 0;
    bool mic_shortcut_used = false;

    void processSSHOutput();
    void processKeyboardInput();
    bool handleMicShortcuts(char base_c);
    bool handleAltShortcuts(char c);
};
