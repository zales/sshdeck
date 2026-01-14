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

    enum State { DISCONNECTED, CONNECTING, CONNECTED, FAILED };

    void connectSSH(const char* host, int port, const char* user, const char* password, const char* keyData = nullptr);
    void disconnect();
    
    bool isConnected() const { return _state == CONNECTED; }
    State getState() const { return _state; }
    String getLastError() const { return _lastError; }

    const String& getConnectedHost() const { return connectedHost; }
    void process();  // Call in loop to handle SSH I/O
    void write(char c); // Inject key
    
private:
    TerminalEmulator& terminal;
    KeyboardManager& keyboard;
    std::function<void()> onRefresh;
    std::function<void()> onHelp;
    
    ssh_session session;
    ssh_channel channel;
    
    State _state = DISCONNECTED;
    String _lastError;
    String connectedHost;

    // Connection Task Params
    String _connHost;
    int _connPort;
    String _connUser;
    String _connPass;
    String _connKey;
    TaskHandle_t _taskHandle = NULL;
    static void connectTask(void* param);
    
    unsigned long last_update;
    volatile bool _pendingCancel = false;
    unsigned long last_mic_press_handled = 0;
    bool mic_shortcut_used = false;

    void processSSHOutput();
    void processKeyboardInput();
    bool handleMicShortcuts(char base_c);
    bool handleAltShortcuts(char c);
};
