#include "bus/BoardI2C.h"

#include <Arduino.h>
#include <Wire.h>

#include "config/DeskConfig.h"
#include "config/pins_config.h"

namespace BoardI2C
{
    void begin()
    {
        Wire.setSDA(I2C_SDA_PIN);
        Wire.setSCL(I2C_SCL_PIN);
        Wire.begin();
        Wire.setClock(I2C_CLOCK_HZ_DEFAULT);
    }

    bool devicePresent(uint8_t address)
    {
        Wire.beginTransmission(address);
        return Wire.endTransmission() == 0;
    }

    uint8_t scan(uint8_t *addresses, uint8_t capacity)
    {
        uint8_t found = 0;

        for (uint8_t address = I2C_SCAN_START_ADDRESS_DEFAULT; address <= I2C_SCAN_END_ADDRESS_DEFAULT; ++address)
        {
            if (devicePresent(address) && found < capacity)
            {
                addresses[found++] = address;
            }
        }

        return found;
    }
}
