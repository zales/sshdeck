#include "terminal_emulator.h"

TerminalEmulator::TerminalEmulator() 
    : cursor_x(0), cursor_y(0), saved_cursor_x(0), saved_cursor_y(0),
      current_inverse(false), ansi_state(ANSI_NORMAL),
      show_cursor(true), application_cursor_mode(false), need_display_update(false),
      scrollTop(0), scrollBottom(TERM_ROWS - 1),
      is_alt_buffer(false), use_line_drawing(false) {
      
    // Point to primary buffer initially
    term_lines = lines_primary;
    term_attrs = attrs_primary;

    // Initialize attributes for both buffers
    for (int i = 0; i < TERM_ROWS; i++) {
        memset(lines_primary[i], 0, TERM_COLS + 1);
        memset(lines_alt[i], 0, TERM_COLS + 1);
        for (int j = 0; j < TERM_COLS; j++) {
            attrs_primary[i][j].inverse = false;
            attrs_alt[i][j].inverse = false;
        }
    }
}

void TerminalEmulator::switchBuffer(bool alt) {
    if (alt == is_alt_buffer) return;
    
    is_alt_buffer = alt;
    if (is_alt_buffer) {
        term_lines = lines_alt;
        term_attrs = attrs_alt;
    } else {
        term_lines = lines_primary;
        term_attrs = attrs_primary;
    }
    need_display_update = true;
}

void TerminalEmulator::appendChar(char c) {
    if (!processAnsi(c)) return;
    
    if (c == '\r') {
        cursor_x = 0;
        return;
    }
    
    if (c == '\n') {
        cursor_x = 0;
        if (cursor_y == scrollBottom) {
             scrollUp();
        } else if (cursor_y < TERM_ROWS - 1) {
             cursor_y++;
        }
        need_display_update = true;
        return;
    }
    
    if (c == 0x08) {  // Backspace
        if (cursor_x > 0) {
            cursor_x--;
        }
        return;
    }

    if (c == '\t') { // Tab
        int next_tab = (cursor_x / 8 + 1) * 8;
        if (next_tab >= TERM_COLS) next_tab = TERM_COLS - 1;
        
        while (cursor_x < next_tab) {
             // Pad gap with spaces
             int len = strlen(term_lines[cursor_y]);
             if (cursor_x >= len) {
                 term_lines[cursor_y][cursor_x] = ' ';
                 term_lines[cursor_y][cursor_x + 1] = '\0';
             }
             term_attrs[cursor_y][cursor_x] = {current_inverse};
             cursor_x++;
        }
        need_display_update = true;
        return;
    }
    
    // Printable character
    if (c >= 32 && c < 127) {
        char display_char = c;
        if (use_line_drawing) {
             // Simple fallback map
             if (c >= 'j' && c <= 'm') display_char = '+'; 
             else if (c == 'q') display_char = '-';
             else if (c == 'x') display_char = '|';
             else if (c == '`') display_char = '+'; 
             else if (c == 'a') display_char = '#';
        }

        // Check if we are writing past end of current string in buffer
        // Note: term_lines[y] is zero-init, so strlen works.
        int len = strlen(term_lines[cursor_y]);
        
        // Pad with spaces if jumping (remote possibility in cursor move)
        while (len < cursor_x) {
             term_lines[cursor_y][len] = ' ';
             term_lines[cursor_y][len+1] = '\0';
             len++;
        }

        term_lines[cursor_y][cursor_x] = display_char;
        if (cursor_x == len) {
            term_lines[cursor_y][cursor_x + 1] = '\0';
        }
        
        term_attrs[cursor_y][cursor_x].inverse = current_inverse;
        
        cursor_x++;
        need_display_update = true;
        
        if (cursor_x >= TERM_COLS) {
            cursor_x = 0;
            if (cursor_y == scrollBottom) {
                scrollUp();
            } else if (cursor_y < TERM_ROWS - 1) {
                cursor_y++;
            }
        }
    }
}



void TerminalEmulator::appendString(const char* str) {
    while (*str) {
        appendChar(*str++);
    }
}

