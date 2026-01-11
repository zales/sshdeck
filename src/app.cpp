#include "app.h"
#include <WiFi.h>
#include <driver/rtc_io.h>
#include "board_def.h"

// Hardware Constants
#define PIN_BOOT_BUTTON 0

App::App() 
    : wifi(terminal, keyboard, display), ui(display), menu(nullptr), sshClient(nullptr), currentState(STATE_MENU), pwrBtnStart(0) {
}

void App::setup() {
    initializeHardware();
    
    security.begin();
    unlockSystem();

    // Setup WiFi (Scan or Connect)
    wifi.setSecurityManager(&security);
    wifi.connect(); 
    
    serverManager.setSecurityManager(&security);
    serverManager.begin();
    menu = new MenuSystem(display, keyboard);
    // Bind member function via lambda
    menu->setIdleCallback([this]() { this->checkPowerButton(); });
    
    Serial.println("Setup complete, entering menu...");
}

void App::initializeHardware() {
    // Serial debugging
    if (DEBUG_SERIAL_ENABLED) {
        Serial.begin(DEBUG_SERIAL_BAUD);
        delay(100);
        Serial.println("\n\n=================================");
        Serial.println("T-Deck Pro SSH Terminal (Refactored)");
        Serial.println("=================================\n");
    }

    pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);

    // Initialize power pins
    Serial.println("Initializing power...");
    pinMode(BOARD_POWERON, OUTPUT);
    digitalWrite(BOARD_POWERON, HIGH);
    
    pinMode(BOARD_1V8_EN, OUTPUT);
    digitalWrite(BOARD_1V8_EN, HIGH); 
    
    pinMode(BOARD_GPS_EN, OUTPUT);
    digitalWrite(BOARD_GPS_EN, HIGH);
    
    pinMode(BOARD_6609_EN, OUTPUT);
    digitalWrite(BOARD_6609_EN, HIGH);
    
    pinMode(BOARD_KEYBOARD_LED, OUTPUT);
    digitalWrite(BOARD_KEYBOARD_LED, HIGH);

    // Disable SPI devices
    pinMode(BOARD_SD_CS, OUTPUT);
    digitalWrite(BOARD_SD_CS, HIGH);
    pinMode(BOARD_LORA_CS, OUTPUT);
    digitalWrite(BOARD_LORA_CS, HIGH);
    
    delay(1500); // Stabilization
    
    if (!display.begin()) {
        Serial.println("Display init failed!");
        while (1) delay(1000);
    }
    
    // Welcome Screen
    ui.drawBootScreen("T-Deck Pro", "Initializing...");
    
    // terminal.appendString("Init System...\n");
    // drawTerminalScreen();
    
    if (!keyboard.begin()) {
        ui.updateBootStatus("Keyboard FAIL!");
        Serial.println("Keyboard init failed!");
        while (1) delay(1000);
    }
    ui.updateBootStatus("Keyboard OK");
}

void App::loop() {
    keyboard.loop();
    checkPowerButton();

    if (currentState == STATE_MENU) {
        handleMainMenu();
    } else {
        // STATE_TERMINAL
        if (sshClient && sshClient->isConnected()) {
            sshClient->process();
            if (terminal.needsUpdate()) {
                drawTerminalScreen();
            }
            if (!sshClient->isConnected()) {
                currentState = STATE_MENU;
                ui.drawMessage("Disconnected", "Session Ended");
            }
        } else {
            currentState = STATE_MENU;
        }
        delay(5); 
    }
}

void App::handleMainMenu() {
    std::vector<String> items = {
        "Saved Servers",
        "Quick Connect",
        "Settings",
        "Power Off"
    };
    
    int choice = menu->showMenu("Main Menu", items);
    
    if (choice == 0) {
        handleSavedServers();
    } else if (choice == 1) {
        handleQuickConnect();
    } else if (choice == 2) {
        handleSettings();
    } else if (choice == 3) {
        enterDeepSleep();
    }
}

void App::checkPowerButton() {
    if (digitalRead(PIN_BOOT_BUTTON) == LOW) {
        if (pwrBtnStart == 0) {
            pwrBtnStart = millis();
        } else if (millis() - pwrBtnStart > 1000) { 
            enterDeepSleep();
        }
    } else {
        pwrBtnStart = 0;
    }
}

void App::enterDeepSleep() {
    Serial.println("Shutting down...");
    
    ui.drawShutdownScreen();

    delay(1000);

    pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);
    while(digitalRead(PIN_BOOT_BUTTON) == LOW) {
        delay(50);
    }

    display.getDisplay().powerOff();
    
    digitalWrite(BOARD_KEYBOARD_LED, LOW);
    digitalWrite(BOARD_1V8_EN, LOW);
    digitalWrite(BOARD_GPS_EN, LOW);
    digitalWrite(BOARD_6609_EN, LOW);
    digitalWrite(BOARD_POWERON, LOW);

    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BOOT_BUTTON, 0); 
    rtc_gpio_pullup_en((gpio_num_t)PIN_BOOT_BUTTON);
    rtc_gpio_pulldown_dis((gpio_num_t)PIN_BOOT_BUTTON);

    esp_deep_sleep_start();
}

