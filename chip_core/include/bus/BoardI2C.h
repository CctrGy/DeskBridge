#pragma once

#include <stdint.h>
#include <Wire.h>

namespace BoardI2C
{
    enum class Bus : uint8_t
    {
        System,
        Sensors,
    };

    void begin();
    TwoWire &systemWire();
    TwoWire &sensorWire();
    TwoWire &wire(Bus bus);
    bool devicePresent(Bus bus, uint8_t address);
    uint8_t scan(Bus bus, uint8_t *addresses, uint8_t capacity);
}
