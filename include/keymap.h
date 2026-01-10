#pragma once

// Updated for T-Deck Pro (4 Rows, 10 Cols)
#define KEY_ROWS 4
#define KEY_COLS 10

// Key Coordinates (Row, Col) - Based on physical test
#define KEY_SYM_ROW 3
#define KEY_SYM_COL 7

#define KEY_SHIFT_L_ROW 3
#define KEY_SHIFT_L_COL 4

#define KEY_SHIFT_R_ROW 3
#define KEY_SHIFT_R_COL 8

#define KEY_ALT_ROW 3
#define KEY_ALT_COL 9

#define KEY_ENTER_ROW 2
#define KEY_ENTER_COL 8

#define KEY_BACKSPACE_ROW 1
#define KEY_BACKSPACE_COL 8

#define KEY_SPACE_ROW 3
#define KEY_SPACE_COL 6

#define KEY_MIC_ROW 3
#define KEY_MIC_COL 5

// Access: keymap[ROW][COL]
char keymap_lower[KEY_ROWS][KEY_COLS] = {
    // Row 0: W E R T Y U I O P [unused]
    {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p' },
    // Row 1: S D F G H J K L BACKSPACE Q
    {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 0x08,},
    // Row 2: Z X C V B N M , ENTER A
    {0, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '\n'},
    // Row 3: [4x unused] SHIFT_L MIC SPACE SYM SHIFT_R ALT
    { 0 ,  0 , ' ',  0 ,  0 }
};

// Symbol Map
char keymap_symbol[KEY_ROWS][KEY_COLS] = {
    // Row 0: 2 3 4 5 6 7 8 9 0 [unused]
    {'2', '3', '4', '5', '6', '7', '8', '9', '0',  0 },
    // Row 1: @ # $ % ^ & * ( ) 1
    {'@', '#', '$', '%', '^', '&', '*', '(', ')', '1'},
    // Row 2: - = [ ] { } \\ . | !
    {'-', '=', '[', ']', '{', '}', '\\', '.', '|', '!'},
    // Row 3: modifiers only
    { 0 ,  0 ,  0 ,  0 ,  0 }
};