float App::getBatteryVoltage() {
    uint32_t raw = analogRead(BOARD_BAT_ADC);
    return (raw * 3.3 * 2.0) / 4095.0;
}

int App::getBatteryPercentage() {
    float v = getBatteryVoltage();
    if (v < 3.3) return 0;
    if (v > 4.2) return 100;
    return (int)((v - 3.3) / (4.2 - 3.3) * 100.0);
}

void App::drawTerminalScreen() {
    display.setRefreshMode(true);
    display.firstPage();
    do {
        display.clear();
        U8G2_FOR_ADAFRUIT_GFX& u8g2 = display.getFonts();
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.setFontMode(1);
        
        int w = display.getWidth();
        
        // Status Bar
        display.getDisplay().fillRect(0, 0, w, 14, GxEPD_WHITE);
        u8g2.setForegroundColor(GxEPD_BLACK);
        u8g2.setBackgroundColor(GxEPD_WHITE);
        
        // WiFi
        u8g2.setCursor(2, 10);
        u8g2.print("W: ");
        if (WiFi.status() == WL_CONNECTED) {
            String ssid = WiFi.SSID();
            if (ssid.length() > 8) ssid = ssid.substring(0, 8);
            u8g2.print(ssid);
        } else {
            u8g2.print("No Net");
        }
        
        // Host
        if (sshClient && sshClient->isConnected()) {
            u8g2.setCursor(90, 10);
            u8g2.print("S: ");
            String host = sshClient->getConnectedHost();
            if (host.length() > 10) host = host.substring(0, 10);
            u8g2.print(host);
        } else if (WiFi.status() == WL_CONNECTED) {
            u8g2.setCursor(90, 10);
            u8g2.print("Connecting...");
        }

        // Battery
        int bat = getBatteryPercentage();
        int bx = w - 24;
        int by = 3;
        display.getDisplay().drawRect(bx, by, 18, 9, GxEPD_BLACK);
        display.getDisplay().fillRect(bx + 18, by + 2, 2, 5, GxEPD_BLACK);
        
        int bw = (bat * 16) / 100;
        if (bw > 16) bw = 16;
        if (bw < 0) bw = 0;
        if (bw > 0) display.getDisplay().fillRect(bx + 1, by + 1, bw, 7, GxEPD_BLACK);
        
        u8g2.setCursor(bx - 28, 10);
        u8g2.print(String(bat) + "%");
        
        display.getDisplay().drawFastHLine(0, 13, w, GxEPD_BLACK);
        
        // Terminal Content
        int y_start = 24;
        int line_h = 10;
        int char_w = 6;
        
        for (int row = 0; row < TERM_ROWS; row++) {
            const String& line = terminal.getLine(row);
            if (line.length() > 0) {
                for (int col = 0; col < line.length() && col < TERM_COLS; col++) {
                    char ch = line[col];
                    bool inv = terminal.getAttr(row, col).inverse;
                    
                    int x = col * char_w;
                    int y = y_start + (row * line_h);
                    
                    if (inv) {
                        display.fillRect(x, y - 8, char_w, line_h, GxEPD_BLACK);
                        u8g2.setFontMode(1); 
                        u8g2.setForegroundColor(GxEPD_WHITE);
                        u8g2.setBackgroundColor(GxEPD_BLACK);
                    } else {
                        u8g2.setFontMode(1); 
                        u8g2.setForegroundColor(GxEPD_BLACK);
                        u8g2.setBackgroundColor(GxEPD_WHITE);
                    }
                    u8g2.setCursor(x, y);
                    u8g2.print(ch);
                }
            }
        }
        if (terminal.isCursorVisible()) {
            int cx = terminal.getCursorX();
            int cy = terminal.getCursorY();
            if (cx >= 0 && cx < TERM_COLS && cy >= 0 && cy < TERM_ROWS) {
                int c_x_px = cx * char_w;
                int c_y_px = y_start + (cy * line_h);
                char cursorChar = ' ';
                const String& line = terminal.getLine(cy);
                if (cx < line.length()) cursorChar = line[cx];
                
                display.fillRect(c_x_px, c_y_px - 8, char_w, line_h, GxEPD_BLACK);
                u8g2.setFontMode(1);
                u8g2.setForegroundColor(GxEPD_WHITE);
                u8g2.setBackgroundColor(GxEPD_BLACK);
                u8g2.setCursor(c_x_px, c_y_px);
                u8g2.print(cursorChar);
            }
        }    
    } while (display.nextPage());
    terminal.clearUpdateFlag();
}

