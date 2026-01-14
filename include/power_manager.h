#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "board_def.h"

// BQ27220 (Fuel Gauge)
#define BQ27220_ADDR                    0x55
#define BQ27220_REG_CNTL                0x00
#define BQ27220_REG_TEMP                0x06 // 0.1 K
#define BQ27220_REG_VOLTAGE             0x08 // mV
#define BQ27220_REG_FLAGS               0x0A // Battery Status
#define BQ27220_REG_CURRENT             0x0C // Standard Current
#define BQ27220_REG_RM                  0x10 // Remaining Capacity
#define BQ27220_REG_FCC                 0x12 // Full Charge Capacity
#define BQ27220_REG_AI                  0x14 // Average Current (Signed)
#define BQ27220_REG_TTE                 0x16 // Time to Empty
#define BQ27220_REG_TTF                 0x18 // Time to Full
#define BQ27220_REG_SI                  0x1A // Standby Current
#define BQ27220_REG_SOC                 0x2C // State of Charge (%)
#define BQ27220_REG_CYCLES              0x2A // Cycle Count
#define BQ27220_REG_SOH                 0x2E // State of Health
#define BQ27220_REG_DESIGN_CAP          0x3C // Design Capacity

// BQ25896 (Charger)
#define BQ25896_ADDR       0x6B
#define BQ25896_REG_IINLIM 0x00
#define BQ25896_REG_CONF   0x02
#define BQ25896_REG_ICHG   0x04
#define BQ25896_REG_IPRE   0x05
#define BQ25896_REG_STAT   0x0B
#define BQ25896_REG_FAULT  0x0C
#define BQ25896_REG_VBAT   0x0E
#define BQ25896_REG_VSYS   0x0F
#define BQ25896_REG_TSPCT  0x10
#define BQ25896_REG_VBUS   0x11
#define BQ25896_REG_ICHGR  0x12

struct BatteryStatus {
    float voltage;
    int percentage;
    int currentMa;
    float temperature;
    int remainingCap;
    int fullCap;
    int designCap;
    int cycles;
    int soh;
    bool isCharging;
    bool isPlugged;
    String powerSource; // "USB", "Adapter", "Battery"
    bool vbusGood;
};

class PowerManager {
public:
    void begin(TwoWire& wire);
    
    // Core Status
    float getVoltage();
    int getPercentage();
    bool isCharging();
    bool isPlugged(); // Cable connected?
    
    // Detailed Info
    BatteryStatus getStatus();

private:
    TwoWire* _wire;
    
    uint16_t readFuelGauge16(uint8_t reg);
    uint8_t readCharger8(uint8_t reg);
    void writeCharger8(uint8_t reg, uint8_t val);
};
