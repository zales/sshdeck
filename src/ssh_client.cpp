#include "ssh_client.h"
#include "keymap.h"

SSHClient::SSHClient(TerminalEmulator& term, KeyboardManager& kb) 
    : terminal(term), keyboard(kb), session(nullptr), channel(nullptr), 
      ssh_connected(false), last_update(0) {
}

SSHClient::~SSHClient() {
    disconnect();
}

bool SSHClient::connectWiFi() {
    terminal.appendString("Connecting WiFi...\n");
    
    WiFi.setHostname(WIFI_HOSTNAME);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 20) {
        delay(500);
        tries++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        terminal.appendString("WiFi Connected!\nIP: ");
        terminal.appendString(WiFi.localIP().toString().c_str());
        terminal.appendString("\n");
        return true;
    } else {
        terminal.appendString("WiFi Failed!\n");
        return false;
    }
}

bool SSHClient::connectSSH() {
    terminal.appendString("Connecting SSH...\n");
    
    // Initialize SSH session
    session = ssh_new();
    if (session == nullptr) {
        terminal.appendString("SSH init failed!\n");
        return false;
    }
    
    ssh_options_set(session, SSH_OPTIONS_HOST, SSH_HOST);
    int port = SSH_PORT;
    ssh_options_set(session, SSH_OPTIONS_PORT, &port);
    ssh_options_set(session, SSH_OPTIONS_USER, SSH_USER);
    
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
    if (ssh_userauth_password(session, nullptr, SSH_PASS) != SSH_AUTH_SUCCESS) {
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
    last_update = millis();
    
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
    
    processKeyboardInput();
    processSSHOutput();
}

void SSHClient::processKeyboardInput() {
    // Process all pending key events
    while (keyboard.available() > 0) {
        char c = keyboard.getKeyChar();
        if (c == 0) continue; // Skip release events or modifier-only changes
        
        // Check for SYM combinations (arrows, ESC, TAB)
        if (keyboard.isSymActive()) {
            char base_c = 0;
            // Find base character from keymap
            for (int row = 0; row < KEY_ROWS; row++) {
                for (int col = 0; col < KEY_COLS; col++) {
                    if (keymap_lower[row][col] == c) {
                        base_c = c;
                        break;
                    }
                }
                if (base_c) break;
            }
            
            if (base_c) {
                handleSYMCombinations(base_c);
                continue; // Done with this char
            }
        }
        
        // Regular character
        ssh_channel_write(channel, &c, 1);
    }
}

void SSHClient::handleSYMCombinations(char base_c) {
    switch (base_c) {
        case 'w':  // Up arrow
            ssh_channel_write(channel, "\x1B[A", 3);
            break;
        case 'a':  // Left arrow
            ssh_channel_write(channel, "\x1B[D", 3);
            break;
        case 's':  // Down arrow
            ssh_channel_write(channel, "\x1B[B", 3);
            break;
        case 'd':  // Right arrow
            ssh_channel_write(channel, "\x1B[C", 3);
            break;
        case 'q': {  // ESC
            char esc = '\x1B';
            ssh_channel_write(channel, &esc, 1);
            break;
        }
        case 'e': {  // TAB
            char tab = '\t';
            ssh_channel_write(channel, &tab, 1);
            break;
        }
    }
}

void SSHClient::processSSHOutput() {
    char buffer[1024];
    int total_read = 0;
    
    // Read until buffer is empty or we processed a safety limit
    while (total_read < 8192) { // Safety limit 8KB per loop
        int nbytes = ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer) - 1, 0);
        
        if (nbytes <= 0) break;
        
        buffer[nbytes] = '\0';
        for (int i = 0; i < nbytes; i++) {
            terminal.appendChar(buffer[i]);
        }
        total_read += nbytes;
        last_update = millis();
    }
}
