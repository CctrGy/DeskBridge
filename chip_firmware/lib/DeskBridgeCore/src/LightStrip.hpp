#pragma once

#include <stdint.h>

#include "light/StripLight.h"

class LightStrip
{
public:
    using Mode = StripLight::Mode;

    void begin()
    {
        StripLight::begin();
    }

    void update()
    {
        StripLight::update();
    }

    bool enabled() const
    {
        return StripLight::enabled();
    }

    void setEnabled(bool value)
    {
        StripLight::setEnabled(value);
    }

    void toggle()
    {
        StripLight::toggleEnabled();
    }

    Mode mode() const
    {
        return StripLight::mode();
    }

    const char *modeName() const
    {
        return StripLight::modeName();
    }

    void setMode(Mode value)
    {
        StripLight::setMode(value);
    }

    void toggleMode()
    {
        StripLight::toggleMode();
    }

    uint16_t brightness() const
    {
        return StripLight::brightness();
    }

    void setBrightness(uint16_t value)
    {
        StripLight::setBrightness(value);
    }

    uint8_t brightnessPercent() const
    {
        return StripLight::brightnessPercent();
    }

    void setBrightnessPercent(uint8_t value)
    {
        StripLight::setBrightnessPercent(value);
    }

    uint16_t kelvinMix() const
    {
        return StripLight::kelvinMix();
    }

    void setKelvinMix(uint16_t value)
    {
        StripLight::setKelvinMix(value);
    }

};