void TerminalEmulator::appendString(const String& str) {
    appendString(str.c_str());
}

void TerminalEmulator::clear() {
    for (int i = 0; i < TERM_ROWS; i++) {
        term_lines[i][0] = '\0';
        for (int j = 0; j < TERM_COLS; j++) {
            term_attrs[i][j].inverse = false;
        }
    }
    cursor_x = 0;
    cursor_y = 0;
    need_display_update = true;
}

const char* TerminalEmulator::getLine(int row) const {
    return term_lines[row];
}

const CharAttr& TerminalEmulator::getAttr(int row, int col) const {
    return term_attrs[row][col];
}

bool TerminalEmulator::processAnsi(char c) {
    switch (ansi_state) {
        case ANSI_NORMAL:
            if (c == '\x1B') {
                ansi_state = ANSI_ESC;
                ansi_buffer = "";
                return false;
            }
            return true;
            
        case ANSI_ESC:
            if (c == '[') {
                ansi_state = ANSI_CSI_PARAM;
                return false;
            } else if (c == 'M') {
                // Reverse Index (RI)
                if (cursor_y == scrollTop) {
                    scrollDown();
                } else if (cursor_y > 0) {
                    cursor_y--;
                }
                need_display_update = true;
                ansi_state = ANSI_NORMAL;
                return false;
            } else if (c == 'D') {
                 // Index (IND)
                 if (cursor_y == scrollBottom) {
                    scrollUp();
                 } else if (cursor_y < TERM_ROWS - 1) {
                    cursor_y++;
                 }
                 need_display_update = true;
                 ansi_state = ANSI_NORMAL;
                 return false;
            } else if (c == 'E') {
                 // Next Line (NEL)
                 cursor_x = 0;
                 if (cursor_y == scrollBottom) {
                    scrollUp();
                 } else if (cursor_y < TERM_ROWS - 1) {
                    cursor_y++;
                 }
                 need_display_update = true;
                 ansi_state = ANSI_NORMAL;
                 return false;
            } else if (c == '7') {
                // Save Cursor (DECSC)
                saved_cursor_x = cursor_x;
                saved_cursor_y = cursor_y;
                ansi_state = ANSI_NORMAL;
                return false;
            } else if (c == '8') {
                // Restore Cursor (DECRC)
                cursor_x = saved_cursor_x;
                cursor_y = saved_cursor_y;
                if (cursor_x >= TERM_COLS) cursor_x = TERM_COLS - 1;
                if (cursor_y >= TERM_ROWS) cursor_y = TERM_ROWS - 1;
                need_display_update = true;
                ansi_state = ANSI_NORMAL;
                return false;
            } else if (c == '=') {
                // Application Keypad
                ansi_state = ANSI_NORMAL;
                return false;
            } else if (c == '>') {
                // Normal Keypad
                ansi_state = ANSI_NORMAL;
                return false;
            } else if (c == ']') {
                // OSC Sequence (Operating System Command)
                // Used for setting window title, colors, etc.
                ansi_state = ANSI_OSC;
                ansi_buffer = "";
                return false;
            } else if (c == 'c') {
                // Full Reset (RIS)
                clear();
                // Reset margins
                scrollTop = 0;
                scrollBottom = TERM_ROWS - 1;
                show_cursor = true;
                current_inverse = false;
                ansi_state = ANSI_NORMAL;
                return false;
            } else if (c == '(' || c == ')' || c == '*' || c == '+' || c == '#') {
                // Character Set Selection (G0-G3) or DEC Line definitions
                // Consume the next character (the designator)
                ansi_state = ANSI_WAIT_CHAR;
                // Store the trigger char to know what to do in WAIT_CHAR
                ansi_buffer = String(c);
                return false;
            } else {
                // Swallow unknown ESC sequences to cleanly avoid printing garbage
                ansi_state = ANSI_NORMAL;
                return false;
            }
            
        case ANSI_WAIT_CHAR:
            // ansi_buffer contains the trigger char
            if (ansi_buffer[0] == '(') {
                 // SCS - Select Character Set (G0)
                 if (c == '0') {
                     use_line_drawing = true;
                 } else if (c == 'B') {
                     use_line_drawing = false;
                 }
            }
            ansi_state = ANSI_NORMAL;
            return false;

        case ANSI_OSC:
            // Consume until BEL (0x07) or ST (ESC \)
            if (c == '\x07') {
                // End of OSC
                // We could parse ansi_buffer here if we supported color queries
                ansi_state = ANSI_NORMAL;
                return false;
            } else if (c == '\x1B') {
                // Potential ST (String Terminator) start
                // We'll treat ESC as a terminator for simplicity (breaking correct parsing of ESC inside OSC which is rare)
                ansi_state = ANSI_ESC; 
                return false; 
            }
            // Add char to buffer if we wanted to parse it, but for now just ignore to save RAM/Time
            // ansi_buffer += c; 
            return false;
            
        case ANSI_CSI_PARAM:
            if ((c >= '0' && c <= '9') || c == ';' || c == '?' || c == ' ' || c == '>' || c == '<' || c == '=') {
                ansi_buffer += c;
                return false;
            } else {
                ansi_buffer += c;
                handleAnsiCommand(ansi_buffer);
                ansi_state = ANSI_NORMAL;
                ansi_buffer = "";
                return false;
            }
    }
    
    return true;
}

