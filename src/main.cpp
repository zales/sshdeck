#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_TCA8418.h>
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <libssh/libssh.h>
//#define XPOWERS_CHIP_AXP2101
//#include <XPowersLib.h>
#include "utilities.h"
#include "keymap.h"

// =============================================================================
// USER CONFIGURATION
// =============================================================================
const char* WIFI_SSID     = "Zales";
const char* WIFI_PASSWORD = "n2w85r3mg2xs9";

const char* SSH_HOST      = "192.168.1.195";
const char* SSH_USER      = "zales";
const char* SSH_PASS      = "9B4990uqondrasek";
const int   SSH_PORT      = 22;

// =============================================================================
// HARDWARE SETUP
// =============================================================================

// PMU
// XPowersPMU PMU;

// Display Setup for T-Deck Pro (E-Paper)
GxEPD2_BW<GxEPD2_310_GDEQ031T10, GxEPD2_310_GDEQ031T10::HEIGHT> display(
    GxEPD2_310_GDEQ031T10(BOARD_EPD_CS, BOARD_EPD_DC, BOARD_EPD_RST, BOARD_EPD_BUSY)
);

// Font Handler
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

// Keyboard Setup
Adafruit_TCA8418 keypad;

// SSH Session
ssh_session my_ssh_session;
ssh_channel my_ssh_channel;
bool ssh_connected = false;

// Terminal State
#define TERM_COLS 40  // Display rotated: 240px width / 6px font = 40 cols
#define TERM_ROWS 32  // Display rotated: 320px height / 10px line = 32 rows
String term_lines[TERM_ROWS];
int cursor_x = 0;
int cursor_y = 0;
bool need_display_update = false;
unsigned long last_update_time = 0;

// Keyboard State
bool is_sym = false;
bool is_shift = false;
bool is_ctrl = false;

// ANSI State
enum AnsiState { ANSI_NORMAL, ANSI_ESC, ANSI_CSI, ANSI_CSI_PARAM };
AnsiState ansi_state = ANSI_NORMAL;
String ansi_params = "";
char ansi_final = 0;

// Character attributes per position
struct CharAttr {
    bool inverse;
};
CharAttr term_attrs[TERM_ROWS][TERM_COLS];
bool current_inverse = false;

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

// Forward declaration
void handleAnsiCommand();

bool processAnsi(char c) {
    switch (ansi_state) {
        case ANSI_NORMAL:
            if (c == '\x1B') {
                ansi_state = ANSI_ESC;
                return false;
            }
            return true;
            
        case ANSI_ESC:
            if (c == '[') {
                ansi_state = ANSI_CSI_PARAM;
                ansi_params = "";
                ansi_final = 0;
            } else {
                ansi_state = ANSI_NORMAL;
            }
            return false;
            
        case ANSI_CSI_PARAM:
            if (c >= '0' && c <= '9' || c == ';' || c == '?') {
                ansi_params += c;
                return false;
            } else if (c >= '@' && c <= '~') {
                // Final byte of CSI sequence
                ansi_final = c;
                handleAnsiCommand();
                ansi_state = ANSI_NORMAL;
                return false;
            } else {
                ansi_state = ANSI_NORMAL;
                return false;
            }
    }
    return true;
}

