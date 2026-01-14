#include "keyboard_manager.h"
#include "board_def.h"
#include "keymap.h"
#include <Preferences.h>

// Static handle for ISR
static TaskHandle_t keyboardIntTask = NULL;

void IRAM_ATTR KeyboardManager::isr() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (keyboardIntTask != NULL) {
        vTaskNotifyGiveFromISR(keyboardIntTask, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

KeyboardManager::KeyboardManager() 
    : sym_active(false), shift_active(false), ctrl_active(false), alt_active(false), pwrBtnStart(0) {
}

void KeyboardManager::loop() {
    // Check Hardware Button (Side Button / Boot Button)
    if (digitalRead(BOARD_BOOT_PIN) == LOW) {
        if (pwrBtnStart == 0) {
            pwrBtnStart = millis();
        } 
    } else {
        pwrBtnStart = 0;
    }
}

SystemEvent KeyboardManager::getSystemEvent() {
    // Check if Side Button held for sleep
    if (pwrBtnStart > 0 && millis() - pwrBtnStart > 1000) {
         pwrBtnStart = 0; // Reset to avoid repeated trigger
         return SYS_EVENT_SLEEP;
    }
    return SYS_EVENT_NONE;
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

    // I2C is already initialized in App::initializeHardware
    
    // Create Queues
    hapticQueue = xQueueCreate(10, sizeof(uint8_t));
    inputQueue = xQueueCreate(32, sizeof(KeyEvent)); // Store up to 32 key events

    // Create Haptic Task (Independent of I2C)
    xTaskCreate(hapticTask, "HapticTask", 2048, this, 1, NULL);

    // Initialize Keypad Hardware FIRST
    if (!initializeKeypad()) {
        Serial.println("Keyboard hardware init failed!");
        return false;
    }

    // THEN Create Input Task (avoids race condition on I2C during init)
    xTaskCreate(inputTask, "InputTask", 4096, this, 2, &inputTaskHandle);
    
    // Register Task for ISR
    keyboardIntTask = inputTaskHandle;
    
    // Attach Interrupt
    pinMode(BOARD_KEYBOARD_INT, INPUT_PULLUP);
    attachInterrupt(BOARD_KEYBOARD_INT, isr, FALLING);

    return true;
}

bool KeyboardManager::initializeKeypad() {
    // Wait for E-Ink power spikes to settle
    delay(100);

    // Try to initialize with retries
    for (int retry = 0; retry < KEYBOARD_INIT_RETRIES; retry++) {
        
        if (i2cMutex) xSemaphoreTake(i2cMutex, portMAX_DELAY);
        bool success = keypad.begin(BOARD_I2C_ADDR_KEYBOARD, &Wire);
        if (i2cMutex) xSemaphoreGive(i2cMutex);
        
        if (success) {
            keypad.matrix(KEY_ROWS, KEY_COLS);
            pinMode(BOARD_KEYBOARD_INT, INPUT_PULLUP);
            keypad.enableInterrupts();
            // Enable hardware debounce
            keypad.enableDebounce();
            keypad.flush();
            return true;
        }

        // Bus Recovery attempt if init failed
        if (retry < KEYBOARD_INIT_RETRIES - 1) {
             if (i2cMutex) xSemaphoreTake(i2cMutex, portMAX_DELAY);
             Wire.end();
             delay(10);
             Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL, 100000);
             if (i2cMutex) xSemaphoreGive(i2cMutex);
             delay(20);
        }
        
        delay(KEYBOARD_INIT_RETRY_DELAY);
    }
    
    return false;
}


void KeyboardManager::inputTask(void* parameter) {
    KeyboardManager* km = (KeyboardManager*)parameter;
    
    // We need to take the Mutex if we access I2C
    // BUT checking interrupts is just a GPIO read (DigitalRead)
    // The I2C only happens when we call keypad.getEvent()
    
    for(;;) {
        // Wait for interrupt (blocking) - or timeout to act as failsafe polling (100ms)
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
        
        // Process if INT pin is LOW (Active)
        if (digitalRead(BOARD_KEYBOARD_INT) == LOW) {
            
            // Lock I2C before talking to chip
            if (i2cMutex == NULL || xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
                
                int loopLimit = 10; // Avoid infinite loop
                while (km->keypad.available() > 0 && loopLimit-- > 0) {
                     
                     int key_code = km->keypad.getEvent();
                     if (key_code > 0) {
                        bool pressed = (key_code & 0x80) != 0;
                        int key = key_code & 0x7F;
                        int key_index = key - 1;
                        int row = key_index / KEY_COLS;
                        int col = key_index % KEY_COLS;
                        
                        if (pressed) km->triggerHaptic();
                        
                        // Process modifiers & logic
                        char c = km->processKeyEvent(row, col, pressed);
                        
                        if (c != 0) {
                            KeyEvent evt;
                            evt.key = c;
                            evt.modifiers = 0; 
                            if (km->isShiftActive()) evt.modifiers |= 1;
                            if (km->isCtrlActive()) evt.modifiers |= 2;
                            if (km->isAltActive()) evt.modifiers |= 4;
                            if (km->isSymActive()) evt.modifiers |= 8;
                            
                            xQueueSend(km->inputQueue, &evt, 0); 
                        }
                     }
                }
                
                if (i2cMutex != NULL) xSemaphoreGive(i2cMutex);
            }
        }
        
        // No delay needed if using notify - we wait at top of loop
    }
}

bool KeyboardManager::isKeyPressed() {
     // Check if we have items in the queue
     return uxQueueMessagesWaiting(inputQueue) > 0;
}

int KeyboardManager::available() {
    // Správně vrací velikost fronty místo přímého I2C volání
    return uxQueueMessagesWaiting(inputQueue);
}

char KeyboardManager::getKeyChar() {
    KeyEvent evt;
    if (xQueueReceive(inputQueue, &evt, 0) == pdTRUE) {
        return evt.key;
    }
    return 0;
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

void KeyboardManager::triggerHaptic() {
    uint8_t dummy = 1;
    // Non-blocking send, if queue full, drop it
    xQueueSend(hapticQueue, &dummy, 0); 
}

void KeyboardManager::hapticTask(void* parameter) {
    KeyboardManager* km = (KeyboardManager*)parameter;
    uint8_t msg;
    
    for(;;) {
        if (xQueueReceive(km->hapticQueue, &msg, portMAX_DELAY) == pdTRUE) {
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

void KeyboardManager::clearBuffer(unsigned long durationMs) {
    // Clear Input Queue
    xQueueReset(inputQueue); 
    
    // Original logic cleared hardware buffer by reading it.
    // The task does that now. We just wait a bit to drain pending events.
    if (durationMs > 0) delay(durationMs);
    xQueueReset(inputQueue); 
}
