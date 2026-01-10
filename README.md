# T-Deck Pro SSH Client

Tento projekt implementuje jednoduchý SSH klient pro LilyGO T-Deck Pro využívající:
- E-Paper displej (GxEPD2)
- Klávesnici (TCA8418)
- WiFi
- LibSSH-ESP32

## Konfigurace

Před nahráním je nutné upravit soubor `src/main.cpp` a vyplnit následující údaje:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASS";

const char* SSH_HOST = "192.168.1.xxx";
const char* SSH_USER = "user";
const char* SSH_PASS = "password";
```

## Ovládání

- **Psaní**: Použijte klávesnici pro psaní příkazů.
- **Enter**: Odešle příkaz do vzdáleného terminálu.
- **Alt/Sym**: Přepíná vrstvy znaků (čísla, symboly).
- **Shift**: Velká písmena.

## Poznámky

- E-Paper displej má pomalou obnovovací frekvenci. Klient je navržen jako "Line Mode" terminál, nikoliv pro real-time aplikace jako `top` nebo `vim`.
- Sroluje se automaticky posledních 12 řádků.

## Kompilace a nahrání

Projekt využívá PlatformIO.

1. Otevřete složku v VS Code.
2. Počkejte na inicializaci PlatformIO.
3. Klikněte na tlačítko "Upload" (šipka doprava) na liště PlatformIO.
