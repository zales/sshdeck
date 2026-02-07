#include "touch_manager.h"

// ─── Init ──────────────────────────────────────────────
bool TouchManager::begin(TwoWire& wire) {
    _panel = new CSE_CST328(240, 320, &wire, BOARD_TOUCH_RST, BOARD_TOUCH_INT);
    
    Serial.println("[Touch] Initializing CSE_CST328 library...");
    
    if (i2cMutex) xSemaphoreTake(i2cMutex, portMAX_DELAY);
    bool ok = _panel->begin();
    
    if (!ok) {
        if (i2cMutex) xSemaphoreGive(i2cMutex);
        Serial.println("[Touch] CSE_CST328 init FAILED");
        delete _panel;
        _panel = nullptr;
        return false;
    }
    
    // Read chip ID (need debug mode)
    _panel->write16(0xD101);  // REG_MODE_DEBUG_INFO
    delay(20);
    _chipId = _panel->readRegister32(0xD204);
    uint32_t fwCheck = _panel->readRegister32(0xD1FC);
    uint32_t fwVer = _panel->readRegister32(0xD208);
    _panel->write16(0xD109);  // REG_MODE_NORMAL
    delay(50);
    if (i2cMutex) xSemaphoreGive(i2cMutex);
    
    Serial.printf("[Touch] Chip ID: 0x%08X, FW check: 0x%08X, FW ver: 0x%08X\n", _chipId, fwCheck, fwVer);
    
    _initialized = true;
    Serial.println("[Touch] CSE_CST328 initialized OK");
    
    // Start background polling task on core 0 (main app runs on core 1)
    xTaskCreatePinnedToCore(pollTask, "touch_poll", 4096, this, 2, &_taskHandle, 0);
    Serial.println("[Touch] Polling task started (core 0, 20ms interval)");
    
    return true;
}

// ─── Diagnostics ───────────────────────────────────────
const char* TouchManager::getModelName() {
    return _initialized ? "CST328" : "NOT FOUND";
}

uint32_t TouchManager::getChipId() {
    return _chipId;
}

// ─── Main-loop API (thread-safe) ──────────────────────
bool TouchManager::available() {
    if (!_initialized) return false;
    return _pendingGesture != GESTURE_NONE || _pendingTouched;
}

TouchEvent TouchManager::read() {
    TouchEvent ev = {false, GESTURE_NONE, 0, 0};
    if (!_initialized) return ev;
    
    portENTER_CRITICAL(&_spinlock);
    ev.gesture = _pendingGesture;
    ev.touched = _pendingTouched;
    ev.x = _pendingX;
    ev.y = _pendingY;
    // Consume the gesture (one-shot)
    _pendingGesture = GESTURE_NONE;
    // Keep touched state — it reflects live state
    portEXIT_CRITICAL(&_spinlock);
    
    return ev;
}

// ─── Background polling task ──────────────────────────
void TouchManager::pollTask(void* param) {
    TouchManager* self = static_cast<TouchManager*>(param);
    self->pollLoop();
}

void TouchManager::pollLoop() {
    unsigned long lastDbg = 0;
    
    for (;;) {
        bool touchActive = false;
        int16_t x = 0, y = 0;
        
        // Read touch from chip (mutex-protected for shared I2C bus)
        if (i2cMutex) xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50));
        if (_panel) {
            touchActive = _panel->isTouched(0);
            if (touchActive) {
                TS_Point pt = _panel->getPoint(0);
                x = pt.x;
                y = pt.y;
            }
        }
        if (i2cMutex) xSemaphoreGive(i2cMutex);
        
        unsigned long now = millis();
        
        // Debug log every 3s
        if (now - lastDbg > 3000) {
            lastDbg = now;
            Serial.printf("[Touch] poll act:%d x:%d y:%d wd:%d\n",
                touchActive, x, y, _wasDown);
        }
        
        if (touchActive) {
            _lastActiveTime = now;
            
            if (!_wasDown) {
                // New touch — record start
                _startX = x;
                _startY = y;
                _lastX = x;
                _lastY = y;
                _startTime = now;
                _wasDown = true;
                Serial.printf("[Touch] DOWN x:%d y:%d\n", x, y);
            } else {
                // Continuing touch — track movement
                _lastX = x;
                _lastY = y;
            }
            
            // Update live touch state
            portENTER_CRITICAL(&_spinlock);
            _pendingTouched = true;
            _pendingX = x;
            _pendingY = y;
            portEXIT_CRITICAL(&_spinlock);
            
        } else if (_wasDown) {
            // Chip says no touch — but debounce: wait RELEASE_DEBOUNCE_MS
            if (now - _lastActiveTime >= RELEASE_DEBOUNCE_MS) {
                // Confirmed release
                _wasDown = false;
                unsigned long duration = _lastActiveTime - _startTime;
                int16_t dx = _lastX - _startX;
                int16_t dy = _lastY - _startY;
                TouchGesture gest = detectGesture(dx, dy, duration);
                
                Serial.printf("[Touch] UP dur:%lums dx:%d dy:%d -> gest:%d\n",
                    duration, dx, dy, gest);
                
                portENTER_CRITICAL(&_spinlock);
                _pendingTouched = false;
                if (gest != GESTURE_NONE) {
                    _pendingGesture = gest;  // Queue for main loop
                }
                portEXIT_CRITICAL(&_spinlock);
            }
            // else: still in debounce window, keep _wasDown=true
            
        } else {
            // No touch, not tracking
            portENTER_CRITICAL(&_spinlock);
            _pendingTouched = false;
            portEXIT_CRITICAL(&_spinlock);
        }
        
        // Timeout safety
        if (_wasDown && (now - _startTime > TOUCH_TIMEOUT_MS)) {
            _wasDown = false;
            int16_t dx = _lastX - _startX;
            int16_t dy = _lastY - _startY;
            unsigned long duration = _lastActiveTime - _startTime;
            TouchGesture gest = detectGesture(dx, dy, duration);
            Serial.printf("[Touch] TIMEOUT dur:%lums dx:%d dy:%d -> gest:%d\n",
                duration, dx, dy, gest);
            
            portENTER_CRITICAL(&_spinlock);
            _pendingTouched = false;
            if (gest != GESTURE_NONE) {
                _pendingGesture = gest;
            }
            portEXIT_CRITICAL(&_spinlock);
        }
        
        vTaskDelay(pdMS_TO_TICKS(20));  // Poll at 50Hz
    }
}

// ─── Gesture detection ────────────────────────────────
TouchGesture TouchManager::detectGesture(int16_t dx, int16_t dy, unsigned long duration) {
    int absDx = abs(dx);
    int absDy = abs(dy);
    int maxDist = max(absDx, absDy);
    
    if (maxDist < SWIPE_MIN_DIST) {
        if (duration < TAP_MAX_MS) return GESTURE_SINGLE_TAP;
        return GESTURE_LONG_PRESS;
    }
    
    if (duration > SWIPE_MAX_MS) return GESTURE_NONE;
    
    if (absDy > absDx) {
        return (dy > 0) ? GESTURE_SWIPE_DOWN : GESTURE_SWIPE_UP;
    } else {
        return (dx > 0) ? GESTURE_SWIPE_RIGHT : GESTURE_SWIPE_LEFT;
    }
}