void App::showHelpScreen() {
    display.getDisplay().firstPage();
    do {
         display.getDisplay().fillScreen(GxEPD_WHITE);
         auto& u8g2 = display.getFonts();
         
         display.getDisplay().fillRect(0, 0, display.getWidth(), 24, GxEPD_BLACK);
         u8g2.setForegroundColor(GxEPD_WHITE);
         u8g2.setBackgroundColor(GxEPD_BLACK);
         u8g2.setFont(u8g2_font_helvB12_tr);
         u8g2.setCursor(5, 18);
         u8g2.print("Help: Shortcuts");
         
         u8g2.setForegroundColor(GxEPD_BLACK);
         u8g2.setBackgroundColor(GxEPD_WHITE);
         u8g2.setFont(u8g2_font_helvR10_tr);
         
         int y = 40; int dy = 16;
         u8g2.setCursor(5, y); u8g2.print("Mic + W/A/S/D : Arrows"); y+=dy;
         u8g2.setCursor(5, y); u8g2.print("Mic + Q       : ESC"); y+=dy;
         u8g2.setCursor(5, y); u8g2.print("Mic + E       : TAB"); y+=dy;
         u8g2.setCursor(5, y); u8g2.print("Alt + 1-9     : F1-F9"); y+=dy;
         u8g2.setCursor(5, y); u8g2.print("Alt + B       : Backlight"); y+=dy;
         u8g2.setCursor(5, y); u8g2.print("Hold Side Btn : Sleep"); y+=dy;
         u8g2.setCursor(5, y); u8g2.print("Mic Key       : Ctrl"); y+=dy;
         y+=4;
         u8g2.setCursor(5, y); u8g2.print("Menu Nav:"); y+=dy;
         u8g2.setCursor(5, y); u8g2.print("Mic + W       : Up"); y+=dy;
         u8g2.setCursor(5, y); u8g2.print("Mic + S       : Down"); y+=dy;
         
         // Footer
         int h = display.getHeight();
         display.fillRect(0, h - 16, display.getWidth(), 16, GxEPD_BLACK);
         u8g2.setForegroundColor(GxEPD_WHITE);
         u8g2.setBackgroundColor(GxEPD_BLACK);
         u8g2.setFont(u8g2_font_profont12_tr); 
         u8g2.setCursor(5, h - 4);
         u8g2.print("Press Key to Close");

    } while (display.getDisplay().nextPage());
    
    while(true) {
        if(keyboard.isKeyPressed()) {
             keyboard.getKeyChar(); 
             break;
        }
        delay(50);
    }
    drawTerminalScreen();
}

void App::connectToServer(const String& host, int port, const String& user, const String& pass, const String& name) {
    terminal.clear();
    terminal.appendString(("Connecting to " + name + "...\n").c_str());
    if (sshClient) delete sshClient;
    sshClient = new SSHClient(terminal, keyboard);
    // Use lambdas for callbacks
    sshClient->setRefreshCallback([this]() { this->drawTerminalScreen(); });
    sshClient->setHelpCallback([this]() { this->showHelpScreen(); });

    if (WiFi.status() != WL_CONNECTED) {
            wifi.setIdleCallback([this]() { this->checkPowerButton(); });
            if (!wifi.connect()) {
                menu->drawMessage("Error", "WiFi Failed");
                return;
            }
    }

    if (sshClient->connectSSH(host.c_str(), port, user.c_str(), pass.c_str())) {
        currentState = STATE_TERMINAL;
    } else {
            menu->drawMessage("Error", "SSH Failed");
    }
}

void App::handleSavedServers() {
    while (true) {
        std::vector<String> items;
        for (const auto& s : serverManager.getServers()) {
            items.push_back(s.name);
        }
        items.push_back("[ Add New Server ]");

        int choice = menu->showMenu("Saved Servers", items);
        
        if (choice < 0) return; // Back
        
        if (choice == items.size() - 1) {
            ServerConfig newSrv;
            String p = "22";
            if (menu->textInput("Name", newSrv.name) && 
                menu->textInput("Host", newSrv.host) &&
                menu->textInput("User", newSrv.user) &&
                menu->textInput("Port", p) &&
                menu->textInput("Password", newSrv.password)) {
                    newSrv.port = p.toInt();
                    serverManager.addServer(newSrv);
            }
        } else {
            ServerConfig selected = serverManager.getServer(choice);
            std::vector<String> actions = {"Connect", "Edit", "Delete"};
            int action = menu->showMenu(selected.name, actions);

            if (action == 0) { // Connect
                connectToServer(selected.host, selected.port, selected.user, selected.password, selected.name);
                return;
            } else if (action == 1) { // Edit
                 String p = String(selected.port);
                 menu->textInput("Name", selected.name);
                 menu->textInput("Host", selected.host);
                 menu->textInput("User", selected.user);
                 menu->textInput("Port", p);
                 selected.port = p.toInt();
                 menu->textInput("Password", selected.password);
                 serverManager.updateServer(choice, selected);
            } else if (action == 2) { // Delete
                 std::vector<String> yn = {"No", "Yes"};
                 if (menu->showMenu("Delete?", yn) == 1) {
                    serverManager.removeServer(choice);
                 }
            }
        }
    }
}

