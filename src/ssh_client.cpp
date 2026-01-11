#include "ssh_client.h"
#include <driver/rtc_io.h>
#include "keymap.h"
#include "board_def.h"

SSHClient::SSHClient(TerminalEmulator& term, KeyboardManager& kb) 
    : terminal(term), keyboard(kb), session(nullptr), channel(nullptr), 
      ssh_connected(false), last_update(0) {
}

void SSHClient::setRefreshCallback(std::function<void()> callback) {
    onRefresh = callback;
}

void SSHClient::setHelpCallback(std::function<void()> callback) {
    onHelp = callback;
}

SSHClient::~SSHClient() {
    disconnect();
}

bool SSHClient::connectSSH(const char* host, int port, const char* user, const char* password) {
    terminal.appendString("Connecting SSH to ");
    terminal.appendString(host);
    terminal.appendString("...\n");
    if (onRefresh) onRefresh();
    
    // Initialize SSH session
    session = ssh_new();
    if (session == nullptr) {
        terminal.appendString("SSH init failed!\n");
        return false;
    }
    
    ssh_options_set(session, SSH_OPTIONS_HOST, host);
    ssh_options_set(session, SSH_OPTIONS_PORT, &port);
    ssh_options_set(session, SSH_OPTIONS_USER, user);
    
    // Disable logging for production performance
    int verbosity = SSH_LOG_NOLOG;
    ssh_options_set(session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
    
    // Connect
    if (ssh_connect(session) != SSH_OK) {
        terminal.appendString("SSH connect failed!\n");
        ssh_free(session);
        session = nullptr;
        return false;
    }
    
    // Authenticate
    int rc = SSH_AUTH_DENIED;
    
    if (SSH_USE_KEY && strlen(SSH_KEY_DATA) > 10) {
        terminal.appendString("Using Key Auth...\n");
        if (onRefresh) onRefresh();
        ssh_key privkey;
        if (ssh_pki_import_privkey_base64(SSH_KEY_DATA, nullptr, nullptr, nullptr, &privkey) == SSH_OK) {
            rc = ssh_userauth_publickey(session, nullptr, privkey);
            ssh_key_free(privkey);
        } else {
            terminal.appendString("Key import failed!\n");
        }
    }

    // Fallback to password (provided or macro fallback) -> actually use the provided one
    if (rc != SSH_AUTH_SUCCESS) {
        if (SSH_USE_KEY) {
             terminal.appendString("Key auth failed, trying password...\n");
             if (onRefresh) onRefresh();
        }
        rc = ssh_userauth_password(session, nullptr, password);
    }
    
    if (rc != SSH_AUTH_SUCCESS) {
        terminal.appendString("SSH auth failed!\n");
        ssh_disconnect(session);
        ssh_free(session);
        session = nullptr;
        return false;
    }
    
    // Open channel
    channel = ssh_channel_new(session);
    if (channel == nullptr) {
        terminal.appendString("SSH channel failed!\n");
        ssh_disconnect(session);
        ssh_free(session);
        session = nullptr;
        return false;
    }
    
    if (ssh_channel_open_session(channel) != SSH_OK) {
        terminal.appendString("Channel open failed!\n");
        ssh_channel_free(channel);
        ssh_disconnect(session);
        ssh_free(session);
        channel = nullptr;
        session = nullptr;
        return false;
    }
    
    // Request PTY
    if (ssh_channel_request_pty_size(channel, TERM_TYPE, TERM_COLS, TERM_ROWS) != SSH_OK) {
        terminal.appendString("PTY request failed!\n");
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        ssh_disconnect(session);
        ssh_free(session);
        channel = nullptr;
        session = nullptr;
        return false;
    }
    
    // Start shell
    if (ssh_channel_request_shell(channel) != SSH_OK) {
        terminal.appendString("Shell request failed!\n");
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        ssh_disconnect(session);
        ssh_free(session);
        channel = nullptr;
        session = nullptr;
        return false;
    }
    
    terminal.appendString("SSH Connected!\n\n");
    ssh_connected = true;
    connectedHost = String(host);
    last_update = millis();
    if (onRefresh) onRefresh();
    
    return true;
}

void SSHClient::disconnect() {
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
    
    ssh_connected = false;
}

void SSHClient::process() {
    if (!ssh_connected) return;
    
    // Reset shortcut flag if Mic released
    if (!keyboard.isMicActive()) {
        mic_shortcut_used = false;
    }

    processKeyboardInput();
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

void SSHClient::processKeyboardInput() {
    // Process all pending key events
    while (keyboard.available() > 0) {
        char c = keyboard.getKeyChar();
        
        if (c == 0) continue; // Skip release events or modifier-only changes
        
        // shortcuts via Mic (which is Ctrl)
        if (keyboard.isMicActive()) {
             mic_shortcut_used = true;
             
             // Since Mic is Ctrl, 'c' is already a Control character (1-26) if it was a-z
             // We need to check what the physical key was, or infer from the control char.
             // Ctrl + W is 23 ('w' - 'a' + 1)
             // Ctrl + A is 1
             // Ctrl + S is 19
             // Ctrl + D is 4
             // Ctrl + Q is 17
             // Ctrl + E is 5
             // Ctrl + H is 8 (Backspace usually, but here 'h')
             
             // Restore base char for mapping check
             char base = 0;
             if (c >= 1 && c <= 26) {
                 base = c + 'a' - 1;
             }
             
             if (base == 'h') {
                 if(onHelp) onHelp();
                 continue;
             }
             
             if (base != 0 && handleMicShortcuts(base)) {
                 continue;
             }
             // If not handled as shortcut, fall through to send the Ctrl character (e.g. Ctrl-C)
        }
        
        // shortcuts via Alt (F1-F9)
        if (keyboard.isAltActive()) {
            if (handleAltShortcuts(c)) continue;
        }
        
        // Regular character
        ssh_channel_write(channel, &c, 1);
    }
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
    
    if (!ssh_connected || channel == nullptr) return;
    
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
            for (int i = 0; i < nbytes; i++) {
                terminal.appendChar(buffer[i]);
            }
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
