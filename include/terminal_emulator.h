#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
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
    
    // Thread-safe read lock (use when reading terminal state from another thread)
    void lock();
    void unlock();
    
    const char* getLine(int row) const;
    const CharAttr& getAttr(int row, int col) const;
    
    int getCursorX() const { return cursor_x; }
    int getCursorY() const { return cursor_y; }
    bool isCursorVisible() const { return show_cursor; }
    
    bool needsUpdate() const { return need_display_update; }
    void clearUpdateFlag();
    bool isAppCursorMode() const { return application_cursor_mode; }
    
    // Scrollback history
    void scrollViewUp(int lines = 3);    // Scroll back into history
    void scrollViewDown(int lines = 3);  // Scroll forward toward live
    void scrollViewReset();              // Return to live view
    int  getViewOffset() const { return _viewOffset; }
    bool isViewingHistory() const { return _viewOffset > 0; }
    
    // Get line for display (accounts for scrollback offset)
    const char* getDisplayLine(int row) const;
    const CharAttr& getDisplayAttr(int row, int col) const;
    
    // Check specifically which rows are dirty
    bool isRowDirty(int row) const { return (row >= 0 && row < TERM_ROWS) ? dirty_rows[row] : false; }
    
    // Get the range of dirty rows. Returns false if no rows are dirty.
    bool getDirtyRange(int& startRow, int& endRow) const {
        startRow = -1;
        endRow = -1;
        for (int i = 0; i < TERM_ROWS; i++) {
            if (dirty_rows[i]) {
                if (startRow == -1) startRow = i;
                endRow = i;
            }
        }
        return (startRow != -1);
    }

private:
    void _appendCharImpl(char c);
    
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
    bool dirty_rows[TERM_ROWS]; // Track dirty rows

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
    
    SemaphoreHandle_t _mutex;
    
    // Scrollback ring buffer
    char _scrollback[SCROLLBACK_LINES][TERM_COLS + 1];
    CharAttr _scrollbackAttrs[SCROLLBACK_LINES][TERM_COLS];
    int _scrollbackHead;   // Next write position (ring buffer)
    int _scrollbackCount;  // How many lines stored (0..SCROLLBACK_LINES)
    int _viewOffset;       // 0 = live view, >0 = scrolled back N lines
    
    void pushScrollbackLine(const char* line, const CharAttr* attrs);
};
