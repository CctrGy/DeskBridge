#pragma once

#include <stdint.h>

namespace NotificationPixels
{
    void begin();
    void clear();
    void showBootStep(uint8_t step);
    void set(uint8_t index, uint8_t red, uint8_t green, uint8_t blue);
    uint8_t brightness();
    void setBrightness(uint8_t value);
    uint8_t mode();
    void setMode(uint8_t value);
}
