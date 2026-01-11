#include "keyboard_manager.h"
#include "board_def.h"
#include "keymap.h"
#include <Preferences.h>

KeyboardManager::KeyboardManager() 
    : sym_active(false), shift_active(false), ctrl_active(false), alt_active(false) {
}

bool KeyboardManager::begin() {
    // Setup Backlight
    pinMode(BOARD_KEYBOARD_LED, OUTPUT);
    
    // Restore Backlight Preference
    Preferences prefs;
    prefs.begin("tdeck", true); // Read-only
    bool bl = prefs.getBool("backlight", false); // Default Off
    prefs.end();
    
    digitalWrite(BOARD_KEYBOARD_LED, bl ? HIGH : LOW);

    // Setup Vibration (PWM)
    ledcSetup(0, 2000, 8); // Channel 0, 2kHz, 8-bit
    ledcAttachPin(BOARD_VIBRATION, 0);
    ledcWrite(0, 0);

    // Initialize I2C
    Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
    Wire.setClock(100000);
    
    // Create Vibration Task
    vibeQueue = xQueueCreate(10, sizeof(uint8_t));
    xTaskCreate(
        vibrationTask,    /* Task function. */
        "VibeTask",       /* String with name of task. */
        2048,             /* Stack size in bytes. */
        this,             /* Parameter passed as input of the task */
        1,                /* Priority of the task. */
        NULL);            /* Task handle. */
    
    return initializeKeypad();
}

bool KeyboardManager::initializeKeypad() {
    // Try to initialize with retries
    for (int retry = 0; retry < KEYBOARD_INIT_RETRIES; retry++) {
        if (keypad.begin(TCA8418_DEFAULT_ADDR, &Wire)) {
            keypad.matrix(KEY_ROWS, KEY_COLS);
            pinMode(BOARD_KEYBOARD_INT, INPUT_PULLUP);
            keypad.enableInterrupts();
            // Enable hardware debounce
            keypad.enableDebounce();
            keypad.flush();
            return true;
        }
        delay(KEYBOARD_INIT_RETRY_DELAY);
    }
    
    return false;
}

bool KeyboardManager::isKeyPressed() {
    return (digitalRead(BOARD_KEYBOARD_INT) == LOW && keypad.available() > 0);
}

int KeyboardManager::available() {
    return keypad.available();
}

char KeyboardManager::getKeyChar() {
    if (!available()) {
        return 0;
    }
    
    int key_code = keypad.getEvent();
    if (key_code <= 0) {
        return 0;
    }
    
    // Bit 7 indicates press (1) or release (0)
    // Meshtastic TDeckProKeyboard uses IsPressed = (k & 0x80)
    bool pressed = (key_code & 0x80) != 0;
    
    if (pressed) {
        triggerVibration();
    }

    // Key ID is in the lower 7 bits (1-based index)
    int key = key_code & 0x7F;
    
    // Adjust to 0-based index for array mapping
    // Fix for "shifted" characters (1->0, 2->1, etc.)
    int key_index = key - 1;
    
    int row = key_index / KEY_COLS;
    int col = key_index % KEY_COLS;
    
    // No mirroring needed for standard layout if keymap is correct
    // col = (KEY_COLS - 1) - col;
    
    return processKeyEvent(row, col, pressed);
}

char KeyboardManager::processKeyEvent(int row, int col, bool pressed) {
    if (row < 0 || row >= KEY_ROWS || col < 0 || col >= KEY_COLS) {
        return 0;
    }
    
    // Handle modifiers
    bool is_sym_key = (row == KEY_SYM_ROW && col == KEY_SYM_COL);
    bool is_shift_key = ((row == KEY_SHIFT_L_ROW && col == KEY_SHIFT_L_COL) || 
                         (row == KEY_SHIFT_R_ROW && col == KEY_SHIFT_R_COL));
    bool is_mic_key = (row == KEY_MIC_ROW && col == KEY_MIC_COL);
    bool is_alt_key = (row == KEY_ALT_ROW && col == KEY_ALT_COL);
    
    if (is_sym_key) {
        sym_active = pressed;
        return 0;
    }
    if (is_shift_key) {
        shift_active = pressed;
        return 0;
    }
    if (is_alt_key) {
        alt_active = pressed;
        return 0;
    }
    if (is_mic_key) {
        // Microphone key acts as '0' when SYM is active, otherwise as Ctrl
        if (sym_active && pressed) {
            return keymap_symbol[row][col];
        }
        if (pressed && !ctrl_active) {
            mic_press_time = millis();
        }
        ctrl_active = pressed;
        return 0;
    }
    
    // Only emit on press
    if (!pressed) return 0;
    
    // Check for Alt+B (Toggle Backlight)
    if (alt_active && keymap_lower[row][col] == 'b') {
        toggleBacklight();
        return 0;
    }

    // Special keys
    if (row == KEY_ENTER_ROW && col == KEY_ENTER_COL) return '\n';
    if (row == KEY_BACKSPACE_ROW && col == KEY_BACKSPACE_COL) return 0x08;
    if (row == KEY_SPACE_ROW && col == KEY_SPACE_COL) return ' ';
    
    // Get base character
    char base_c = keymap_lower[row][col];
    
    // Handle symbol layer characters or return base with modifiers
    char c;
    if (sym_active) {
        c = keymap_symbol[row][col];
    } else {
        c = base_c;
        
        // Ctrl combinations
        if (ctrl_active && c >= 'a' && c <= 'z') {
            return c - 'a' + 1;
        }
        
        // Shift for uppercase
        if (shift_active && c >= 'a' && c <= 'z') {
            c -= 32;
        }
    }
    
    return c;
}

void KeyboardManager::triggerVibration() {
    uint8_t dummy = 1;
    // Non-blocking send, if queue full, drop it
    xQueueSend(vibeQueue, &dummy, 0); 
}

void KeyboardManager::vibrationTask(void* parameter) {
    KeyboardManager* km = (KeyboardManager*)parameter;
    uint8_t msg;
    
    for(;;) {
        if (xQueueReceive(km->vibeQueue, &msg, portMAX_DELAY) == pdTRUE) {
            ledcWrite(0, 128); // 50% power
            vTaskDelay(pdMS_TO_TICKS(5)); // 5ms duration
            ledcWrite(0, 0);
            vTaskDelay(pdMS_TO_TICKS(20)); // Cooldown
        }
    }
}

void KeyboardManager::setBacklight(bool on) {
    digitalWrite(BOARD_KEYBOARD_LED, on ? HIGH : LOW);
    
    // Save Preference
    Preferences prefs;
    prefs.begin("tdeck", false); // Read-write
    prefs.putBool("backlight", on);
    prefs.end();
}

void KeyboardManager::toggleBacklight() {
    int state = digitalRead(BOARD_KEYBOARD_LED);
    setBacklight(state == LOW);
}
