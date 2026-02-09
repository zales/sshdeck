#include "ssh_client.h"
#include <driver/rtc_io.h>
#include "keymap.h"
#include "board_def.h"

SSHClient::SSHClient(TerminalEmulator& term, KeyboardManager& kb) 
    : terminal(term), keyboard(kb), session(nullptr), channel(nullptr), 
      _state(DISCONNECTED), last_update(0), _pendingCancel(false) {
    _sessionMutex = xSemaphoreCreateMutex();
}

void SSHClient::setRefreshCallback(std::function<void()> callback) {
    onRefresh = callback;
}

void SSHClient::setHelpCallback(std::function<void()> callback) {
    onHelp = callback;
}

SSHClient::~SSHClient() {
    disconnect();
    // Wait for the connection task to gracefully exit to avoid Use-After-Free
    while (_taskHandle != NULL) {
        // vTaskDelay is FreeRTOS friendly delay
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void SSHClient::connectSSH(const char* host, int port, const char* user, const char* password, const char* keyData) {
    if (_state == CONNECTING) return;
    
    // Ensure we don't start if a task is somehow still running (should be handled by cancel check)
    if (_taskHandle != NULL) {
        terminal.appendString("Busy cleanup...\n");
        return;
    }

    terminal.appendString("Connecting SSH to ");
    terminal.appendString(host);
    terminal.appendString("...\n");
    if (onRefresh) onRefresh();
    
    _connHost = host;
    _connPort = port;
    _connUser = user;
    _connPass = password;
    if (keyData) _connKey = keyData; else _connKey = "";

    _state = CONNECTING;
    _pendingCancel = false;
    _lastError = "";

    xTaskCreate(connectTask, "ssh_conn", 16384, this, 1, &_taskHandle);
}

void SSHClient::connectTask(void* param) {
    SSHClient* self = (SSHClient*)param;
    
    // Initialize SSH session
    ssh_session session = ssh_new();
    if (session == nullptr) {
        self->terminal.appendString("SSH init failed!\n");
        self->_state = FAILED;
        self->_lastError = "SSH Init Failed";
        self->_taskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    if (self->_pendingCancel) {
        ssh_free(session);
        self->_taskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }
    
    ssh_options_set(session, SSH_OPTIONS_HOST, self->_connHost.c_str());
    ssh_options_set(session, SSH_OPTIONS_PORT, &self->_connPort);
    ssh_options_set(session, SSH_OPTIONS_USER, self->_connUser.c_str());
    
    // Disable logging for production performance
    int verbosity = SSH_LOG_NOLOG;
    ssh_options_set(session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
    
    // Set 10s connection timeout
    long timeout = 10; 
    ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &timeout);
    
    // Connect
    if (ssh_connect(session) != SSH_OK) {
        self->terminal.appendString("SSH connect failed!\n");
        ssh_free(session);
        self->_state = FAILED;
        self->_lastError = "Connect Failed";
        self->_taskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    if (self->_pendingCancel) {
        ssh_disconnect(session);
        ssh_free(session);
        self->_taskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }
    
    // Authenticate
    int rc = SSH_AUTH_DENIED;
    
    const char* keyData = self->_connKey.length() > 0 ? self->_connKey.c_str() : nullptr;
    
    // Check provided key first, then macro fallback if needed
    bool hasKey = (keyData != nullptr && strlen(keyData) > 10);
    bool useMacroKey = (!hasKey && SSH_USE_KEY && strlen(SSH_KEY_DATA) > 10);

    if (hasKey || useMacroKey) {
        self->terminal.appendString("Using Key Auth...\n");
        ssh_key privkey;
        
        const char* keyToUse = hasKey ? keyData : SSH_KEY_DATA;
        
        int importRc = ssh_pki_import_privkey_base64(keyToUse, nullptr, nullptr, nullptr, &privkey);
        if (importRc == SSH_OK) {
            self->terminal.appendString("Key Import OK. Auth...\n");
            rc = ssh_userauth_publickey(session, nullptr, privkey);
            ssh_key_free(privkey);
            
            if (rc != SSH_AUTH_SUCCESS) {
                self->terminal.appendString("Pubkey Auth Failed: ");
                self->terminal.appendString(ssh_get_error(session));
                self->terminal.appendString("\nCheck server authorized_keys\n");
            }
        } else {
            self->terminal.appendString("Key Import Failed!\n");
        }
    }

    // Fallback to password
    if (rc != SSH_AUTH_SUCCESS) {
        if (SSH_USE_KEY) {
             self->terminal.appendString("Key auth failed, trying password...\n");
        }
        rc = ssh_userauth_password(session, nullptr, self->_connPass.c_str());
    }

    if (self->_pendingCancel) {
        ssh_disconnect(session);
        ssh_free(session);
        self->_taskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }
    
    if (rc != SSH_AUTH_SUCCESS) {
        self->terminal.appendString("SSH auth failed!\n");
        ssh_disconnect(session);
        ssh_free(session);
        self->_state = FAILED;
        self->_lastError = "Auth Failed";
        self->_taskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }
    
    // Open channel
    ssh_channel channel = ssh_channel_new(session);
    if (channel == nullptr) {
        self->terminal.appendString("SSH channel failed!\n");
        ssh_disconnect(session);
        ssh_free(session);
        self->_state = FAILED;
        self->_lastError = "Channel Failed";
        self->_taskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }
    
    if (ssh_channel_open_session(channel) != SSH_OK) {
        self->terminal.appendString("Channel open failed!\n");
        ssh_channel_free(channel);
        ssh_disconnect(session);
        ssh_free(session);
        self->_state = FAILED;
         self->_lastError = "Channel Open Failed";
        self->_taskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }
    
    // Request PTY
    if (ssh_channel_request_pty_size(channel, TERM_TYPE, TERM_COLS, TERM_ROWS) != SSH_OK) {
        self->terminal.appendString("PTY request failed!\n");
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        ssh_disconnect(session);
        ssh_free(session);
        self->_state = FAILED;
         self->_lastError = "PTY Failed";
        self->_taskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }
    
    // Start shell
    if (ssh_channel_request_shell(channel) != SSH_OK) {
        self->terminal.appendString("Shell request failed!\n");
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        ssh_disconnect(session);
        ssh_free(session);
        self->_state = FAILED;
         self->_lastError = "Shell Failed";
        self->_taskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }

    if (self->_pendingCancel) {
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        ssh_disconnect(session);
        ssh_free(session);
        self->_taskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }
    
    self->terminal.appendString("SSH Connected!\n\n");
    
    xSemaphoreTake(self->_sessionMutex, portMAX_DELAY);
    if (self->_pendingCancel) {
        // Race: disconnect was requested while we were finalizing
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        ssh_disconnect(session);
        ssh_free(session);
        xSemaphoreGive(self->_sessionMutex);
        self->_taskHandle = NULL;
        vTaskDelete(NULL);
        return;
    }
    self->session = session;
    self->channel = channel;
    self->connectedHost = self->_connHost;
    self->last_update = millis();
    self->_state = CONNECTED;
    xSemaphoreGive(self->_sessionMutex);
    
    if (self->onRefresh) self->onRefresh();
    
    self->_taskHandle = NULL;
    vTaskDelete(NULL);
}

void SSHClient::disconnect() {
    _pendingCancel = true;

    // Do not kill task aggressively to prevent leaks
    // The task will exit when it sees _pendingCancel
    
    xSemaphoreTake(_sessionMutex, portMAX_DELAY);
    _state = DISCONNECTED;
    
    if (channel) {
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        channel = nullptr;
    }
    
    if (session) {
        ssh_disconnect(session);
        ssh_free(session);
        session = nullptr;
    }
    xSemaphoreGive(_sessionMutex);
}

void SSHClient::process() {
    if (_state != CONNECTED) return;
    
    // Process Startup Command if any
    if (_startupCommand.length() > 0) {
        String scriptName = ""; 
        // We don't have the script name here, only the command. 
        // Just indicate we are running something.
        terminal.appendString("\n[AutoRun] Executing script...\n");
        
        if (!_startupCommand.endsWith("\n")) {
             _startupCommand += "\n";
        }
        
        if (xSemaphoreTake(_sessionMutex, 100)) {
            ssh_channel_write(channel, _startupCommand.c_str(), _startupCommand.length());
            xSemaphoreGive(_sessionMutex);
            _startupCommand = ""; // Executed
        }
    }

    // Reset shortcut flag if Mic released
    if (!keyboard.isMicActive()) {
        mic_shortcut_used = false;
    }

    processSSHOutput();

    // Check for Long Press Mic (Help)
    // Only if no other key was pressed during this hold (mic_shortcut_used == false)
    if (keyboard.isMicActive() && !mic_shortcut_used) {
         unsigned long pressTime = keyboard.getMicPressTime();
         if (pressTime != last_mic_press_handled && (millis() - pressTime > 800)) {
             last_mic_press_handled = pressTime;
             if (onHelp) onHelp();
         }
    }
}

void SSHClient::write(char c) {
    if (_state != CONNECTED) return;

    if (c == 0) return; 
    
    // shortcuts via Mic (which is Ctrl)
    if (keyboard.isMicActive()) {
            mic_shortcut_used = true;
            
            // Since Mic is Ctrl, 'c' is already a Control character (1-26) if it was a-z
            // We need to check what the physical key was, or infer from the control char.
            
            // Restore base char for mapping check
            char base = 0;
            if (c >= 1 && c <= 26) {
                base = c + 'a' - 1;
            }
            
            if (base == 'h') {
                if(onHelp) onHelp();
                return;
            }
            
            if (base != 0 && handleMicShortcuts(base)) {
                return;
            }
            // If not handled as shortcut, fall through to send the Ctrl character (e.g. Ctrl-C)
    }
    
    // shortcuts via Alt (F1-F9)
    if (keyboard.isAltActive()) {
        if (handleAltShortcuts(c)) return;
    }
    
    // Regular character
    ssh_channel_write(channel, &c, 1);
}

bool SSHClient::handleAltShortcuts(char c) {
    // Map base characters (where numbers are printed) to F-keys
    // F1=\033OP, F2=\033OQ, F3=\033OR, F4=\033OS
    // F5=\033[15~, F6=\033[17~, F7=\033[18~, F8=\033[19~, F9=\033[20~
    
    switch (c) {
        case 'w': // 1 -> F1
            ssh_channel_write(channel, "\x1BOP", 3);
            return true;
        case 'e': // 2 -> F2
            ssh_channel_write(channel, "\x1BOQ", 3);
            return true;
        case 'r': // 3 -> F3
            ssh_channel_write(channel, "\x1BOR", 3);
            return true;
        case 's': // 4 -> F4
            ssh_channel_write(channel, "\x1BOS", 3);
            return true;
        case 'd': // 5 -> F5
            ssh_channel_write(channel, "\x1B[15~", 5);
            return true;
        case 'f': // 6 -> F6
            ssh_channel_write(channel, "\x1B[17~", 5);
            return true;
        case 'z': // 7 -> F7
            ssh_channel_write(channel, "\x1B[18~", 5);
            return true;
        case 'x': // 8 -> F8
            ssh_channel_write(channel, "\x1B[19~", 5);
            return true;
        case 'c': // 9 -> F9
            ssh_channel_write(channel, "\x1B[20~", 5);
            return true;
    }
    return false;
}

bool SSHClient::handleMicShortcuts(char base_c) {
    bool appMode = terminal.isAppCursorMode();
    
    switch (base_c) {
        case 'w':  // Up arrow
            if(appMode) ssh_channel_write(channel, "\x1BOA", 3);
            else        ssh_channel_write(channel, "\x1B[A", 3);
            return true;
        case 'a':  // Left arrow
            if(appMode) ssh_channel_write(channel, "\x1BOD", 3);
            else        ssh_channel_write(channel, "\x1B[D", 3);
            return true;
        case 's':  // Down arrow
            if(appMode) ssh_channel_write(channel, "\x1BOB", 3);
            else        ssh_channel_write(channel, "\x1B[B", 3);
            return true;
        case 'd':  // Right arrow
            if(appMode) ssh_channel_write(channel, "\x1BOC", 3);
            else        ssh_channel_write(channel, "\x1B[C", 3);
            return true;
        case 'q': {  // ESC
            char esc = '\x1B';
            ssh_channel_write(channel, &esc, 1);
            return true;
        }
        case 'e': {  // TAB
            char tab = '\t';
            ssh_channel_write(channel, &tab, 1);
            return true;
        }
    }
    return false;
}

void SSHClient::processSSHOutput() {
    char buffer[1024];
    int total_read = 0;
    
    if (_state != CONNECTED || channel == nullptr) return;
    
    // Read until buffer is empty or we processed a safety limit
    while (total_read < 8192) { // Safety limit 8KB per loop
        int nbytes = ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer) - 1, 0);
        
        if (nbytes < 0) {
             // Error occurred
             disconnect();
             return;
        }
        
        if (nbytes > 0) {
            buffer[nbytes] = '\0';
            terminal.appendString(buffer);
            total_read += nbytes;
            last_update = millis();
        } else {
            // nbytes == 0, check for EOF
            if (ssh_channel_is_eof(channel)) {
                disconnect();
                return;
            }
            break; 
        }
    }
    
    if (ssh_channel_is_closed(channel)) {
        disconnect();
    }
}
