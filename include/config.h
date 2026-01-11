#pragma once

// WiFi Configuration
#define WIFI_SSID     "Zales"
#define WIFI_PASSWORD "n2w85r3mg2xs9"
#define WIFI_HOSTNAME "tvoje-mama"

// SSH Configuration
#define SSH_HOST      "192.168.1.195"
#define SSH_USER      "zales"
#define SSH_PASS      "9B4990uqondrasek"
#define SSH_PORT      22

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
