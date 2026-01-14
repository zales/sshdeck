# T-Deck Pro SSH Terminal

![Status](https://img.shields.io/badge/Status-Stable-brightgreen)
[![Web Installer](https://img.shields.io/badge/Install-Web_Flasher-blue?style=for-the-badge&logo=googlechrome)](https://zales.github.io/sshdeck/)

A functional SSH client firmware for the **LilyGO T-Deck Pro**. This project transforms the lo-fi handheld into a fully functional remote administration tool with a proper terminal emulator, WiFi management, and persistent configuration.

## Features

*   **Full SSH Client**: Connect to any SSH server using LibSSH-ESP32.
*   **Terminal Emulator**:
    *   Optimized "Line Mode" for E-Paper latency.
    *   VT100/ANSI support (inverse colors, basic cursor).
    *   Scrollable buffer history (managed automatically).
*   **Connection Manager**:
    *   **Bookmarking**: Save unlimited* SSH servers (Host, Port, User, Password).
    *   **Quick Connect**: Type-and-go for one-off connections.
*   **WiFi Manager**:
    *   On-device network scanning and connection.
    *   Saves up to 5 preferred networks with auto-connect.
*   **Hardware Integration**:
    *   **Keyboard**: Full QWERTY support with Shift, Alt/Sym, and Ctrl layers.
    *   **Power Management**: Deep sleep support with zero-power draw (Wake on Power Button).
    *   **Display**: E-Paper (GxEPD2) drivers tuned for partial refresh responsiveness.
*   **System UI**:
    *   Battery meter (Voltage & Percentage).
    *   Status bar (WiFi SSID, Connection State).

## Hardware Requirements

*   **LilyGO T-Deck Pro** (ESP32-S3 + LoRa + 2.0" Keyboard + E-Paper).
*   *Note: This firmware is specifically tuned for the "Pro" version with the extensive keyboard matrix and E-Paper display.*

## Controls & Shortcuts

The keyboard logic is designed to maximize utility despite the limited key count.

| Key / Combo | Action | Note |
| :--- | :--- | :--- |
| **Power Button** (Hold 1s) | **Sleep / Power Off** | Dedicated hardware callback. Wake by pressing again. |
| **Mic Keys Navigation** | **Arrow Keys** | Use `Mic + W/A/S/D` for Arrow Keys navigation. |
| **Mic** (Microphone Icon) | **Control (Ctrl)** | Functions as the `Ctrl` modifier key. |
| **Sym** (Speaker Icon) | **Symbols / Numbers** | Activates the Symbol layer (orange/blue keys). |
| **Shift** (Up Arrow) | **Shift** | Capital letters. |

### Terminal Shortcuts

| Combo | Result | Description |
| :--- | :--- | :--- |
| `Mic` + `C` | `Ctrl+C` | Send Interrupt signal (useful for killing processes). |
| `Mic` + `Q` | `Esc` | Send Escape key (great for Vim). |
| `Mic` + `E` | `Tab` | Send Tab key. |
| `Mic` + `W/S` | `Up/Down` | Scroll command history or navigation. |
| `Alt` + `1`..`9` | `F1`..`F9` | Function keys. |

## Installation

### Method 1: Web Installer (Recommended)
The easiest way to install is via the browser-based flasher. No software required.

1.  Connect T-Deck Pro to your computer via USB.
2.  Open **[zales.github.io/sshdeck](https://zales.github.io/sshdeck/)** in Chrome or Edge.
3.  Click **Install** and select the device port.

### Method 2: PlatformIO (Developers)
This project is built with **PlatformIO**.

1.  **Clone the repository**:
    ```bash
    git clone https://github.com/your-username/tdeck-ssh.git
    cd tdeck-ssh
    ```
2.  **Open in VS Code**:
    Ensure the [PlatformIO](https://platformio.org/) extension is installed.
3.  **Build & Upload**:
    Connect your T-Deck Pro via USB-C.
    Press the **PlatformIO Upload** button (Right Arrow icon) in the bottom bar.
    *Note: The system will auto-detect the port (e.g., `/dev/cu.usbmodem...`).*

## Architecture

The codebase has been refactored (Jan 2026) into a modular, object-oriented structure for maintainability.

*   `App` (Singleton): The central orchestrator. Initializes hardware, manages state (`Menu` vs `Terminal`), and handles the main loop.
*   `WifiManager`: Handles scanning, credentials storage (NVS), and connection logic.
*   `SSHClient`: Wrapper around `libssh_esp32`, handling channel reading/writing.
*   `TerminalEmulator`: Manages the text buffer, ANSI parsing, and cursor position. It does *not* draw to screen directly.
*   `DisplayManager`: Abstraction over `GxEPD2`. Handles full vs partial refreshes and text rendering using `U8g2` fonts.
*   `KeyboardManager`: Driver for the TCA8418 matrix, mapping raw keycodes to ASCII/modifier states.

### Directory Structure
```
include/        # Header files (Interfaces)
src/           # Implementation
  app.cpp      # Main Logic
  ui/          # Menu System
  ...          # Managers
platformio.ini # Dependencies & Build Flags
```

## Troubleshooting

-   **Display Ghosting**: E-Paper displays inherently have ghosting. The system uses partial refreshes for speed. A full refresh happens on major state changes (e.g., entering Sleep or Menu).
-   **WiFi Fails**: Ensure 2.4GHz network. ESP32-S3 does not support 5GHz.
-   **Enter Key**: On some T-Deck units, the generic Enter key code might vary. Check `keymap.cpp` if issues arise.

## License

MIT License. Open source and free to use.
