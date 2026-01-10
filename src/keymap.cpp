/**
 * Keyboard Layout Mapping for T-Deck Pro
 * Based on Meshtastic Firmware (TDeckProKeyboard.cpp)
 */

#include "keymap.h"

// Note: The keyboard matrix on T-Deck Pro (TCA8418) seems to be mapped 
// "Right-to-Left" for columns in standard index order (0..9).
// Index 0 is Rightmost, Index 9 is Leftmost (for top rows).

char keymap_lower[KEY_ROWS][KEY_COLS] = {
    // Row 0 (Right-to-Left): p o i u y t r e w q
    {'p', 'o', 'i', 'u', 'y', 't', 'r', 'e', 'w', 'q'},
    
    // Row 1 (Right-to-Left): BSP l k j h g f d s a
    {0x08, 'l', 'k', 'j', 'h', 'g', 'f', 'd', 's', 'a'},
    
    // Row 2 (Right-to-Left): Ent $ m n b v c x z Alt
    // Note: Alt is handled by modifier check, but placed here for reference if needed
    {'\n', '$', 'm', 'n', 'b', 'v', 'c', 'x', 'z', 0}, 

    // Row 3 (Right-to-Left?): Shift(R) Sym Space Mic Shift(L) [Unused...]
    // Indices in Meshtastic: 30(R_Shift), 31(Sym), 32(Space), 33(Mic), 34(L_Shift)
    { 0 ,  0 , ' ',  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 }
};

char keymap_symbol[KEY_ROWS][KEY_COLS] = {
    // Row 0 Symbols: @ + - _ ) ( 3 2 1 #
    {'@', '+', '-', '_', ')', '(', '3', '2', '1', '#'},
    
    // Row 1 Symbols: (BSP) " ' ; : / 6 5 4 *
    { 0 , '"', '\'', ';', ':', '/', '6', '5', '4', '*'},
    
    // Row 2 Symbols: (Ent) ($) . , ! ? 9 8 7 (Alt)
    { 0 ,  0 , '.', ',', '!', '?', '9', '8', '7',  0 },
    
    // Row 3 Symbols: ... Mic maps to '0'
    { 0 ,  0 ,  0 , '0',  0 ,  0 ,  0 ,  0 ,  0 ,  0 }
};
