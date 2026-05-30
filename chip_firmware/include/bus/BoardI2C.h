#pragma once

#include <stdint.h>

namespace BoardI2C
{
    void begin();
    bool devicePresent(uint8_t address);
    uint8_t scan(uint8_t *addresses, uint8_t capacity);
}
