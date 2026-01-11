#!/usr/bin/env python3
import time
import sys

def print_center(text, width=40):
    padding = (width - len(text)) // 2
    print(" " * padding + text)

def clear():
    sys.stdout.write("\033[2J\033[H")
    sys.stdout.flush()

def main():
    clear()
    print_center("=== T-Deck Terminal Test ===")
    print("\n")

    # 1. Colors & Attributes
    print("--- Attributes ---")
    print("Normal Text")
    print("\033[1mBold Text\033[0m")
    print("\033[7mInverse Text\033[0m")
    print("\033[1;7mBold Inverse\033[0m")
    
    # 2. Cursor Movement
    print("\n--- Cursor Movement ---")
    print("Corner check (Wait 2s)...")
    sys.stdout.flush()
    time.sleep(1)
    
    # Save cursor
    sys.stdout.write("\0337")
    
    # Move to corners
    sys.stdout.write("\033[1;1H[TL]") # Top Left
    sys.stdout.write("\033[1;36H[TR]") # Top Right (assuming 40 cols)
    sys.stdout.write("\033[30;1H[BL]") # Bottom Left (assuming 30 rows)
    sys.stdout.write("\033[30;36H[BR]") # Bottom Right
    sys.stdout.flush()
    time.sleep(2)
    
    # Restore cursor
    sys.stdout.write("\0338")
    print("Done.")

    # 3. Line Drawing (DEC Special Graphics)
    print("\n--- Line Drawing (Box) ---")
    
    # Enter Line Drawing Mode
    sys.stdout.write("\033(0") 
    
    # Top
    print("l" + "q"*30 + "k")
    # Body
    for i in range(3):
        print("x" + " "*30 + "x")
    # Bottom
    print("m" + "q"*30 + "j")
    
    # Exit Line Drawing Mode
    sys.stdout.write("\033(B")
    
    print("\nTest Complete.")
    print("Press 'q' to exit or Enter to repeat.")

if __name__ == "__main__":
    try:
        main()
        while True:
            k = sys.stdin.read(1)
            if k == 'q': break
            if k == '\n': main()
    except KeyboardInterrupt:
        pass
    finally:
        # Reset attributes
        sys.stdout.write("\033[0m\n")
