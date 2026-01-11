#pragma once

// SSH Key Configuration
// Paste your private key between the quotes. valid format:
// R"(
// -----BEGIN OPENSSH PRIVATE KEY-----
// ...
// -----END OPENSSH PRIVATE KEY-----
// )"
#define SSH_KEY_DATA  R"()" 
// Set to true to prefer key auth over password
#define SSH_USE_KEY   false

// Terminal Configuration
#define TERM_COLS     40
#define TERM_ROWS     30
#define TERM_TYPE     "xterm-mono"

// Display Configuration
#define DISPLAY_UPDATE_INTERVAL_MS  200
#define DISPLAY_ROTATION            0  // Portrait mode

// Keyboard Configuration
#define KEYBOARD_INIT_RETRIES      5
#define KEYBOARD_INIT_RETRY_DELAY  500

// Debug Configuration
#define DEBUG_SERIAL_ENABLED       true
#define DEBUG_SERIAL_BAUD          115200
