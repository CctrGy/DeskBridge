#include "bus/BoardI2C.h"

#include <Arduino.h>
#include <Wire.h>

#include "config/DeskConfig.h"
#include "config/pins_config.h"

namespace BoardI2C
{
    TwoWire systemBus(I2C_SYSTEM_SDA_PIN, I2C_SYSTEM_SCL_PIN);
    TwoWire sensorBus;

    void begin()
    {
        systemBus.begin();
        systemBus.setClock(I2C_CLOCK_HZ_DEFAULT);

        sensorBus.setSDA(I2C_SENSOR_SDA_PIN_NAME);
        sensorBus.setSCL(I2C_SENSOR_SCL_PIN_NAME);
        sensorBus.begin();
        sensorBus.setClock(I2C_CLOCK_HZ_DEFAULT);
    }

    TwoWire &systemWire()
    {
        return systemBus;
    }

    TwoWire &sensorWire()
    {
        return sensorBus;
    }

    TwoWire &wire(Bus bus)
    {
        return bus == Bus::Sensors ? sensorBus : systemBus;
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