void TerminalEmulator::handleAnsiCommand(const String& cmd) {
    if (cmd.length() == 0) return;
    
    char command = cmd.charAt(cmd.length() - 1);
    String params_str = cmd.substring(0, cmd.length() - 1);
    
    // Check for private mode sequences (starting with ?) or secondary DA (starting with >)
    bool is_private = false;
    bool is_secondary = false;
    
    // Handle ? prefix
    if (params_str.startsWith("?")) {
        is_private = true;
        params_str = params_str.substring(1);
    } else if (params_str.startsWith(">")) {
        is_secondary = true;
        params_str = params_str.substring(1);
    }
    
    // Parse parameters
    int params[16];
    int param_count = 0;
    int start = 0;
    
    for (int i = 0; i <= params_str.length(); i++) {
        if (i == params_str.length() || params_str.charAt(i) == ';') {
            String val = params_str.substring(start, i);
            if (val.length() > 0) {
                 params[param_count++] = val.toInt();
            } else {
                 params[param_count++] = 0;
            }
            start = i + 1;
            if (param_count >= 16) break;
        }
    }
    if (params_str.length() == 0 && param_count == 0) param_count = 0;
    
    #define GET_PARAM(i, def) ((param_count > (i)) ? (params[i] == 0 ? (def) : params[i]) : (def))
    
    switch (command) {
        case 'c': // Device Attributes
            // Used by vim/etc to identify terminal capabilities.
            // Ideally we should respond with built-in escape codes, but for now ignoring is sufficient to prevent "c" on screen.
            break;

        case 'H':  // Cursor position
        case 'f':
            {
                int r = GET_PARAM(0, 1);
                int c = GET_PARAM(1, 1);
                cursor_y = r - 1;
                cursor_x = c - 1;
                if (cursor_y < 0) cursor_y = 0;
                if (cursor_y >= TERM_ROWS) cursor_y = TERM_ROWS - 1;
                if (cursor_x < 0) cursor_x = 0;
                if (cursor_x >= TERM_COLS) cursor_x = TERM_COLS - 1;
                need_display_update = true;
            }
            break;
            
        case 'A':  // Cursor up
            {
                int n = GET_PARAM(0, 1);
                cursor_y -= n;
                if (cursor_y < 0) cursor_y = 0;
                need_display_update = true;
            }
            break;
            
        case 'B':  // Cursor down
            {
                int n = GET_PARAM(0, 1);
                cursor_y += n;
                if (cursor_y >= TERM_ROWS) cursor_y = TERM_ROWS - 1;
                need_display_update = true;
            }
            break;
            
        case 'C':  // Cursor forward
            {
                int n = GET_PARAM(0, 1);
                cursor_x += n;
                if (cursor_x >= TERM_COLS) cursor_x = TERM_COLS - 1;
                need_display_update = true;
            }
            break;
            
        case 'D':  // Cursor back
            {
                int n = GET_PARAM(0, 1);
                cursor_x -= n;
                if (cursor_x < 0) cursor_x = 0;
                need_display_update = true;
            }
            break;
            
        case 'G': // Cursor Horizontal Absolute
             {
                int n = GET_PARAM(0, 1);
                cursor_x = n - 1;
                if (cursor_x < 0) cursor_x = 0;
                if (cursor_x >= TERM_COLS) cursor_x = TERM_COLS - 1;
                need_display_update = true;
             }
             break;
             
        case 'd': // Line Position Absolute
             {
                int n = GET_PARAM(0, 1);
                cursor_y = n - 1;
                if (cursor_y < 0) cursor_y = 0;
                if (cursor_y >= TERM_ROWS) cursor_y = TERM_ROWS - 1;
                need_display_update = true;
             }
             break;
            
        case 'L': // Insert Line
            {
                int n = GET_PARAM(0, 1);
                int bottom = scrollBottom;
                
                if (cursor_y <= bottom && cursor_y >= scrollTop) {
                     for (int i = bottom; i >= cursor_y + n; i--) {
                         memcpy(term_lines[i], term_lines[i - n], TERM_COLS + 1);
                         for(int j=0; j<TERM_COLS; j++) term_attrs[i][j] = term_attrs[i-n][j];
                     }
                     for (int i = cursor_y; i < cursor_y + n && i <= bottom; i++) {
                         term_lines[i][0] = '\0';
                         for(int j=0; j<TERM_COLS; j++) term_attrs[i][j] = {false};
                     }
                }
                need_display_update = true;
            }
            break;
            
        case 'M': // Delete Line
            {
                int n = GET_PARAM(0, 1);
                int bottom = scrollBottom;
                if (cursor_y <= bottom && cursor_y >= scrollTop) {
                     for (int i = cursor_y; i <= bottom - n; i++) {
                         memcpy(term_lines[i], term_lines[i + n], TERM_COLS + 1);
                         for(int j=0; j<TERM_COLS; j++) term_attrs[i][j] = term_attrs[i+n][j];
                     }
                     for (int i = bottom - n + 1; i <= bottom; i++) {
                         term_lines[i][0] = '\0';
                         for (int j = 0; j < TERM_COLS; j++) term_attrs[i][j] = {false};
                     }
                }
                need_display_update = true;
            }
            break;
            

        case '@': // Insert Character
            {
                int n = GET_PARAM(0, 1);
                char* line = term_lines[cursor_y];
                int len = strlen(line);
                
                // If cursor is past current length, pad with spaces first
                while (len < cursor_x) {
                    if (len >= TERM_COLS) break;
                    line[len] = ' ';
                    len++;
                }
                line[len] = '\0';
                
                // Calculate how many chars we can actually shift
                // If we insert n chars at cursor_x, chars from cursor_x to the end
                // move to cursor_x + n.
                // Any chars that fall off TERM_COLS are lost.
                
                if (cursor_x < TERM_COLS) {
                    int chars_to_move = len - cursor_x; 
                    // limit chars_to_move so we don't write past end
                    if (cursor_x + n + chars_to_move > TERM_COLS) {
                        chars_to_move = TERM_COLS - (cursor_x + n);
                    }
                    
                    if (chars_to_move > 0) {
                        memmove(&line[cursor_x + n], &line[cursor_x], chars_to_move);
                    }
                    
                    // Fill the gap with spaces
                    for (int i = 0; i < n; i++) {
                        if (cursor_x + i < TERM_COLS) {
                            line[cursor_x + i] = ' ';
                        }
                    }
                    
                    // Ensure null termination at potentially new length
                    int new_len = len + n;
                    if (new_len > TERM_COLS) new_len = TERM_COLS;
                    line[new_len] = '\0';
                }

                // Shift attributes
                for (int j = TERM_COLS - 1; j >= cursor_x + n; j--) {
                    term_attrs[cursor_y][j] = term_attrs[cursor_y][j - n];
                }
                for (int j = cursor_x; j < cursor_x + n && j < TERM_COLS; j++) {
                    term_attrs[cursor_y][j] = {false};
                }
                need_display_update = true;
            }
            break; 
            
        case 'P': // Delete Character
             {
                int n = GET_PARAM(0, 1);
                char* line = term_lines[cursor_y];
                int len = strlen(line);
                
                if (cursor_x < len) {
                    int remaining = len - cursor_x;
                    if (n > remaining) n = remaining;
                    
                    // Shift left
                    memmove(&line[cursor_x], &line[cursor_x + n], remaining - n);
                    
                    // Terminate the new shorter string
                    line[len - n] = '\0';
                }
                
                // Shift attributes
                for (int j = cursor_x; j < TERM_COLS - n; j++) {
                    term_attrs[cursor_y][j] = term_attrs[cursor_y][j + n];
                }
                for (int j = TERM_COLS - n; j < TERM_COLS; j++) {
                    term_attrs[cursor_y][j] = {false};
                }
                need_display_update = true;
             }
             break;
             
        case 'X': // Erase Character
             {
                 int n = GET_PARAM(0, 1);
                 char* line = term_lines[cursor_y];
                 int len = strlen(line);
                 
                 // Extend line if needed
                 while (len < cursor_x + n) {
                     if (len >= TERM_COLS) break;
                     line[len] = ' ';
                     len++;
                 }
                 line[len] = '\0';
                 
                 for(int i=0; i<n; i++) {
                     if (cursor_x + i < len) line[cursor_x + i] = ' ';
                     if (cursor_x + i < TERM_COLS) term_attrs[cursor_y][cursor_x + i] = {false};
                 }
                 need_display_update = true;
             }
             break;
            
        case 'J':  // Erase display
            {
                int mode = GET_PARAM(0, 0);
                if (mode == 0) {
                    // Clear from cursor to end
                    char* line = term_lines[cursor_y];
                    int len = strlen(line);
                    
                    if (cursor_x < len) {
                        line[cursor_x] = '\0';
                    } else if (cursor_x == 0) {
                        line[0] = '\0';
                    }
                     
                    for (int j = cursor_x; j < TERM_COLS; j++) term_attrs[cursor_y][j] = {false};
                    
                    for (int i = cursor_y + 1; i < TERM_ROWS; i++) {
                        term_lines[i][0] = '\0';
                        for(int j=0; j<TERM_COLS; j++) term_attrs[i][j] = {false};
                    }
                } else if (mode == 1) {
                    // Start of screen to cursor
                    for (int i = 0; i < cursor_y; i++) {
                         term_lines[i][0] = '\0';
                         for(int j=0; j<TERM_COLS; j++) term_attrs[i][j]={false};
                    }
                    
                    char* line = term_lines[cursor_y];
                    int len = strlen(line);
                    // Ensure padding up to cursor
                    while (len <= cursor_x) {
                        if (len >= TERM_COLS) break;
                        line[len] = ' ';
                        len++;
                    }
                    line[len] = '\0';
                    
                    for(int i=0; i<=cursor_x; i++) {
                        if(i < TERM_COLS) line[i] = ' ';
                    }
                    for (int j = 0; j <= cursor_x; j++) term_attrs[cursor_y][j] = {false};
                } else if (mode == 2) {
                    clear();
                }
                need_display_update = true;
            }
            break;
            
        case 'K':  // Erase line
            {
                int mode = GET_PARAM(0, 0);
                if (mode == 0) {
                    // Clear from cursor to end of line
                    char* line = term_lines[cursor_y];
                    int len = strlen(line);
                    if (cursor_x < len) {
                        line[cursor_x] = '\0';
                    } else if (cursor_x == 0) {
                        line[0] = '\0';
                    }
                        
                    for (int j = cursor_x; j < TERM_COLS; j++) term_attrs[cursor_y][j] = {false};
                } else if (mode == 1) {
                    // Clear from start to cursor
                    char* line = term_lines[cursor_y];
                    int len = strlen(line);
                    
                    // Pad if needed
                    while (len <= cursor_x) {
                         if (len >= TERM_COLS) break;
                         line[len] = ' ';
                         len++;
                    }
                    line[len] = '\0';
                    
                    for(int i=0; i<=cursor_x; i++) {
                        if (i < TERM_COLS) line[i] = ' ';
                    }
                    for (int j = 0; j <= cursor_x; j++) term_attrs[cursor_y][j] = {false};
                } else if (mode == 2) {
                    term_lines[cursor_y][0] = '\0';
                    for (int j = 0; j < TERM_COLS; j++) term_attrs[cursor_y][j] = {false};
                }
                need_display_update = true;
            }
            break;
            
        case 'r': // Set Top and Bottom Margins (DECSTBM)
             {
                 int top = GET_PARAM(0, 1);
                 int bot = GET_PARAM(1, TERM_ROWS);
                 if (top < 1) top = 1;
                 if (bot > TERM_ROWS) bot = TERM_ROWS;
                 if (top > bot) top = bot; 
                 
                 scrollTop = top - 1;
                 scrollBottom = bot - 1;
                 cursor_x = 0;
                 cursor_y = 0;
                 need_display_update = true;
             }
             break;
            
        case 'h': // Set Mode
            if (is_private) {
                for (int i=0; i<param_count; i++) {
                     if (params[i] == 1) {
                         application_cursor_mode = true;
                     } else if (params[i] == 25) {
                         show_cursor = true;
                     } else if (params[i] == 47 || params[i] == 1047 || params[i] == 1049) {
                         // Switch to Alternate Screen Buffer
                         if (params[i] == 1049) {
                             saved_cursor_x = cursor_x;
                             saved_cursor_y = cursor_y;
                         }
                         
                         switchBuffer(true);
                         
                         if (params[i] == 1049) {
                             clear();
                         }
                     }
                }
                need_display_update = true;
            }
            break;
            
        case 'l': // Reset Mode
            if (is_private) {
                for (int i=0; i<param_count; i++) {
                     if (params[i] == 1) {
                         application_cursor_mode = false;
                     } else if (params[i] == 25) {
                         show_cursor = false;
                     } else if (params[i] == 47 || params[i] == 1047 || params[i] == 1049) {
                         // Switch to Normal Screen Buffer
                         switchBuffer(false);
                         
                         if (params[i] == 1049) {
                             cursor_x = saved_cursor_x;
                             cursor_y = saved_cursor_y;
                             // Ensure bounds
                             if (cursor_x >= TERM_COLS) cursor_x = TERM_COLS - 1;
                             if (cursor_y >= TERM_ROWS) cursor_y = TERM_ROWS - 1;
                         }
                     }
                }
                need_display_update = true;
            }
            break;
            
        case 'm':  // SGR - Select Graphic Rendition
            if (param_count == 0) {
                current_inverse = false;
            } else {
                for (int i = 0; i < param_count; i++) {
                    if (params[i] == 0) {
                        current_inverse = false;
                    } else if (params[i] == 7) {
                        current_inverse = true;
                    } else if (params[i] == 27) {
                        current_inverse = false;
                    }
                }
            }
            break;
    }
}

void TerminalEmulator::scrollUp() {
    for (int i = scrollTop; i < scrollBottom; i++) {
        memcpy(term_lines[i], term_lines[i + 1], TERM_COLS + 1);
        for (int j = 0; j < TERM_COLS; j++) {
            term_attrs[i][j] = term_attrs[i + 1][j];
        }
    }
    term_lines[scrollBottom][0] = '\0';
    for (int j = 0; j < TERM_COLS; j++) {
        term_attrs[scrollBottom][j].inverse = false;
    }
    need_display_update = true;
}

void TerminalEmulator::scrollDown() {
    for (int i = scrollBottom; i > scrollTop; i--) {
        memcpy(term_lines[i], term_lines[i - 1], TERM_COLS + 1);
        for (int j = 0; j < TERM_COLS; j++) {
            term_attrs[i][j] = term_attrs[i - 1][j];
        }
    }
    term_lines[scrollTop][0] = '\0';
    for (int j = 0; j < TERM_COLS; j++) {
        term_attrs[scrollTop][j].inverse = false;
    }
    need_display_update = true;
}
