#include "bus/BoardI2C.h"

#include <Arduino.h>
#include <Wire.h>

#include "config/DeskConfig.h"
#include "config/pins_config.h"

namespace BoardI2C
{
    TwoWire sharedBus(I2C_SYSTEM_SDA_PIN, I2C_SYSTEM_SCL_PIN);

    void begin()
    {
        sharedBus.begin();
        sharedBus.setClock(I2C_CLOCK_HZ_DEFAULT);
    }

    TwoWire &systemWire()
    {
        return sharedBus;
    }

    TwoWire &sensorWire()
    {
        return sharedBus;
    }

    TwoWire &wire(Bus bus)
    {
        (void)bus;
        return sharedBus;
    }

    bool devicePresent(Bus bus, uint8_t address)
    {
        TwoWire &target = wire(bus);
        target.beginTransmission(address);
        return target.endTransmission() == 0;
    }

    uint8_t scan(Bus bus, uint8_t *addresses, uint8_t capacity)
    {
        uint8_t found = 0;

        for (uint8_t address = I2C_SCAN_START_ADDRESS_DEFAULT; address <= I2C_SCAN_END_ADDRESS_DEFAULT; ++address)
        {
            if (devicePresent(bus, address) && found < capacity)
            {
                addresses[found++] = address;
            }
        }
        return found;
    }
}