void handleAnsiCommand() {
    int params[4] = {0, 0, 0, 0};
    int param_count = 0;
    
    // Parse parameters
    if (ansi_params.length() > 0) {
        int start = 0;
        for (int i = 0; i <= ansi_params.length(); i++) {
            if (i == ansi_params.length() || ansi_params[i] == ';') {
                if (i > start) {
                    params[param_count++] = ansi_params.substring(start, i).toInt();
                    if (param_count >= 4) break;
                }
                start = i + 1;
            }
        }
    }
    
    switch (ansi_final) {
        case 'H': // CUP - Cursor Position
        case 'f': // HVP - Horizontal Vertical Position
            cursor_y = (params[0] > 0 ? params[0] - 1 : 0);
            cursor_x = (params[1] > 0 ? params[1] - 1 : 0);
            if (cursor_y >= TERM_ROWS) cursor_y = TERM_ROWS - 1;
            if (cursor_x >= TERM_COLS) cursor_x = TERM_COLS - 1;
            break;
            
        case 'A': // CUU - Cursor Up
            cursor_y -= (params[0] > 0 ? params[0] : 1);
            if (cursor_y < 0) cursor_y = 0;
            break;
            
        case 'B': // CUD - Cursor Down
            cursor_y += (params[0] > 0 ? params[0] : 1);
            if (cursor_y >= TERM_ROWS) cursor_y = TERM_ROWS - 1;
            break;
            
        case 'C': // CUF - Cursor Forward
            cursor_x += (params[0] > 0 ? params[0] : 1);
            if (cursor_x >= TERM_COLS) cursor_x = TERM_COLS - 1;
            break;
            
        case 'D': // CUB - Cursor Back
            cursor_x -= (params[0] > 0 ? params[0] : 1);
            if (cursor_x < 0) cursor_x = 0;
            break;
            
        case 'J': // ED - Erase Display
            if (params[0] == 2 || params[0] == 3) {
                // Clear entire screen
                for (int i = 0; i < TERM_ROWS; i++) {
                    term_lines[i] = "";
                }
                cursor_x = 0;
                cursor_y = 0;
            } else if (params[0] == 0) {
                // Clear from cursor to end of screen
                term_lines[cursor_y] = term_lines[cursor_y].substring(0, cursor_x);
                for (int i = cursor_y + 1; i < TERM_ROWS; i++) {
                    term_lines[i] = "";
                }
            } else if (params[0] == 1) {
                // Clear from cursor to beginning
                String rest = "";
                if (cursor_x < term_lines[cursor_y].length()) {
                    rest = term_lines[cursor_y].substring(cursor_x);
                }
                term_lines[cursor_y] = String(' ', cursor_x) + rest;
                for (int i = 0; i < cursor_y; i++) {
                    term_lines[i] = "";
                }
            }
            break;
            
        case 'K': // EL - Erase Line
            if (params[0] == 0) {
                // Clear from cursor to end of line
                term_lines[cursor_y] = term_lines[cursor_y].substring(0, cursor_x);
            } else if (params[0] == 1) {
                // Clear from start to cursor
                String rest = "";
                if (cursor_x < term_lines[cursor_y].length()) {
                    rest = term_lines[cursor_y].substring(cursor_x);
                }
                term_lines[cursor_y] = String(' ', cursor_x) + rest;
            } else if (params[0] == 2) {
                // Clear entire line
                term_lines[cursor_y] = "";
            }
            break;
            
        case 'm': // SGR - Select Graphic Rendition
            if (param_count == 0) {
                // Reset all attributes
                current_inverse = false;
            } else {
                for (int i = 0; i < param_count; i++) {
                    if (params[i] == 0) {
                        current_inverse = false; // Reset
                    } else if (params[i] == 7) {
                        current_inverse = true; // Inverse on
                    } else if (params[i] == 27) {
                        current_inverse = false; // Inverse off
                    }
                }
            }
            break;
    }
}

void drawTerminal() {
    display.setPartialWindow(0, 0, display.width(), display.height());
    display.firstPage();
    do {
        display.fillScreen(GxEPD_BLACK); // Black background for terminal
        
        u8g2Fonts.setFont(u8g2_font_6x10_tf); 
        u8g2Fonts.setFontMode(1); // Transparent
        
        int y_start = 10; // First line baseline
        int line_h = 10;  // Line spacing
        int char_w = 6;   // Character width
        
        for (int row = 0; row < TERM_ROWS; row++) {
            String &line = term_lines[row];
            if (line.length() > 0) {
                for (int col = 0; col < line.length() && col < TERM_COLS; col++) {
                    char ch = line[col];
                    bool inv = term_attrs[row][col].inverse;
                    
                    int x = col * char_w;
                    int y = y_start + (row * line_h);
                    
                    if (inv) {
                        // Inverse: black text on white background
                        display.fillRect(x, y - 8, char_w, line_h, GxEPD_WHITE);
                        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
                    } else {
                        // Normal: white text on black background
                        u8g2Fonts.setForegroundColor(GxEPD_WHITE);
                    }
                    
                    u8g2Fonts.setCursor(x, y);
                    u8g2Fonts.print(ch);
                }
            }
        }
    } while (display.nextPage());
}


