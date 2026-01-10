#pragma once

// Updated for T-Deck Pro (4 Rows, 10 Cols)
#define KEY_ROWS 4
#define KEY_COLS 10

// Key Coordinates (Row, Col) - Based on Meshtastic Firmware (TDeckProKeyboard.cpp)
// Note: T-Deck Pro matrix is mapped right-to-left
// Row 0: p o i u y ... q
// Row 1: BSP l k j ... a
// Row 2: Ent $ m n ... Alt
// Row 3: Shift(R) Sym Space Mic Shift(L)

#define KEY_SHIFT_R_ROW 3
#define KEY_SHIFT_R_COL 0

#define KEY_SYM_ROW 3
#define KEY_SYM_COL 1

#define KEY_SPACE_ROW 3
#define KEY_SPACE_COL 2

#define KEY_MIC_ROW 3
#define KEY_MIC_COL 3

#define KEY_SHIFT_L_ROW 3
#define KEY_SHIFT_L_COL 4

#define KEY_ENTER_ROW 2
#define KEY_ENTER_COL 0

#define KEY_BACKSPACE_ROW 1
#define KEY_BACKSPACE_COL 0

#define KEY_ALT_ROW 2
#define KEY_ALT_COL 9

// Access: keymap[ROW][COL]
extern char keymap_lower[KEY_ROWS][KEY_COLS];
extern char keymap_symbol[KEY_ROWS][KEY_COLS];

