#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <libssh/libssh.h>
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
    
    bool connectWiFi();
    bool connectSSH();
    void disconnect();
    
    bool isConnected() const { return ssh_connected; }
    void process();  // Call in loop to handle SSH I/O
    
private:
    TerminalEmulator& terminal;
    KeyboardManager& keyboard;
    
    ssh_session session;
    ssh_channel channel;
    bool ssh_connected;
    
    unsigned long last_update;
    
    void processSSHOutput();
    void processKeyboardInput();
    void handleSYMCombinations(char base_c);
};
