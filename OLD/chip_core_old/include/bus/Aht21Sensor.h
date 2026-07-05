#pragma once

#include <Arduino.h>
#include <Wire.h>

class Aht21Sensor
{
public:
    bool begin(TwoWire &wire, uint8_t address = 0x38);
    bool read(float &temperatureC, float &humidity);

private:
    bool writeCommand(uint8_t command);
    bool writeCommand(uint8_t command, uint8_t arg0, uint8_t arg1);
    bool readStatus(uint8_t &status);

    TwoWire *wire_ = nullptr;
    uint8_t address_ = 0x38;
};
