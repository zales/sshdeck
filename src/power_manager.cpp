#include "power_manager.h"

void PowerManager::begin(TwoWire& wire) {
    _wire = &wire;
}

uint16_t PowerManager::readFuelGauge16(uint8_t reg) {
    uint16_t val = 0;
    
    // I2C Mutex protection since we share bus
    if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50))) {
        _wire->beginTransmission(BQ27220_ADDR);
        _wire->write(reg);
        // Use STOP condition (true) to prevent stuck bus issues
        if (_wire->endTransmission(true) == 0) {
            if (_wire->requestFrom((uint8_t)BQ27220_ADDR, (uint8_t)2) == 2) {
                val = _wire->read();      // LSB
                val |= (_wire->read() << 8); // MSB
            }
        }
        xSemaphoreGive(i2cMutex);
    }
    return val;
}

uint8_t PowerManager::readCharger8(uint8_t reg) {
    uint8_t val = 0;
    if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50))) {
        _wire->beginTransmission(BQ25896_ADDR);
        _wire->write(reg);
        if (_wire->endTransmission(false) == 0) {
            if (_wire->requestFrom((uint8_t)BQ25896_ADDR, (uint8_t)1) == 1) {
                val = _wire->read();
            }
        }
        xSemaphoreGive(i2cMutex);
    }
    return val;
}

void PowerManager::writeCharger8(uint8_t reg, uint8_t val) {
    if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(50))) {
        _wire->beginTransmission(BQ25896_ADDR);
        _wire->write(reg);
        _wire->write(val);
        _wire->endTransmission();
        xSemaphoreGive(i2cMutex);
    }
}

float PowerManager::getVoltage() {
    // Check if we have BQ27220 (based on platform definition)
    if (BOARD_BAT_ADC < 0) {
        uint16_t mv = readFuelGauge16(BQ27220_REG_VOLTAGE);
        if (mv > 0) return mv / 1000.0f;
    }
    return 0.0f;
}

int PowerManager::getPercentage() {
    if (BOARD_BAT_ADC < 0) {
         uint16_t soc = readFuelGauge16(BQ27220_REG_SOC);
         // Clamp to 0-100: BQ27220 can return >100 during calibration,
         // or 0xFFFF on I2C failure
         if (soc > 100) soc = 100;
         return (int)soc;
    }
    return 0;
}

bool PowerManager::isCharging() {
    if (BOARD_BAT_ADC < 0) {
        uint8_t val = readCharger8(BQ25896_REG_STAT);
        
        // Bits[4:3] CHRG_STAT: 01=Pre-charge, 10=Fast Charging
        uint8_t chrg_stat = (val >> 3) & 0x03;
        if (chrg_stat == 0x01 || chrg_stat == 0x02) return true;
    }
    return false;
}

bool PowerManager::isPlugged() {
    if (BOARD_BAT_ADC < 0) {
        uint8_t val = readCharger8(BQ25896_REG_STAT);
        
        // Bits[7:5] VBUS_STAT
        uint8_t vbus_stat = (val >> 5) & 0x07;
        return (vbus_stat != 0); 
    }
    return false;
}

BatteryStatus PowerManager::getStatus() {
    BatteryStatus s;
    s.voltage = getVoltage();
    s.percentage = getPercentage();
    s.isCharging = isCharging();
    s.isPlugged = isPlugged();
    
    // --- Detailed Fuel Gauge Reads ---
    uint16_t t = readFuelGauge16(BQ27220_REG_TEMP); // 0.1K
    s.temperature = (t * 0.1) - 273.15;
    
    s.currentMa = (int16_t)readFuelGauge16(BQ27220_REG_AI);
    s.remainingCap = readFuelGauge16(BQ27220_REG_RM);
    s.fullCap = readFuelGauge16(BQ27220_REG_FCC);
    s.designCap = readFuelGauge16(BQ27220_REG_DESIGN_CAP);
    s.cycles = readFuelGauge16(BQ27220_REG_CYCLES);
    
    uint16_t sohRaw = readFuelGauge16(BQ27220_REG_SOH);
    s.soh = (sohRaw & 0x00FF);
    
    // --- Detailed Charger Reads ---
    
    // Force a conversion to get fresh VBUS status? 
    // Usually REG0B (STAT) is updated automatically, but VBUS measure might need force.
    // Keeping it simple for now, just reading STAT.
    
    uint8_t stat = readCharger8(BQ25896_REG_STAT);
    uint8_t vbus = readCharger8(BQ25896_REG_VBUS);

    s.vbusGood = (vbus >> 7) & 0x01;
    
    uint8_t vbusStat = (stat >> 5) & 0x07; // VBUS_STAT is bits [7:5] (3 bits)
    if (vbusStat == 0) s.powerSource = "Battery";
    else if (vbusStat == 1) s.powerSource = "USB SDP";
    else if (vbusStat == 2) s.powerSource = "USB CDP";
    else if (vbusStat == 3) s.powerSource = "DCP Adapter";
    else if (vbusStat == 7) s.powerSource = "OTG";
    else s.powerSource = "Adapter";
    
    return s;
}