void App::handleQuickConnect() {
     ServerConfig s;
     s.name = "Quick Connect";
     s.port = 22;
     String p = "22";
     
     if (menu->textInput("Host / IP", s.host) &&
         menu->textInput("Port", p) &&
         menu->textInput("User", s.user) &&
         menu->textInput("Password", s.password, true)) {
             s.port = p.toInt();
             connectToServer(s.host, s.port, s.user, s.password, s.name);
     }
}

void App::handleSettings() {
    while(true) {
        std::vector<String> items = {
            "Change PIN",
            "WiFi Network",
            "System Info",
            "Back"
        };
        int choice = menu->showMenu("Settings", items);
        
        if (choice == 0) {
            handleChangePin();
        } 
        else if (choice == 1) {
             wifi.setIdleCallback([this]() { this->checkPowerButton(); });
             wifi.manage();
        } 
        else if (choice == 2) {
            String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "Disconnected";
            String bat = String(getBatteryPercentage()) + "% (" + String(getBatteryVoltage()) + "V)";
            String ram = String(ESP.getFreeHeap() / 1024) + " KB";
            
            ui.drawSystemInfo(ip, bat, ram, WiFi.macAddress());
            
            while(!keyboard.isKeyPressed()) delay(10);
            keyboard.getKeyChar();
        }
        else {
            return;
        }
    }
}

void App::handleChangePin() {
    String newPin = "";
    
    // 1. Enter New PIN
    while (true) {
        ui.drawPinEntry("CHANGE PIN", "Enter New PIN:", newPin);
        while(!keyboard.available()) { delay(10); checkPowerButton(); }
        char key = keyboard.getKeyChar();
        if (key == '\n' || key == '\r') { if (newPin.length() > 0) break; }
        else if (key == 0x08) { if (newPin.length() > 0) newPin.remove(newPin.length()-1); }
        else if (key >= 32 && key <= 126) newPin += key;
    }
    
    // 2. Confirm
    String confirmPin = "";
    while (true) {
        ui.drawPinEntry("CHANGE PIN", "Confirm New PIN:", confirmPin);
        while(!keyboard.available()) { delay(10); checkPowerButton(); }
        char key = keyboard.getKeyChar();
        if (key == '\n' || key == '\r') { if (confirmPin.length() > 0) break; }
        else if (key == 0x08) { if (confirmPin.length() > 0) confirmPin.remove(confirmPin.length()-1); }
        else if (key >= 32 && key <= 126) confirmPin += key;
    }
    
    if (newPin != confirmPin) {
        ui.drawMessage("Error", "PIN Mismatch");
        return;
    }
    
    // 3. Re-Encrypt
    ui.drawMessage("Processing", "Re-encrypting data...");
    security.changePin(newPin);
    serverManager.reEncryptAll();
    wifi.reEncryptAll();
    ui.drawMessage("Success", "PIN Changed!");
}

void App::unlockSystem() {
    String pin = "";
    display.setRefreshMode(true); 

    ui.drawPinEntry("SECURE BOOT", "Enter PIN:", "", false);

    while (true) {
        char key = keyboard.getKeyChar();
        if (key > 0) {
            bool changed = false;
            bool enter = false;
            if (key == '\n' || key == '\r') { if (pin.length() > 0) enter = true; } 
            else if (key == 0x08) { if (pin.length() > 0) { pin.remove(pin.length() - 1); changed = true; } } 
            else if (key >= 32 && key <= 126) { pin += key; changed = true; }
            
            if (enter) {
                 ui.drawMessage("Verifying", "Please wait...");
                 if (security.authenticate(pin)) break; 
                 else { 
                    pin = ""; 
                    ui.drawPinEntry("ACCESS DENIED", "Try Again:", "", true); 
                    delay(500); 
                 }
            } else if (changed) {
                 ui.drawPinEntry("SECURE BOOT", "Enter PIN:", pin, false);
            }
        }
        delay(10);
    }
    
    ui.drawMessage("UNLOCKED", "System Ready");
    delay(1000); 
    display.setRefreshMode(false); 
}
