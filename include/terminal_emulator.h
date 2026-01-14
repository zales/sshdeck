#pragma once

#include <Arduino.h>
#include "config.h"

/**
 * @brief Character attributes for terminal display
 */
struct CharAttr {
    bool inverse;
};

/**
 * @brief Terminal Emulator with ANSI escape sequence support
 * Handles terminal state, ANSI parsing, and screen buffer management
 */
class TerminalEmulator {
public:
    TerminalEmulator();
    
    void appendChar(char c);
    void appendString(const char* str);
    void appendString(const String& str);
    void clear();
    
    const char* getLine(int row) const;
    const CharAttr& getAttr(int row, int col) const;
    
    int getCursorX() const { return cursor_x; }
    int getCursorY() const { return cursor_y; }
    bool isCursorVisible() const { return show_cursor; }
    
    bool needsUpdate() const { return need_display_update; }
    void clearUpdateFlag() { need_display_update = false; }
    bool isAppCursorMode() const { return application_cursor_mode; }
    
private:
    // Buffers - Fixed char arrays to prevent heap fragmentation
    char lines_primary[TERM_ROWS][TERM_COLS + 1];
    CharAttr attrs_primary[TERM_ROWS][TERM_COLS];
    
    char lines_alt[TERM_ROWS][TERM_COLS + 1];
    CharAttr attrs_alt[TERM_ROWS][TERM_COLS];
    
    // Pointers to active buffer
    char (*term_lines)[TERM_COLS + 1]; 
    CharAttr (*term_attrs)[TERM_COLS];
    bool is_alt_buffer;

    // Line drawing state
    bool use_line_drawing;  // Current G0 state
    
    int cursor_x;
    int cursor_y;
    int saved_cursor_x;
    int saved_cursor_y;
    int scrollTop;     // Top margin (0-based)
    int scrollBottom;  // Bottom margin (0-based, inclusive)
    bool current_inverse;
    bool show_cursor;
    bool application_cursor_mode; // DECCKM
    
    bool need_display_update;
    
    // ANSI parsing state
    enum AnsiState {
        ANSI_NORMAL,
        ANSI_ESC,
        ANSI_CSI_PARAM,
        ANSI_OSC,           // Operating System Commands
        ANSI_WAIT_CHAR      // Consume one character then go to NORMAL
    };
    
    AnsiState ansi_state;
    String ansi_buffer;
    
    bool processAnsi(char c);
    void handleAnsiCommand(const String& cmd);
    void scrollUp();
    void scrollDown();
    void switchBuffer(bool alt);
};
