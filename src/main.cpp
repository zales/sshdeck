/**
 * T-Deck Pro SSH Terminal
 * 
 * A production-ready SSH terminal client for LilyGO T-Deck Pro
 * featuring E-Paper display, physical QWERTY keyboard, and WiFi connectivity.
 * 
 * Hardware: ESP32-S3, E-Paper GDEQ031T10 (320x240), TCA8418 Keyboard
 * 
 * @author GitHub Copilot
 * @date 2026-01-10
 */

#include <Arduino.h>
#include "config.h"
#include "utilities.h"
#include "display_manager.h"
#include "keyboard_manager.h"
#include "terminal_emulator.h"
#include "ssh_client.h"

// Global managers
DisplayManager display;
KeyboardManager keyboard;
TerminalEmulator terminal;
SSHClient* sshClient = nullptr;

// Forward declaration
void drawTerminalScreen();

/**
 * @brief Initialize all hardware and establish SSH connection
 */
void setup() {
    // Serial debugging
    if (DEBUG_SERIAL_ENABLED) {
        Serial.begin(DEBUG_SERIAL_BAUD);
        delay(100);
        Serial.println("\n\n=================================");
        Serial.println("T-Deck Pro SSH Terminal");
        Serial.println("=================================\n");
    }
    
    // Initialize display
    Serial.println("Initializing power...");
    // Force proper power-up sequence
    pinMode(BOARD_POWERON, OUTPUT);
    digitalWrite(BOARD_POWERON, HIGH);
    
    pinMode(BOARD_1V8_EN, OUTPUT);
    digitalWrite(BOARD_1V8_EN, HIGH); 
    
    pinMode(BOARD_GPS_EN, OUTPUT);
    digitalWrite(BOARD_GPS_EN, HIGH);
    
    pinMode(BOARD_6609_EN, OUTPUT);
    digitalWrite(BOARD_6609_EN, HIGH);
    
    pinMode(BOARD_KEYBOARD_LED, OUTPUT);
    digitalWrite(BOARD_KEYBOARD_LED, HIGH);

    // Disable other SPI devices to prevent bus conflict
    pinMode(BOARD_SD_CS, OUTPUT);
    digitalWrite(BOARD_SD_CS, HIGH);
    pinMode(BOARD_LORA_CS, OUTPUT);
    digitalWrite(BOARD_LORA_CS, HIGH);
    
    delay(1500); // Wait for power to stabilize
    
    if (!display.begin()) {
        Serial.println("Display init failed!");
        while (1) delay(1000);
    }
    
    // Draw initial welcome message directly to display
    display.setRefreshMode(false); // Full update for init
    display.firstPage();
    do {
        display.clear();
        display.drawText(10, 20, "T-Deck Pro SSH Terminal");
        display.drawText(10, 40, "Initializing...");
    } while (display.nextPage());
    
    // Now start terminal output
    terminal.appendString("Init SSH-Deck...\n");
    drawTerminalScreen();
    
    // Initialize keyboard
    terminal.appendString("Init Keyboard...\n");
    drawTerminalScreen();
    
    if (!keyboard.begin()) {
        terminal.appendString("Keyboard FAIL!\n");
        drawTerminalScreen();
        Serial.println("Keyboard init failed!");
        while (1) delay(1000);
    }
    terminal.appendString("Keypad OK!\n");
    drawTerminalScreen();
    
    // Create SSH client
    sshClient = new SSHClient(terminal, keyboard);
    
    // Connect WiFi
    terminal.appendString("Connecting WiFi...\n");
    drawTerminalScreen();
    
    if (!sshClient->connectWiFi()) {
        terminal.appendString("WiFi FAIL!\n");
        drawTerminalScreen();
        Serial.println("WiFi connection failed!");
        while (1) delay(1000);
    }
    
    // WiFi success message already shown by connectWiFi
    drawTerminalScreen();
    
    // Connect SSH
    terminal.appendString("Connecting SSH...\n");
    drawTerminalScreen();
    
    if (!sshClient->connectSSH()) {
        terminal.appendString("SSH FAIL!\n");
        drawTerminalScreen();
        Serial.println("SSH connection failed!");
        while (1) delay(1000);
    }
    
    terminal.appendString("SSH Connected!\n");
    drawTerminalScreen();
    
    Serial.println("Setup complete!");
}

/**
 * @brief Main loop - process SSH I/O and update display
 */
void loop() {
    // Process SSH communication
    if (sshClient && sshClient->isConnected()) {
        sshClient->process();
    }
    
    // Update display only when needed (throttled)
    if (terminal.needsUpdate()) {
        drawTerminalScreen();
    }
    
    delay(10);
}

/**
 * @brief Render terminal content to E-Paper display
 */
void drawTerminalScreen() {
    // Use partial update over entire screen (fast refresh)
    display.setRefreshMode(true);
    display.firstPage();
    
    do {
        display.clear();
        
        U8G2_FOR_ADAFRUIT_GFX& u8g2 = display.getFonts();
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.setFontMode(1);
        
        int y_start = 10;
        int line_h = 10;
        int char_w = 6;
        
        // Render each line
        for (int row = 0; row < TERM_ROWS; row++) {
            const String& line = terminal.getLine(row);
            if (line.length() > 0) {
                for (int col = 0; col < line.length() && col < TERM_COLS; col++) {
                    char ch = line[col];
                    bool inv = terminal.getAttr(row, col).inverse;
                    
                    int x = col * char_w;
                    int y = y_start + (row * line_h);
                    
                    if (inv) {
                         // Explicitly draw background for inverse - needed for empty spaces (htop bars)
                        display.fillRect(x, y - 8, char_w, line_h, GxEPD_BLACK);
                        u8g2.setFontMode(1); // Transparent to keep the black box
                        u8g2.setForegroundColor(GxEPD_WHITE);
                        u8g2.setBackgroundColor(GxEPD_BLACK);
                    } else {
                        u8g2.setFontMode(1); // Transparent
                        u8g2.setForegroundColor(GxEPD_BLACK);
                        u8g2.setBackgroundColor(GxEPD_WHITE);
                    }
                    
                    u8g2.setCursor(x, y);
                    u8g2.print(ch);
                }
            }
        }
        // Draw Cursor
        if (terminal.isCursorVisible()) {
            int cx = terminal.getCursorX();
            int cy = terminal.getCursorY();
            
            // Check bounds
            if (cx >= 0 && cx < TERM_COLS && cy >= 0 && cy < TERM_ROWS) {
                int c_x_px = cx * char_w;
                int c_y_px = y_start + (cy * line_h);
                
                // Get char under cursor (default space)
                char cursorChar = ' ';
                const String& line = terminal.getLine(cy);
                if (cx < line.length()) {
                    cursorChar = line[cx];
                }
                
                // Draw Block Cursor (Inverted)
                display.fillRect(c_x_px, c_y_px - 8, char_w, line_h, GxEPD_BLACK);
                u8g2.setFontMode(1);
                u8g2.setForegroundColor(GxEPD_WHITE);
                u8g2.setBackgroundColor(GxEPD_BLACK);
                u8g2.setCursor(c_x_px, c_y_px);
                u8g2.print(cursorChar);
            }
        }    
    } while (display.nextPage());
    
    // Clear the update flag after drawing
    terminal.clearUpdateFlag();
}
