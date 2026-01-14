# SshDeck - T-Deck Pro Firmware Documentation

This document provides a comprehensive technical overview, user manual, and developer guide for the SshDeck firmware running on the LilyGO T-Deck Pro.

## 1. Hardware Architecture

### 1.1 Device Specifications
*   **MCU**: ESP32-S3 (Xtensa Dual-core 32-bit LX7, up to 240 MHz)
*   **Flash**: 16MB (SPI/QSPI)
*   **RAM**: 8MB (Octal PSRAM) + 512KB SRAM
*   **Display**: 2.33" E-Paper (GDEQ031T10 / GxEPD2 driven) or ST7789 TFT (Standard T-Deck)
*   **Radio**: SX1262 LoRa Module (868/915 MHz)
*   **Input**: BlackBerry-style QWERTY Keyboard (TCA8418 I2C Controller)
*   **Power**: AXP2101 / I2C PMU (BQ25896 + BQ27220)

### 1.2 Pinout Configuration (T-Deck Pro)
The firmware is configured specifically for the "Pro" revision. Do not use standard T-Deck pinouts.

| Peripheral | Signal | Pin | Notes |
| :--- | :--- | :--- | :--- |
| **I2C Bus** | SDA | 13 | Shared (Touch, Keyboard, PMU) |
| | SCL | 14 | |
| **SPI Bus** | SCK | 36 | Shared (Display, LoRa, SD) |
| | MOSI | 33 | |
| | MISO | 47 | |
| **Display** | CS | 34 | E-Paper Chip Select |
| | DC | 35 | Data/Command |
| | BUSY | 37 | Busy Status |
| **LoRa** | CS | 3 | Radio Chip Select |
| | RST | 4 | Reset |
| | INT | 5 | Interrupt (DIO1) |
| | BUSY | 6 | |
| **SD Card** | CS | 48 | MicroSD Slot |
| **Input** | KB Int | 15 | Keyboard Interrupt |
| | KB LED | 42 | Backlight / Status LED |
| | Touch Int| 12 | Touchscreen Interrupt |
| **System** | Motor | 2 | Vibration Motor |
| | Boot | 0 | Boot Button |

## 2. Software Architecture

The project is built on **PlatformIO** using the **Arduino Framework**. It follows an object-oriented design where a central `App` class manages localized subsystems.

### 2.1 Class Structure

*   **`App`**: The main controller. Initializes hardware, manages the main loop, handles global state (Menu vs Terminal), and power management.
*   **`KeyboardManager`**: Wraps the `Adafruit_TCA8418` library. Handles debouncing, layer switching (Alt/Shift/Sym), and key mapping.
*   **`DisplayManager`**: Wraps `GxEPD2`. Manages full vs partial refreshes to optimize for E-Paper latency. Handles font rendering via `U8g2_for_Adafruit_GFX`.
*   **`SSHClient`**: Wrapper around `LibSSH-ESP32`. Manages session lifecycle, authentication, and non-blocking data exchange.
*   **`TerminalEmulator`**: A lightweight VT100-compatible state machine. Parses ANSI escape codes (colors, cursor positioning) and maintains the text buffer.
*   **`WifiManager`**: Handles scanning, connection, and credential persistence.
*   **`SecurityManager`**: Manages encrypted storage of sensitive data (SSH keys, passwords) using NVS.

### 2.2 Dependencies
*   `GxEPD2`: E-Paper display driver.
*   `Adafruit TCA8418`: Keyboard controller driver.
*   `LibSSH-ESP32`: SSH protocol implementation.
*   `U8g2_for_Adafruit_GFX`: High-quality typography.

## 3. User Guide

### 3.1 Keyboard Layout & Layers

The T-Deck keyboard has fewer keys than a standard PC keyboard. Layers are used to access numbers and symbols.

*   **Primary Layer**: Lowercase letters `a-z`, Space, Enter/Return, Backspace.
*   **Shift Layer** (Press `Shift` / Up Arrow): Uppercase letters.
*   **Sym Layer** (Press `Sym` / Speaker Icon):
    *   `Q` -> `1`, `W` -> `2`, `E` -> `3`, etc.
    *   `L` -> `"`, `M` -> `:`, `$` -> `;`
    *   Common symbols map to the key labels printed in orange/blue.