void appendToTerminal(char c) {
    if (!processAnsi(c)) return;

    if (c == '\r') {
        cursor_x = 0;
        return;
    }
    
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= TERM_ROWS) {
            // Scroll up
            for (int i = 0; i < TERM_ROWS - 1; i++) {
                term_lines[i] = term_lines[i+1];
            }
            term_lines[TERM_ROWS-1] = "";
            cursor_y = TERM_ROWS - 1;
        }
        return;
    }
    
    if (c == 0x08) { // Backspace
        if (cursor_x > 0) {
            cursor_x--;
            if (cursor_x < term_lines[cursor_y].length()) {
                term_lines[cursor_y].remove(cursor_x, 1);
            }
        }
        return;
    }
    
    // Regular character - insert at cursor position
    String &line = term_lines[cursor_y];
    
    // Extend line with spaces if needed
    while (line.length() < cursor_x) {
        line += ' ';
        term_attrs[cursor_y][line.length() - 1].inverse = false;
    }
    
    if (cursor_x >= line.length()) {
        line += c;
    } else {
        line[cursor_x] = c;
    }
    
    // Store attributes
    if (cursor_x < TERM_COLS) {
        term_attrs[cursor_y][cursor_x].inverse = current_inverse;
    }
    
    cursor_x++;
    if (cursor_x >= TERM_COLS) {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= TERM_ROWS) {
            // Scroll up
            for (int i = 0; i < TERM_ROWS - 1; i++) {
                term_lines[i] = term_lines[i+1];
            }
            term_lines[TERM_ROWS-1] = "";
            cursor_y = TERM_ROWS - 1;
        }
    }
    
    need_display_update = true;
}

void appendString(const char* str) {
    while (*str) {
        appendToTerminal(*str++);
    }
}

// Convert TCA8418 key to character based on keymap.h logic
char getKeyChar(int row, int col, bool pressed) {
    char c = 0;
    
    // Valid Key Check
    if (row < 0 || row >= KEY_ROWS || col < 0 || col >= KEY_COLS) return 0;

    // Handle Modifiers (State Based)
    bool is_sym_key = (row == KEY_SYM_ROW && col == KEY_SYM_COL);
    bool is_shift_key = ((row == KEY_SHIFT_L_ROW && col == KEY_SHIFT_L_COL) || 
                         (row == KEY_SHIFT_R_ROW && col == KEY_SHIFT_R_COL));
    bool is_ctrl_key = (row == KEY_MIC_ROW && col == KEY_MIC_COL); // MIC button = CTRL
                         
    if (is_sym_key) {
        is_sym = pressed; 
        return 0;
    }
    if (is_shift_key) {
        is_shift = pressed;
        return 0;
    }
    if (is_ctrl_key) {
        is_ctrl = pressed;
        return 0;
    }

    // Only emit characters on PRESS
    if (!pressed) return 0;
    
    if (row == KEY_ENTER_ROW && col == KEY_ENTER_COL) return '\n';
    if (row == KEY_BACKSPACE_ROW && col == KEY_BACKSPACE_COL) return 0x08;
    if (row == KEY_SPACE_ROW && col == KEY_SPACE_COL) return ' ';

    // Get base character
    char base_c = keymap_lower[row][col];
    
    // SYM combinations for arrows, ESC, TAB
    if (is_sym && ssh_connected) {
        if (base_c == 'w') {
            ssh_channel_write(my_ssh_channel, "\x1B[A", 3); // Up
            return 0;
        }
        if (base_c == 'a') {
            ssh_channel_write(my_ssh_channel, "\x1B[D", 3); // Left
            return 0;
        }
        if (base_c == 's') {
            ssh_channel_write(my_ssh_channel, "\x1B[B", 3); // Down
            return 0;
        }
        if (base_c == 'd') {
            ssh_channel_write(my_ssh_channel, "\x1B[C", 3); // Right
            return 0;
        }
        if (base_c == 'q') {
            char esc = '\x1B';
            ssh_channel_write(my_ssh_channel, &esc, 1); // ESC
            return 0;
        }
        if (base_c == 'e') {
            return '\t'; // TAB
        }
    }

    // Standard keys
    if (is_sym) {
        c = keymap_symbol[row][col];
    } else {
        c = base_c;
        
        // Ctrl combinations
        if (is_ctrl && c >= 'a' && c <= 'z') {
            // Ctrl+A = 0x01, Ctrl+B = 0x02, ..., Ctrl+Z = 0x1A
            return c - 'a' + 1;
        }
        
        if (is_shift && c >= 'a' && c <= 'z') {
            c -= 32; // To uppercase
        }
    }
    return c;
}


