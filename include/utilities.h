#pragma once

// Pin definitions sourced from T-Deck Pro main repo examples

// I2C 
#define BOARD_I2C_SDA   13
#define BOARD_I2C_SCL   14

// Keypad (TCA8418)
#define KEYPAD_SDA      BOARD_I2C_SDA
#define KEYPAD_SCL      BOARD_I2C_SCL
#define KEYPAD_INT      15
#define TCA8418_DEFAULT_ADDR 0x34

// EPD (E-Paper Display)
#define BOARD_EPD_CS    34
#define BOARD_EPD_DC    35
#define BOARD_EPD_RST   -1
#define BOARD_EPD_BUSY  37
#define BOARD_EPD_SCK   36
#define BOARD_EPD_MOSI  33

// LoRa (SX1262)
#define BOARD_LORA_CS   3
#define BOARD_LORA_RST  4
#define BOARD_LORA_BUSY 6
#define BOARD_LORA_INT  5

// SD Card
#define BOARD_SD_CS     48

// SPI Shared Bus
#define BOARD_SPI_SCK   36
#define BOARD_SPI_MOSI  33
#define BOARD_SPI_MISO  47 

// Touch (CST328)
#define BOARD_TOUCH_INT 12
#define BOARD_TOUCH_RST 45

// Power / Other
#define BOARD_KEYBOARD_LED 42
#define BOARD_POWERON      10
#define BOARD_BAT_ADC      4
#define BOARD_1V8_EN       38
#define BOARD_GPS_EN       39
#define BOARD_6609_EN      41 
