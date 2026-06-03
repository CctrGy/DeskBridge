#pragma once

#include <stdint.h>

#include "light/NotificationPixels.h"

class LedNotifications
{
public:
    void begin()
    {
        NotificationPixels::begin();
    }

    void clear()
    {
        NotificationPixels::clear();
    }

    void set(uint8_t index, uint8_t red, uint8_t green, uint8_t blue)
    {
        NotificationPixels::set(index, red, green, blue);
    }

    void bootStep(uint8_t step)
    {
        NotificationPixels::showBootStep(step);
    }

    uint8_t brightness() const
    {
        return NotificationPixels::brightness();
    }

    void setBrightness(uint8_t value)
    {
        NotificationPixels::setBrightness(value);
    }

    uint8_t mode() const
    {
        return NotificationPixels::mode();
    }

    void setMode(uint8_t value)
    {
        NotificationPixels::setMode(value);
    }
};