*   **Control Layer** (Press `Mic` / Microphone Icon):
    *   Acts as the `Ctrl` key.
    *   `Mic` + `C` = `Ctrl+C` (Interrupt).
    *   `Mic` + `Z` = `Ctrl+Z` (Suspend).
    *   `Mic` + `D` = `Ctrl+D` (EOF).

### 3.2 Navigation
*   **Mic + WASD**: Arrow keys for navigation and terminal scrolling.

## 4. Operational Modes

### 4.1 Menu Mode
The default startup state.
*   **Quick Connect**: Enter details manually for a one-time session.
*   **Saved Servers**: Browse and connect to stored hosts.
*   **Settings**: Configure WiFi, Font size, etc.
*   **Power Off**: Puts the device into Deep Sleep.

### 4.2 Terminal Mode
Active when connected to an SSH server.
*   **Output**: Renders connection output. Optimized for E-Paper (partial updates).
*   **Input**: Sends keystrokes to remote server.
*   **Disconnect**: `Ctrl+D` usually closes the shell, returning to Menu.

## 5. Development & Modifications

### 5.1 Build System
*   **Platform**: `espressif32`
*   **Board**: `esp32-s3-devkitc-1` (Customized via build flags)
*   **Build Flags**:
    *   `ARDUINO_USB_CDC_ON_BOOT=1`: Enables USB Serial for debugging.
    *   `qio_qspi`: Memory configuration for stability.

### 5.2 Common Tasks
*   **Changing Pin Definitions**: Edit `include/board_def.h`.
*   **Adjusting Keymap**: Edit `src/keymap.cpp`.
*   **Theme/Fonts**: Edit `src/display_manager.cpp`.

### 5.3 Troubleshooting
*   **Display is blank**: Verify `BOARD_EPD_CS` and `BOARD_POWERON` in `board_def.h`. E-Paper displays require specific partial refresh handling.
*   **Keyboard not responding**: Check I2C address (`0x34` vs `0x55`) and Interrupt pin (`15`).
*   **WiFi Fails**: Ensure 2.4GHz network (ESP32 does not support 5GHz).

## 6. SSH Key Management

The firmware supports public key authentication (`pubkey`). Due to the embedded nature of the device, private keys must be prepared and imported securely.

### 6.1 Supported Key Formats
The system **ONLY** supports **RSA** keys in the **PEM (Legacy)** format.
*   **Supported**: `-----BEGIN RSA PRIVATE KEY-----`
*   **Unsupported**: `OPENSSH PRIVATE KEY`, `ECDSA`, `Ed25519`.

**How to convert your key:**
If you have a modern OpenSSH key (starts with `-----BEGIN OPENSSH PRIVATE KEY-----`), you must convert it:
```bash
cp ~/.ssh/id_rsa id_rsa_legacy
ssh-keygen -p -m PEM -f id_rsa_legacy
```
*Note: This will ask for your old passphrase and a new one. The new key will be in the old PEM format.*

### 6.2 Importing the Key
You can import the private key using two methods via the **Storage** menu.

#### Method A: USB Mass Storage (RAM Disk)
1.  Go to `Main Menu > Settings > Storage`.
2.  The device will emulate a small USB drive (labeled `KEY_DISK`).
3.  Connect the T-Deck to your PC via USB-C.
4.  Copy your `id_rsa_legacy` file to the drive.
    *   *Filename does not matter, the system scans for the PEM header.*
5.  On the T-Deck, select **Scan USB Disk**.
6.  If found, the key is encrypted with your PIN and saved to internal storage.
7.  The RAM disk is cleared on reboot; your key is not stored there permanently.

#### Method B: MicroSD Card
1.  Copy your converted key to the **root** of a formatted MicroSD card.
2.  Name the file exactly `id_rsa` (no extension).
3.  Insert the SD card into the T-Deck.
4.  Go to `Main Menu > Settings > Storage > Import from SD`.

### 6.3 Security
*   The private key is **encrypted** using AES-256 before being saved to the internal NVS (Non-Volatile Storage).
*   The encryption key is derived from your **Device PIN**.
*   **Warning**: If you change your PIN, the key is re-encrypted. If you lose your PIN, the key cannot be recovered from the device.

## 7. License & Credits
Based on the LilyGO T-Deck examples and community contributions.
Open Source / MIT License.
