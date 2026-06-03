#pragma once

#include <Arduino.h>
#include <RTClib.h>
#include <stdint.h>

#include "bus/DeskBus.h"

class Bus
{
public:
    using Device = DeskBus::Device;
    using Measurements = DeskBus::Measurements;

    void begin()
    {
        DeskBus::begin();
    }

    void update()
    {
        DeskBus::update();
    }

    bool ready(Device device) const
    {
        return DeskBus::ready(device);
    }

    const char *name(Device device) const
    {
        return DeskBus::name(device);
    }

    const char *addressText(Device device) const
    {
        return DeskBus::addressText(device);
    }

    const Measurements &measurements() const
    {
        return DeskBus::measurements();
    }

    DateTime now() const
    {
        return DeskBus::now();
    }

    bool setRtc(const DateTime &dateTime)
    {
        return DeskBus::setRtc(dateTime);
    }

    bool scanAddress(uint8_t address) const
    {
        return DeskBus::devicePresent(address);
    }

    uint8_t scan(uint8_t *addresses, uint8_t capacity)
    {
        return DeskBus::scan(addresses, capacity);
    }
};
