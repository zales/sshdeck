#pragma once
#include <Arduino.h>

// Global I2C Mutex
extern SemaphoreHandle_t i2cMutex;

// Pin definitions for LilyGo T-Deck Pro (E-Paper Version)
// Source: https://github.com/Xinyuan-LilyGO/T-Deck-Pro/blob/main/examples/factory/utilities.h

// --- Power Management ---
#define BOARD_POWERON       10 
// T-Deck Pro uses I2C PMU (BQ25896 + BQ27220), not a simple ADC pin.
// Setting to -1 to indicate no direct ADC reading.
#define BOARD_BAT_ADC       -1 

// --- Deprecated / Dummy for T-Deck Pro Compatibility ---
// These pins exist on T-Deck TFT but not Pro (or handled by PMU)
#define BOARD_1V8_EN        -1
#define BOARD_GPS_EN        -1
#define BOARD_6609_EN       -1

// --- I2C Bus ---
#define BOARD_I2C_SDA       13
#define BOARD_I2C_SCL       14

// --- SPI Bus ---
#define BOARD_SPI_SCK       36
#define BOARD_SPI_MOSI      33
#define BOARD_SPI_MISO      47

// --- Display (E-Paper) ---
#define BOARD_EPD_CS        34
#define BOARD_EPD_DC        35
#define BOARD_EPD_RST       -1
#define BOARD_EPD_BUSY      37
#define BOARD_EPD_SCK       BOARD_SPI_SCK
#define BOARD_EPD_MOSI      BOARD_SPI_MOSI

// --- LoRa (SX1262) ---
#define BOARD_LORA_CS       3
#define BOARD_LORA_RST      4
#define BOARD_LORA_BUSY     6
#define BOARD_LORA_INT      5   // DIO1
#define BOARD_LORA_MISO     BOARD_SPI_MISO
#define BOARD_LORA_MOSI     BOARD_SPI_MOSI
#define BOARD_LORA_SCK      BOARD_SPI_SCK

// --- SD Card ---
#define BOARD_SD_CS         48
#define BOARD_SD_SCK        BOARD_SPI_SCK
#define BOARD_SD_MOSI       BOARD_SPI_MOSI
#define BOARD_SD_MISO       BOARD_SPI_MISO

// --- Touch ---
#define BOARD_TOUCH_INT     12
#define BOARD_TOUCH_RST     45
#define BOARD_TOUCH_SCL     BOARD_I2C_SCL
#define BOARD_TOUCH_SDA     BOARD_I2C_SDA

// --- Keyboard ---
#define BOARD_KEYBOARD_SCL  BOARD_I2C_SCL
#define BOARD_KEYBOARD_SDA  BOARD_I2C_SDA
#define BOARD_KEYBOARD_INT  15
#define BOARD_KEYBOARD_LED  42

// --- Other Peripherals ---
#define BOARD_GPS_RXD       44
#define BOARD_GPS_TXD       43
#define BOARD_Motor_PIN     2
#define BOARD_VIBRATION     BOARD_Motor_PIN
#define BOARD_BOOT_PIN      0

// --- Addresses ---
#define BOARD_I2C_ADDR_TOUCH      0x1A 
#define BOARD_I2C_ADDR_KEYBOARD   0x34
#define BOARD_I2C_ADDR_BQ27220    0x55 
#define BOARD_I2C_ADDR_BQ25896    0x6B 