// =============================================================================
// SETUP
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(100);
    
    // Initialize terminal attributes
    for (int i = 0; i < TERM_ROWS; i++) {
        for (int j = 0; j < TERM_COLS; j++) {
            term_attrs[i][j].inverse = false;
        }
    }
    
    // 1. Early I2C Init (before power)
    Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
    Wire.setClock(100000);
    
    // 2. Power Init
    // T-Deck Pro uses discrete power enables
    pinMode(BOARD_1V8_EN, OUTPUT);
    digitalWrite(BOARD_1V8_EN, HIGH); 
    
    pinMode(BOARD_GPS_EN, OUTPUT);
    digitalWrite(BOARD_GPS_EN, HIGH);
    
    pinMode(BOARD_6609_EN, OUTPUT);
    digitalWrite(BOARD_6609_EN, HIGH);
    
    pinMode(BOARD_KEYBOARD_LED, OUTPUT);
    digitalWrite(BOARD_KEYBOARD_LED, HIGH); // Enable keyboard power
    
    delay(1500); // Allow power to stabilize
    Serial.println("Power Rails Enabled");
    
    // Re-init I2C after power stabilization
    Wire.end();
    delay(50);
    Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
    Wire.setClock(100000);
    Wire.setTimeout(500);
    delay(200);
    
    // Setup keyboard interrupt pin
    pinMode(KEYPAD_INT, INPUT_PULLUP);
    delay(100);
    
    // I2C Scan
    Serial.println("Scanning I2C bus...");
    int nDevices = 0;
    for(byte address = 1; address < 127; address++ )
    {
        Wire.beginTransmission(address);
        byte error = Wire.endTransmission();
    
        if (error == 0)
        {
            Serial.print("I2C device found at address 0x");
            if (address<16) Serial.print("0");
            Serial.print(address,HEX);
            Serial.println("  !");
            nDevices++;
        }
        else if (error==4) 
        {
            Serial.print("Unknown error at address 0x");
            if (address<16) Serial.print("0");
            Serial.println(address,HEX);
        }    
    }
    if (nDevices == 0)
        Serial.println("No I2C devices found\n");
    else
        Serial.println("done\n");



    
    pinMode(BOARD_LORA_CS, OUTPUT); digitalWrite(BOARD_LORA_CS, HIGH); // Deselect Chips
    pinMode(BOARD_SD_CS, OUTPUT); digitalWrite(BOARD_SD_CS, HIGH);
    
    // Reset Peripherals to clear I2C bus
    pinMode(BOARD_TOUCH_RST, OUTPUT);
    digitalWrite(BOARD_TOUCH_RST, LOW);
    delay(20);
    digitalWrite(BOARD_TOUCH_RST, HIGH);
    delay(50);
    
    pinMode(BOARD_LORA_RST, OUTPUT); 
    digitalWrite(BOARD_LORA_RST, LOW);
    delay(20);
    digitalWrite(BOARD_LORA_RST, HIGH);
    delay(50);

    // 2. Display Init
    SPI.begin(BOARD_EPD_SCK, -1, BOARD_EPD_MOSI, BOARD_EPD_CS);
    display.init(115200, true, 2, false);
    display.setRotation(0); // Rotated 90 deg from 1 (User request)
    display.fillScreen(GxEPD_WHITE);
    display.display();
    
    u8g2Fonts.begin(display); // Connect u8g2 procedures to GFX
    
    appendString("Init SSH-Deck...\n");
    drawTerminal();

    // 3. I2C & Keypad
    // Use the already initialized Wire
    
    // Add I2C bus recovery?
    // Wire.setClock(100000); 

    if (nDevices == 0) {
        appendString("I2C Bus Empty!\nCheck Power.\n");
        drawTerminal();
    }
    
    // Attempt keypad with retry
    Serial.println("Attempting TCA8418 init...");
    Serial.printf("I2C Pins: SDA=%d, SCL=%d\n", BOARD_I2C_SDA, BOARD_I2C_SCL);
    Serial.printf("Expected Address: 0x%02X\n", TCA8418_DEFAULT_ADDR);
    
    bool keypad_found = false;
    
    for (int attempt = 0; attempt < 5; attempt++) {
        Serial.printf("Keypad init attempt %d...\n", attempt + 1);
        
        // Try to reset the TCA8418 if possible
        if (attempt > 0) {
            Wire.end();
            delay(100);
            Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
            Wire.setClock(100000);
            delay(100);
        }
        
        keypad_found = keypad.begin(TCA8418_DEFAULT_ADDR, &Wire);
        if (keypad_found) {
            Serial.println("Keypad found!");
            break;
        }
        delay(300);
    }
    
    if (!keypad_found) {
        Serial.println("Keypad NOT found at 0x34");
        Serial.println("Check: 1) I2C wiring 2) Power 3) I2C pullups");
        appendString("Keypad Fail!\n");
        drawTerminal();
        // Allow continue, maybe just no keypad
    } else {
         // Configure Matrix (4 rows, 10 columns)
        Serial.printf("Configuring matrix: %dx%d\n", KEY_ROWS, KEY_COLS);
        if (!keypad.matrix(KEY_ROWS, KEY_COLS)) {
            Serial.println("Matrix config failed!");
            appendString("Matrix Init Fail\n");
            drawTerminal();
        } else {
            Serial.println("Matrix configured successfully!");
            appendString("Keypad OK!\n");
            drawTerminal();
        }
        keypad.flush();
        Serial.println("Keypad ready.");
    }
    
    // 4. WiFi
    appendString("Connecting WiFi...\n");
    drawTerminal();
    
    WiFi.setHostname("tvoje-mama");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 20) {
        delay(500);
        tries++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        appendString("WiFi Connected!\nIP: ");
        appendString(WiFi.localIP().toString().c_str());
        appendString("\n");
        drawTerminal();
    } else {
        appendString("WiFi Failed.\n");
        drawTerminal();
        // Here we could return or allow offline typing
    }

    // 5. SSH Connect
    if (WiFi.status() == WL_CONNECTED) {
        appendString("Connecting SSH...\n");
        drawTerminal();
        
        my_ssh_session = ssh_new();
        if (my_ssh_session == NULL) {
             appendString("SSH Alloc Fail\n");
             drawTerminal();
             return;
        }

        ssh_options_set(my_ssh_session, SSH_OPTIONS_HOST, SSH_HOST);
        ssh_options_set(my_ssh_session, SSH_OPTIONS_USER, SSH_USER);
        int port = SSH_PORT;
        ssh_options_set(my_ssh_session, SSH_OPTIONS_PORT, &port);

        int rc = ssh_connect(my_ssh_session);
        if (rc != SSH_OK) {
            appendString("SSH Connect Fail:\n");
            appendString(ssh_get_error(my_ssh_session));
            appendString("\n");
            drawTerminal();
            ssh_free(my_ssh_session);
            return;
        }

        rc = ssh_userauth_password(my_ssh_session, NULL, SSH_PASS);
        if (rc != SSH_AUTH_SUCCESS) {
            appendString("SSH Auth Fail:\n");
            appendString(ssh_get_error(my_ssh_session));
            appendString("\n");
            drawTerminal();
            ssh_disconnect(my_ssh_session);
            ssh_free(my_ssh_session);
            return;
        }

        my_ssh_channel = ssh_channel_new(my_ssh_session);
        if (my_ssh_channel == NULL) {
             appendString("SSH Channel Fail\n");
             drawTerminal();
             return;
        }

        rc = ssh_channel_open_session(my_ssh_channel);
        if (rc != SSH_OK) {
            appendString("SSH Chan Open Fail\n");
            drawTerminal();
            return;
        }

        // Request PTY with monochrome terminal type
        ssh_channel_request_pty_size(my_ssh_channel, "xterm-mono", TERM_COLS, TERM_ROWS);
        ssh_channel_request_shell(my_ssh_channel);
        
        ssh_connected = true;
        appendString("SSH Connected!\n");
        drawTerminal();
    }
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
    // 1. Handle Keypad Input -> Send to SSH
    // Check if keypad was initialized successfully? 
    // TCA8418::available() calls readRegister which might crash if bus is stuck
    // But if begin() failed, typically we shouldn't be here or we should check flag.
    // For now, assume if scanner found devices, bus works.
    
    // Add a safe check or try/catch if C++ allowed it (no exceptions on ESP32 default)
    // Just try reading. If Error 263 persists, it might spam.
    
    if (keypad.available()) {
        uint8_t k_evt = keypad.getEvent();
        // Bit 7 is 1 for Press, 0 for Release in TCA8418
        bool is_pressed = (k_evt & 0x80) != 0; 
        int key_code = k_evt & 0x7F;

        if (key_code > 0) {
             int r = (key_code - 1) / 10;
             int c = (key_code - 1) % 10;
             
             // Mirror column mapping (keyboard is wired reversed)
             c = (KEY_COLS - 1) - c;
             
             // Pass press state to update modifiers and get char
             char ch = getKeyChar(r, c, is_pressed);
             
             if (ch != 0) {
                 if (ssh_connected) {
                     ssh_channel_write(my_ssh_channel, &ch, 1);
                 } else {
                     appendToTerminal(ch); // Local echo
                 }
                 // Force update on typing to see text immediately
                 need_display_update = true;
             }
        }
    }

    // 2. Handle SSH Output -> Terminal Buffer
    // Read only a small chunk to keep loop responsive
    if (ssh_connected) {
        char buffer[64]; // Reduced buffer size
        int nbytes = ssh_channel_read_nonblocking(my_ssh_channel, buffer, sizeof(buffer)-1, 0);
        if (nbytes > 0) {
            buffer[nbytes] = 0;
            appendString(buffer);
        } else if (nbytes == SSH_EOF) {
            ssh_connected = false;
            appendString("\nSSH Connection Closed\n");
        }
    }

    // 3. Update Display Throttled
    if (need_display_update && (millis() - last_update_time > 200)) {
        drawTerminal();
        need_display_update = false;
        last_update_time = millis();
    }
    
    // 4. Yield to IDLE task
    delay(10); 
    
    // Monitor Stack
    // if (millis() % 5000 == 0) Serial.printf("Stack: %d\n", uxTaskGetStackHighWaterMark(NULL));
}
