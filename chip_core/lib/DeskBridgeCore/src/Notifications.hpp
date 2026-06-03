#pragma once

#include <stdint.h>

#include "LedNotifications.hpp"

class Notifications
{
public:
    enum class Type : uint8_t
    {
        None,
        BusFault,
        TemperatureRange,
        HumidityRange,
        Co2Range,
        Runtime,
    };

    void begin()
    {
        leds_.begin();
        clear();
    }

    void clear()
    {
        current_ = Type::None;
        leds_.clear();
    }

    void show(Type type)
    {
        current_ = type;
        switch (type)
        {
            case Type::BusFault:
                leds_.set(0, 255, 80, 0);
                break;
            case Type::TemperatureRange:
                leds_.set(1, 255, 0, 0);
                break;
            case Type::HumidityRange:
                leds_.set(1, 0, 0, 255);
                break;
            case Type::Co2Range:
                leds_.set(2, 255, 255, 0);
                break;
            case Type::Runtime:
                leds_.set(2, 80, 0, 255);
                break;
            case Type::None:
            default:
                clear();
                break;
        }
    }

    Type current() const
    {
        return current_;
    }

    LedNotifications &leds()
    {
        return leds_;
    }

private:
    Type current_ = Type::None;
    LedNotifications leds_;
};
